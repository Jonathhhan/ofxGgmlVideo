param(
	[Parameter(Mandatory = $true)]
	[string[]] $Images,
	[Parameter(Mandatory = $true)]
	[string] $MontagePrompt,
	[string] $VisionModel = $(if ($env:OFXGGML_VISION_SERVER_MODEL) { $env:OFXGGML_VISION_SERVER_MODEL } else { "" }),
	[string] $VisionServerUrl = $(if ($env:OFXGGML_VISION_SERVER_URL) { $env:OFXGGML_VISION_SERVER_URL } else { "http://127.0.0.1:11434" }),
	[double] $SegmentDurationSeconds = 2.0,
	[switch] $DryRun,
	[switch] $Json
)

$ErrorActionPreference = "Stop"

function Get-MeaningfulTokens {
	param([string] $Text)
	$stopWords = @("a", "an", "and", "as", "at", "be", "by", "for", "from", "in", "is", "it", "of", "on", "or", "the", "this", "to", "with")
	return @([regex]::Matches($Text.ToLowerInvariant(), "[a-z0-9]+") |
		ForEach-Object { $_.Value } |
		Where-Object { $_.Length -gt 1 -and $_ -notin $stopWords } |
		Select-Object -Unique)
}

function Get-ScoringTokens {
	param([string] $Text)
	$tokens = @(Get-MeaningfulTokens -Text $Text)
	$genericVisualWords = @("frame", "frames", "image", "images", "montage", "most", "picture", "pictures", "prefer", "preferred", "visual", "visually")
	$specific = @($tokens | Where-Object { $_ -notin $genericVisualWords })
	return $(if ($specific.Count -gt 0) { $specific } else { $tokens })
}

function Get-MatchedPromptTokens {
	param(
		[string[]] $PromptTokens,
		[string[]] $CaptionTokens
	)
	return @($PromptTokens | Where-Object {
		$promptToken = $_
		$CaptionTokens | Where-Object {
			$_ -eq $promptToken -or
			($_.Length -ge 5 -and $promptToken.Length -ge 5 -and $_.Substring(0, 4) -eq $promptToken.Substring(0, 4))
		} | Select-Object -First 1
	})
}

function ConvertFrom-SmokeJson {
	param([object[]] $Output)
	$text = ($Output | ForEach-Object { $_.ToString() }) -join "`n"
	try { return $text | ConvertFrom-Json } catch { throw "Vision smoke did not return valid JSON: $text" }
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$videoRoot = Resolve-Path (Join-Path $scriptRoot "..")
$addonsRoot = Split-Path -Parent $videoRoot
$visionScript = Join-Path $addonsRoot "ofxGgmlVision\scripts\run-vision-server-smoke.ps1"
if (-not (Test-Path -LiteralPath $visionScript -PathType Leaf)) {
	throw "Vision server smoke was not found: $visionScript"
}
if ([string]::IsNullOrWhiteSpace($VisionModel)) {
	throw "Pass -VisionModel or set OFXGGML_VISION_SERVER_MODEL."
}
if ([string]::IsNullOrWhiteSpace($MontagePrompt)) {
	throw "MontagePrompt must not be empty."
}
if ($SegmentDurationSeconds -le 0) {
	throw "SegmentDurationSeconds must be greater than zero."
}

$resolvedImages = @($Images | ForEach-Object {
	$path = [Environment]::ExpandEnvironmentVariables($_)
	if (-not [System.IO.Path]::IsPathRooted($path)) { $path = Join-Path $videoRoot $path }
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Montage frame was not found: $path" }
	(Resolve-Path -LiteralPath $path).Path
})
if ($resolvedImages.Count -lt 2) {
	throw "Pass at least two montage frames."
}

if ($DryRun) {
	$plan = [ordered]@{
		Name = "ofxGgmlVideo model-informed montage smoke"
		Ready = $true
		ModelBacked = $true
		InferenceChecked = $false
		ScoringOwner = "ofxGgmlVision"
		DecisionOwner = "ofxGgmlVideo deterministic relevance ranking"
		MontagePrompt = $MontagePrompt
		Images = $resolvedImages
	}
	if ($Json) { $plan | ConvertTo-Json -Depth 5 } else { $plan | Format-List }
	return
}

$goalTokens = @(Get-ScoringTokens -Text $MontagePrompt)
if ($goalTokens.Count -eq 0) { throw "MontagePrompt did not contain meaningful scoring tokens." }
$started = Get-Date
$candidates = @()
for ($index = 0; $index -lt $resolvedImages.Count; $index++) {
	$image = $resolvedImages[$index]
	$vision = $null
	$attemptUsed = 0
	for ($attempt = 1; $attempt -le 2; $attempt++) {
		$attemptUsed = $attempt
		$output = & $visionScript `
			-ServerUrl $VisionServerUrl `
			-Model $VisionModel `
			-Image $image `
			-Prompt "Describe the visible subject, predominant colors, mood, and composition. Use concrete words." `
			-MaxTokens 128 `
			-Json `
			-SummaryOnly 2>&1
		$succeeded = $?
		$vision = ConvertFrom-SmokeJson -Output $output
		if ($succeeded -and $vision.Passed -and -not [string]::IsNullOrWhiteSpace([string] $vision.Text)) { break }
	}
	if (-not $vision.Passed -or [string]::IsNullOrWhiteSpace([string] $vision.Text)) {
		throw "Vision did not describe montage frame after $attemptUsed attempts: $image"
	}
	$captionTokens = @(Get-MeaningfulTokens -Text ([string] $vision.Text))
	$matches = @(Get-MatchedPromptTokens -PromptTokens $goalTokens -CaptionTokens $captionTokens)
	$score = [Math]::Round($matches.Count / [double]$goalTokens.Count, 6)
	$candidates += [pscustomobject]@{
		OriginalIndex = $index
		SourcePath = $image
		Caption = ([string] $vision.Text).Trim()
		MatchedTokens = $matches
		Score = $score
		VisionAttempts = $attemptUsed
		VisionElapsedMs = [int] $vision.ElapsedMs
	}
}

$ordered = @($candidates | Sort-Object @{ Expression = "Score"; Descending = $true }, @{ Expression = "OriginalIndex"; Descending = $false })
$segments = @()
for ($index = 0; $index -lt $ordered.Count; $index++) {
	$item = $ordered[$index]
	$start = $index * $SegmentDurationSeconds
	$segments += [ordered]@{
		Index = $index
		SourcePath = $item.SourcePath
		TimelineStartSeconds = $start
		TimelineEndSeconds = $start + $SegmentDurationSeconds
		DurationSeconds = $SegmentDurationSeconds
		VisionCaption = $item.Caption
		RelevanceScore = $item.Score
		MatchedPromptTokens = $item.MatchedTokens
	}
}

$summary = [ordered]@{
	Name = "ofxGgmlVideo model-informed montage smoke"
	Passed = $true
	ModelBacked = $true
	InferenceChecked = $true
	SmokeKind = "model-informed-vision-ranked-montage"
	ScoringOwner = "ofxGgmlVision"
	DecisionOwner = "ofxGgmlVideo deterministic relevance ranking"
	VisionModel = $VisionModel
	MontagePrompt = $MontagePrompt
	PromptTokens = $goalTokens
	SegmentCount = $segments.Count
	DurationSeconds = $segments.Count * $SegmentDurationSeconds
	Segments = $segments
	ElapsedMs = [int] ((Get-Date) - $started).TotalMilliseconds
}

if ($Json) {
	$summary | ConvertTo-Json -Depth 7
} else {
	Write-Host "ofxGgmlVideo model-informed montage smoke passed"
	foreach ($segment in $segments) {
		Write-Host "[$($segment.Index)] score=$($segment.RelevanceScore) $($segment.SourcePath)"
	}
}
