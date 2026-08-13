param(
	[Parameter(Mandatory = $true)]
	[string]$Video,
	[string]$OutputPath = "",
	[string]$MontagePrompt = "strong visual montage",
	[string]$MontageManifestPath = "",
	[string]$VisionModel = $(if ($env:OFXGGML_VISION_SERVER_MODEL) { $env:OFXGGML_VISION_SERVER_MODEL } else { "" }),
	[string]$VisionServerUrl = $(if ($env:OFXGGML_VISION_SERVER_URL) { $env:OFXGGML_VISION_SERVER_URL } else { "http://127.0.0.1:8080" }),
	[string]$VisionModelPath = $(if ($env:OFXGGML_VISION_MODEL) { $env:OFXGGML_VISION_MODEL } else { "" }),
	[string]$VisionMmprojPath = $(if ($env:OFXGGML_VISION_MMPROJ) { $env:OFXGGML_VISION_MMPROJ } else { "" }),
	[ValidateSet("cuda", "cpu")]
	[string]$VisionBackend = "cuda",
	[ValidateRange(1, 3600)]
	[int]$VisionStartupTimeoutSeconds = 120,
	[ValidateSet("uniform", "scene")]
	[string]$SamplingMode = "uniform",
	[int]$SampleCount = 6,
	[int]$MaxOutputSegments = 4,
	[double]$SegmentDurationSeconds = 2.0,
	[ValidateRange(0.01, 1.0)]
	[double]$SceneThreshold = 0.3,
	[ValidateRange(0.0, 3600.0)]
	[double]$SceneMinGapSeconds = 1.0,
	[string]$FfmpegExecutable = "",
	[string]$FfprobeExecutable = "",
	[switch]$SkipVision,
	[switch]$KeepFrames,
	[switch]$DryRun,
	[switch]$Json
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-Executable {
	param([string]$ExplicitPath, [string]$CommandName)
	if (![string]::IsNullOrWhiteSpace($ExplicitPath)) {
		$expanded = [Environment]::ExpandEnvironmentVariables($ExplicitPath)
		if (!(Test-Path -LiteralPath $expanded -PathType Leaf)) {
			throw "$CommandName executable was not found: $expanded"
		}
		return (Resolve-Path -LiteralPath $expanded).Path
	}
	$command = Get-Command $CommandName -ErrorAction SilentlyContinue
	if (!$command) {
		throw "$CommandName was not found. Install FFmpeg or pass -${CommandName}Executable."
	}
	return $command.Source
}

function Invoke-NativeCapture {
	param([string]$Executable, [string[]]$Arguments, [string]$Label)
	$previousErrorAction = $ErrorActionPreference
	try {
		$ErrorActionPreference = "Continue"
		$output = @(& $Executable @Arguments 2>&1 | ForEach-Object { [string]$_ })
		$exitCode = $LASTEXITCODE
	} finally {
		$ErrorActionPreference = $previousErrorAction
	}
	if ($exitCode -ne 0) {
		throw "$Label failed with exit code $exitCode`n$($output -join [Environment]::NewLine)"
	}
	return @($output)
}

function Format-Seconds {
	param([double]$Value)
	return $Value.ToString("0.######", [Globalization.CultureInfo]::InvariantCulture)
}

function Resolve-XfadeTransition {
	param([string]$Kind)
	switch ($Kind.ToLowerInvariant()) {
		"crossfade" { return "fade" }
		"dip" { return "fadeblack" }
		"wipe" { return "wipeleft" }
		default { throw "Unsupported overlapping montage transition '$Kind'. Use cut, crossfade, dip, or wipe." }
	}
}

function Get-VideoDuration {
	param([string]$Ffprobe, [string]$Path)
	$output = Invoke-NativeCapture -Executable $Ffprobe -Label "ffprobe duration" -Arguments @(
		"-v", "error",
		"-show_entries", "format=duration",
		"-of", "default=noprint_wrappers=1:nokey=1",
		$Path
	)
	$value = ($output | Select-Object -First 1).Trim()
	$duration = 0.0
	if (![double]::TryParse($value, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$duration) -or $duration -le 0) {
		throw "Could not determine a positive video duration from ffprobe output: $value"
	}
	return $duration
}

function Select-EvenlySpacedValues {
	param([double[]]$Values, [int]$Count)
	if ($Values.Count -le $Count) {
		return @($Values)
	}
	if ($Count -eq 1) {
		return @($Values[[Math]::Floor(($Values.Count - 1) / 2.0)])
	}
	$selected = @()
	for ($index = 0; $index -lt $Count; $index++) {
		$valueIndex = [int][Math]::Round($index * ($Values.Count - 1) / ($Count - 1.0))
		$selected += [double]$Values[$valueIndex]
	}
	return @($selected)
}

function Get-SceneSamplePlan {
	param(
		[string]$Ffmpeg,
		[string]$Path,
		[double]$DurationSeconds,
		[double]$Threshold,
		[double]$MinGapSeconds,
		[int]$MaxSamples
	)
	$filter = "scale=320:-2,select='gt(scene,$(Format-Seconds $Threshold))',showinfo"
	$output = Invoke-NativeCapture -Executable $Ffmpeg -Label "FFmpeg scene detection" -Arguments @(
		"-hide_banner", "-loglevel", "info", "-i", $Path,
		"-an", "-vf", $filter, "-f", "null", "-"
	)
	$cuts = @()
	foreach ($line in $output) {
		if ($line -notmatch 'pts_time:(?<time>[0-9eE+.-]+)') { continue }
		$time = 0.0
		if (![double]::TryParse($Matches.time, [Globalization.NumberStyles]::Float,
			[Globalization.CultureInfo]::InvariantCulture, [ref]$time)) { continue }
		if ($time -le 0.0 -or $time -ge $DurationSeconds) { continue }
		if ($cuts.Count -eq 0 -or ($time - [double]$cuts[-1]) -ge $MinGapSeconds) {
			$cuts += $time
		}
	}
	if ($cuts.Count -eq 0) {
		return [pscustomobject]@{ Cuts = @(); Times = @() }
	}
	$boundaries = @(0.0) + @($cuts) + @($DurationSeconds)
	$centers = @()
	for ($index = 0; $index -lt $boundaries.Count - 1; $index++) {
		$centers += ([double]$boundaries[$index] +
			(([double]$boundaries[$index + 1] - [double]$boundaries[$index]) / 2.0))
	}
	return [pscustomobject]@{
		Cuts = @($cuts)
		Times = @(Select-EvenlySpacedValues -Values $centers -Count $MaxSamples)
	}
}

function Get-LocalVisionServerAlias {
	param(
		[string]$ModelPath,
		[string]$MmprojPath,
		[string]$Backend
	)
	$signature = "$ModelPath|$MmprojPath|$Backend"
	$sha256 = [Security.Cryptography.SHA256]::Create()
	try {
		$hash = $sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($signature))
	} finally {
		$sha256.Dispose()
	}
	$suffix = -join @($hash[0..5] | ForEach-Object { $_.ToString("x2") })
	return "ofxggml-video-$Backend-$suffix"
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$rankingScript = Join-Path $scriptRoot "run-model-informed-montage-smoke.ps1"
$addonsRoot = Split-Path -Parent $addonRoot
$localVisionLauncher = Join-Path $addonsRoot "ofxGgmlVision\scripts\start-local-vision-server.ps1"
$expandedVideo = [Environment]::ExpandEnvironmentVariables($Video)
if (![System.IO.Path]::IsPathRooted($expandedVideo)) {
	$expandedVideo = Join-Path $addonRoot $expandedVideo
}
if (!(Test-Path -LiteralPath $expandedVideo -PathType Leaf)) {
	throw "Input video was not found: $expandedVideo"
}
$resolvedVideo = (Resolve-Path -LiteralPath $expandedVideo).Path

if ([string]::IsNullOrWhiteSpace($MontageManifestPath) -and $SampleCount -lt 2) {
	throw "SampleCount must be at least 2."
}
if ($MaxOutputSegments -lt 1 -or ([string]::IsNullOrWhiteSpace($MontageManifestPath) -and $MaxOutputSegments -gt $SampleCount)) {
	throw "MaxOutputSegments must be between 1 and SampleCount."
}
if ($SegmentDurationSeconds -le 0) {
	throw "SegmentDurationSeconds must be greater than zero."
}
$VisionBackend = $VisionBackend.ToLowerInvariant()
$localVision = !$SkipVision -and ![string]::IsNullOrWhiteSpace($VisionModelPath)
$visionGpuLayers = if ($VisionBackend -eq "cpu") { "0" } else { "99" }
$localVisionServerAlias = ""
if ($localVision) {
	$expandedVisionModel = [Environment]::ExpandEnvironmentVariables($VisionModelPath)
	$expandedVisionMmproj = [Environment]::ExpandEnvironmentVariables($VisionMmprojPath)
	foreach ($expanded in @($expandedVisionModel, $expandedVisionMmproj)) {
		if (!(Test-Path -LiteralPath $expanded -PathType Leaf)) {
			throw "Local Vision model file was not found: $expanded"
		}
	}
	$VisionModelPath = (Resolve-Path -LiteralPath $expandedVisionModel).Path
	$VisionMmprojPath = (Resolve-Path -LiteralPath $expandedVisionMmproj).Path
	$VisionModel = [System.IO.Path]::GetFileNameWithoutExtension($VisionModelPath)
	$localVisionServerAlias = Get-LocalVisionServerAlias `
		-ModelPath $VisionModelPath `
		-MmprojPath $VisionMmprojPath `
		-Backend $VisionBackend
	$VisionServerUrl = "http://127.0.0.1:8082"
} elseif (!$SkipVision -and [string]::IsNullOrWhiteSpace($VisionModel)) {
	throw "Pass local -VisionModelPath plus -VisionMmprojPath, pass -VisionModel for an external server, or use -SkipVision."
}

$ffmpeg = Resolve-Executable -ExplicitPath $FfmpegExecutable -CommandName "ffmpeg"
$ffprobe = Resolve-Executable -ExplicitPath $FfprobeExecutable -CommandName "ffprobe"
$durationSeconds = Get-VideoDuration -Ffprobe $ffprobe -Path $resolvedVideo
$SamplingMode = $SamplingMode.ToLowerInvariant()
$manifestDriven = ![string]::IsNullOrWhiteSpace($MontageManifestPath)
$resolvedManifestPath = ""
$manifestSegments = @()
$manifestOverlapTransitions = $false
$manifestTransitionKind = "cut"
$manifestTransitionSeconds = 0.0
if ($manifestDriven) {
	$expandedManifestPath = [Environment]::ExpandEnvironmentVariables($MontageManifestPath)
	if (![System.IO.Path]::IsPathRooted($expandedManifestPath)) {
		$expandedManifestPath = Join-Path (Get-Location).Path $expandedManifestPath
	}
	if (!(Test-Path -LiteralPath $expandedManifestPath -PathType Leaf)) {
		throw "Montage manifest was not found: $expandedManifestPath"
	}
	$resolvedManifestPath = (Resolve-Path -LiteralPath $expandedManifestPath).Path
	try {
		$manifest = Get-Content -LiteralPath $resolvedManifestPath -Raw | ConvertFrom-Json
	} catch {
		throw "Montage manifest is not valid JSON: $($_.Exception.Message)"
	}
	if ([string]$manifest.kind -ne "ofxGgmlVideoMontageManifest" -or [int]$manifest.version -ne 1) {
		throw "Montage manifest must use kind 'ofxGgmlVideoMontageManifest' and version 1."
	}
	$manifestOverlapTransitions = [bool]$manifest.options.overlapTransitions
	$manifestTransitionKind = ([string]$manifest.options.transitionKind).ToLowerInvariant()
	$manifestTransitionSeconds = [double]$manifest.options.transitionSeconds
	$rawSegments = @($manifest.segments)
	if ($rawSegments.Count -lt 1) {
		throw "Montage manifest must contain at least one segment."
	}
	$manifestDirectory = Split-Path -Parent $resolvedManifestPath
	foreach ($segment in $rawSegments) {
		$segmentSource = [Environment]::ExpandEnvironmentVariables([string]$segment.sourcePath)
		if (![System.IO.Path]::IsPathRooted($segmentSource)) {
			$segmentSource = Join-Path $manifestDirectory $segmentSource
		}
		if (!(Test-Path -LiteralPath $segmentSource -PathType Leaf)) {
			throw "Montage manifest segment source was not found: $segmentSource"
		}
		$resolvedSegmentSource = (Resolve-Path -LiteralPath $segmentSource).Path
		if (![string]::Equals($resolvedSegmentSource, $resolvedVideo, [StringComparison]::OrdinalIgnoreCase)) {
			throw "This single-input renderer only accepts manifest segments from the selected input video: $resolvedSegmentSource"
		}
		$sourceStart = [double]$segment.sourceStartSeconds
		$clipDuration = [double]$segment.durationSeconds
		if ($sourceStart -lt 0 -or $clipDuration -le 0) {
			throw "Montage manifest segment $($segment.index) must have a non-negative source start and positive duration."
		}
		if (($sourceStart + $clipDuration) -gt ($durationSeconds + 0.02)) {
			throw "Montage manifest segment $($segment.index) exceeds the $([Math]::Round($durationSeconds, 3)) second input video."
		}
		$manifestSegments += [pscustomobject]@{
			OriginalIndex = [int]$segment.index
			SourceStartSeconds = $sourceStart
			DurationSeconds = $clipDuration
			SourceTimeSeconds = $sourceStart + ($clipDuration / 2.0)
			Label = [string]$segment.label
			TransitionInKind = [string]$segment.transitionIn.kind
			TransitionInSeconds = [double]$segment.transitionIn.durationSeconds
			TransitionOutKind = [string]$segment.transitionOut.kind
			TransitionOutSeconds = [double]$segment.transitionOut.durationSeconds
		}
	}
	if (!$PSBoundParameters.ContainsKey("MontagePrompt") -and ![string]::IsNullOrWhiteSpace([string]$manifest.prompt)) {
		$MontagePrompt = [string]$manifest.prompt
	}
	$SampleCount = $manifestSegments.Count
	if (!$PSBoundParameters.ContainsKey("MaxOutputSegments")) {
		$MaxOutputSegments = $manifestSegments.Count
	} else {
		$MaxOutputSegments = [Math]::Min($MaxOutputSegments, $manifestSegments.Count)
	}
}

$plannedTransitionRendering = "hard-cut"
$plannedTransitionCount = 0
if ($manifestDriven -and $manifestOverlapTransitions -and $manifestSegments.Count -gt 1) {
	$plannedTransitions = @($manifestSegments | Select-Object -First $MaxOutputSegments | Select-Object -Skip 1 | Where-Object {
		$_.TransitionInSeconds -gt 0 -and $_.TransitionInKind.ToLowerInvariant() -ne "cut"
	})
	foreach ($plannedTransition in $plannedTransitions) {
		[void](Resolve-XfadeTransition -Kind $plannedTransition.TransitionInKind)
	}
	$plannedTransitionCount = $plannedTransitions.Count
	if ($plannedTransitionCount -gt 0) {
		$plannedTransitionRendering = "ffmpeg-xfade"
	}
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
	$outputDirectory = Split-Path -Parent $resolvedVideo
	$outputName = ([System.IO.Path]::GetFileNameWithoutExtension($resolvedVideo)) + ".montage.mp4"
	$OutputPath = Join-Path $outputDirectory $outputName
} else {
	$OutputPath = [Environment]::ExpandEnvironmentVariables($OutputPath)
	if (![System.IO.Path]::IsPathRooted($OutputPath)) {
		$OutputPath = Join-Path (Get-Location).Path $OutputPath
	}
}
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
if ([string]::Equals($resolvedVideo, $resolvedOutput, [StringComparison]::OrdinalIgnoreCase)) {
	throw "OutputPath must not overwrite the input video."
}

$samplingModeUsed = if ($manifestDriven) { "manifest" } else { $SamplingMode }
$samplingFallbackReason = ""
$sceneCuts = @()
$sceneDetectionExercised = $false
$sampleTimes = @()
if ($manifestDriven) {
	$sampleTimes = @($manifestSegments | ForEach-Object { [double]$_.SourceTimeSeconds })
	if ($SamplingMode -eq "scene" -and $PSBoundParameters.ContainsKey("SamplingMode")) {
		throw "SamplingMode scene cannot be combined with MontageManifestPath because the manifest already owns the source windows."
	}
	$samplingModeUsed = "manifest"
} else {
	if ($SamplingMode -eq "scene") {
		$sceneDetectionExercised = $true
		$scenePlan = Get-SceneSamplePlan -Ffmpeg $ffmpeg -Path $resolvedVideo `
			-DurationSeconds $durationSeconds -Threshold $SceneThreshold `
			-MinGapSeconds $SceneMinGapSeconds -MaxSamples $SampleCount
		$sceneCuts = @($scenePlan.Cuts)
		$sampleTimes = @($scenePlan.Times)
		if ($sampleTimes.Count -eq 0) {
			$samplingModeUsed = "uniform"
			$samplingFallbackReason = "No scene change met the selected threshold; using uniform samples."
		}
	}
	if ($sampleTimes.Count -eq 0) {
		for ($index = 0; $index -lt $SampleCount; $index++) {
			$sampleTimes += (($index + 0.5) * $durationSeconds / $SampleCount)
		}
	}
}

$plan = [ordered]@{
	Name = "ofxGgmlVideo user-video montage workflow"
	Ready = $true
	InputVideo = $resolvedVideo
	OutputVideo = $resolvedOutput
	DurationSeconds = [Math]::Round($durationSeconds, 6)
	ManifestDriven = $manifestDriven
	MontageManifestPath = $resolvedManifestPath
	TransitionRendering = $plannedTransitionRendering
	TransitionCount = $plannedTransitionCount
	SamplingModeRequested = $(if ($manifestDriven) { "manifest" } else { $SamplingMode })
	SamplingModeUsed = $samplingModeUsed
	SamplingFallbackReason = $samplingFallbackReason
	SceneDetectionExercised = $sceneDetectionExercised
	SceneThreshold = $SceneThreshold
	SceneMinGapSeconds = $SceneMinGapSeconds
	SceneCutsSeconds = @($sceneCuts | ForEach-Object { [Math]::Round($_, 6) })
	SampleCount = $sampleTimes.Count
	MaxOutputSegments = $MaxOutputSegments
	SegmentDurationSeconds = $SegmentDurationSeconds
	MontagePrompt = $MontagePrompt
	ModelBacked = !$SkipVision
	LocalVision = $localVision
	VisionModel = $(if ($SkipVision) { "" } else { $VisionModel })
	VisionServerModel = $(if ($localVision) { $localVisionServerAlias } elseif ($SkipVision) { "" } else { $VisionModel })
	VisionModelPath = $(if ($localVision) { $VisionModelPath } else { "" })
	VisionMmprojPath = $(if ($localVision) { $VisionMmprojPath } else { "" })
	VisionServerUrl = $(if ($SkipVision) { "" } else { $VisionServerUrl })
	VisionBackend = $(if ($SkipVision) { "none" } elseif ($localVision) { $VisionBackend } else { "external" })
	VisionGpuLayers = $(if ($localVision) { $visionGpuLayers } else { "" })
	Ffmpeg = $ffmpeg
	Ffprobe = $ffprobe
	SampleTimesSeconds = @($sampleTimes | ForEach-Object { [Math]::Round($_, 6) })
}
if ($DryRun) {
	if ($Json) { $plan | ConvertTo-Json -Depth 6 } else { $plan | Format-List }
	return
}

if ($localVision) {
	if (!(Test-Path -LiteralPath $localVisionLauncher -PathType Leaf)) {
		throw "Local Vision launcher was not found: $localVisionLauncher"
	}
	& $localVisionLauncher `
		-ModelPath $VisionModelPath `
		-MmprojPath $VisionMmprojPath `
		-Alias $localVisionServerAlias `
		-GpuLayers $visionGpuLayers `
		-StartupTimeoutSeconds $VisionStartupTimeoutSeconds
	if (!$?) {
		throw "The local llama.cpp Vision model did not start."
	}
}

$outputDirectory = Split-Path -Parent $resolvedOutput
if (![string]::IsNullOrWhiteSpace($outputDirectory)) {
	[void](New-Item -ItemType Directory -Path $outputDirectory -Force)
}
$workRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("ofxGgmlVideo-montage-" + [guid]::NewGuid().ToString("N"))
$framesRoot = Join-Path $workRoot "frames"
[void](New-Item -ItemType Directory -Path $framesRoot -Force)
$started = Get-Date

try {
	$frameRecords = @()
	for ($index = 0; $index -lt $sampleTimes.Count; $index++) {
		$time = [double]$sampleTimes[$index]
		$plannedSegment = if ($manifestDriven) { $manifestSegments[$index] } else { $null }
		$framePath = Join-Path $framesRoot ("frame-{0:D3}-{1}.png" -f $index, ([Math]::Round($time * 1000.0)))
		[void](Invoke-NativeCapture -Executable $ffmpeg -Label "FFmpeg frame extraction" -Arguments @(
			"-hide_banner", "-loglevel", "error",
			"-ss", (Format-Seconds $time),
			"-i", $resolvedVideo,
			"-frames:v", "1",
			"-vf", "scale='min(1280,iw)':-2",
			"-y", $framePath
		))
		if (!(Test-Path -LiteralPath $framePath -PathType Leaf)) {
			throw "FFmpeg did not create sampled frame: $framePath"
		}
		$frameRecords += [pscustomobject]@{
			OriginalIndex = $(if ($manifestDriven) { $plannedSegment.OriginalIndex } else { $index })
			SourceTimeSeconds = $time
			SourceStartSeconds = $(if ($manifestDriven) { $plannedSegment.SourceStartSeconds } else { 0.0 })
			DurationSeconds = $(if ($manifestDriven) { $plannedSegment.DurationSeconds } else { 0.0 })
			Label = $(if ($manifestDriven) { $plannedSegment.Label } elseif ($samplingModeUsed -eq "scene") { "scene $($index + 1)" } else { "sample $($index + 1)" })
			TransitionInKind = $(if ($manifestDriven) { $plannedSegment.TransitionInKind } else { "cut" })
			TransitionInSeconds = $(if ($manifestDriven) { $plannedSegment.TransitionInSeconds } else { 0.0 })
			TransitionOutKind = $(if ($manifestDriven) { $plannedSegment.TransitionOutKind } else { "cut" })
			TransitionOutSeconds = $(if ($manifestDriven) { $plannedSegment.TransitionOutSeconds } else { 0.0 })
			FramePath = $framePath
			Caption = ""
			RelevanceScore = 0.0
		}
	}

	$modelBacked = !$SkipVision
	$scoringOwner = "ofxGgmlVideo $samplingModeUsed sampling"
	$orderedFrames = @($frameRecords)
	if ($modelBacked) {
		$rankingOutput = @(& $rankingScript `
			-Images @($frameRecords.FramePath) `
			-MontagePrompt $MontagePrompt `
			-VisionModel $(if ($localVision) { $localVisionServerAlias } else { $VisionModel }) `
			-VisionServerUrl $VisionServerUrl `
			-SegmentDurationSeconds $SegmentDurationSeconds `
			-Json 2>&1 | ForEach-Object { [string]$_ })
		if (!$?) {
			throw "Model-informed frame ranking failed:`n$($rankingOutput -join [Environment]::NewLine)"
		}
		$ranking = ($rankingOutput -join [Environment]::NewLine) | ConvertFrom-Json
		if (!$ranking.Passed -or !$ranking.InferenceChecked) {
			throw "Model-informed frame ranking did not report exercised inference."
		}
		$scoringOwner = [string]$ranking.ScoringOwner
		$orderedFrames = @($ranking.Segments | ForEach-Object {
			$rankedPath = [string]$_.SourcePath
			$record = $frameRecords | Where-Object { [string]::Equals($_.FramePath, $rankedPath, [StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
			if (!$record) { throw "Ranked frame was not part of the extracted frame set: $rankedPath" }
			[pscustomobject]@{
				OriginalIndex = $record.OriginalIndex
				SourceTimeSeconds = $record.SourceTimeSeconds
				SourceStartSeconds = $record.SourceStartSeconds
				DurationSeconds = $record.DurationSeconds
				Label = $record.Label
				TransitionInKind = $record.TransitionInKind
				TransitionInSeconds = $record.TransitionInSeconds
				TransitionOutKind = $record.TransitionOutKind
				TransitionOutSeconds = $record.TransitionOutSeconds
				FramePath = $record.FramePath
				Caption = [string]$_.VisionCaption
				RelevanceScore = [double]$_.RelevanceScore
			}
		})
	}

	$selectedFrames = @($orderedFrames | Select-Object -First $MaxOutputSegments)
	$sourceAudioCodec = [string](Invoke-NativeCapture -Executable $ffprobe -Label "ffprobe input audio stream" -Arguments @(
		"-v", "error",
		"-select_streams", "a:0",
		"-show_entries", "stream=codec_name",
		"-of", "default=noprint_wrappers=1:nokey=1",
		$resolvedVideo
	) | Select-Object -First 1)
	$hasSourceAudio = ![string]::IsNullOrWhiteSpace($sourceAudioCodec)
	$segments = @()
	for ($index = 0; $index -lt $selectedFrames.Count; $index++) {
		$frame = $selectedFrames[$index]
		if ($manifestDriven) {
			$clipDuration = [double]$frame.DurationSeconds
			$sourceStart = [double]$frame.SourceStartSeconds
		} else {
			$clipDuration = [Math]::Min($SegmentDurationSeconds, $durationSeconds)
			$maxStart = [Math]::Max(0.0, $durationSeconds - $clipDuration)
			$sourceStart = [Math]::Max(0.0, [Math]::Min($maxStart, [double]$frame.SourceTimeSeconds - ($clipDuration / 2.0)))
		}
		$segments += [pscustomobject]@{
			Index = $index
			OriginalSampleIndex = $frame.OriginalIndex
			Label = [string]$frame.Label
			SourceTimeSeconds = [Math]::Round([double]$frame.SourceTimeSeconds, 6)
			SourceStartSeconds = [Math]::Round($sourceStart, 6)
			DurationSeconds = [Math]::Round($clipDuration, 6)
			FramePath = [string]$frame.FramePath
			Caption = [string]$frame.Caption
			RelevanceScore = [double]$frame.RelevanceScore
			TransitionInKind = [string]$frame.TransitionInKind
			TransitionInSeconds = [double]$frame.TransitionInSeconds
			TransitionOutKind = [string]$frame.TransitionOutKind
			TransitionOutSeconds = [double]$frame.TransitionOutSeconds
			RenderedTransitionInKind = "cut"
			RenderedTransitionInSeconds = 0.0
		}
	}

	$filterParts = @()
	foreach ($segment in $segments) {
		$start = Format-Seconds ([double]$segment.SourceStartSeconds)
		$length = Format-Seconds ([double]$segment.DurationSeconds)
		$label = "v$($segment.Index)"
		$filterParts += "[0:v]trim=start=$start`:duration=$length,setpts=PTS-STARTPTS,settb=AVTB,scale=trunc(iw/2)*2:trunc(ih/2)*2,setsar=1[$label]"
		if ($hasSourceAudio) {
			$audioLabel = "a$($segment.Index)"
			$filterParts += "[0:a]atrim=start=$start`:duration=$length,asetpts=PTS-STARTPTS,aresample=48000[$audioLabel]"
		}
	}

	$renderedTransitions = @()
	if ($segments.Count -eq 1) {
		$filterParts += "[v0]null[outv]"
		if ($hasSourceAudio) {
			$filterParts += "[a0]anull[outa]"
		}
	} else {
		$currentVideoLabel = "v0"
		$currentAudioLabel = "a0"
		$currentDuration = [double]$segments[0].DurationSeconds
		for ($index = 1; $index -lt $segments.Count; $index++) {
			$segment = $segments[$index]
			$previousSegment = $segments[$index - 1]
			$transitionKind = ([string]$segment.TransitionInKind).ToLowerInvariant()
			$requestedTransition = [double]$segment.TransitionInSeconds
			if ($modelBacked) {
				$transitionKind = $manifestTransitionKind
				$requestedTransition = $manifestTransitionSeconds
			}
			$transitionSeconds = 0.0
			if ($manifestDriven -and $manifestOverlapTransitions -and $transitionKind -ne "cut" -and $requestedTransition -gt 0) {
				$transitionSeconds = [Math]::Min($requestedTransition,
					[Math]::Min([double]$previousSegment.DurationSeconds * 0.5, [double]$segment.DurationSeconds * 0.5))
			}

			$outputVideoLabel = if ($index + 1 -eq $segments.Count) { "outv" } else { "vmix$index" }
			$outputAudioLabel = if ($index + 1 -eq $segments.Count) { "outa" } else { "amix$index" }
			if ($transitionSeconds -gt 0) {
				$xfadeTransition = Resolve-XfadeTransition -Kind $transitionKind
				$durationText = Format-Seconds $transitionSeconds
				$offsetText = Format-Seconds ([Math]::Max(0.0, $currentDuration - $transitionSeconds))
				$filterParts += "[$currentVideoLabel][v$index]xfade=transition=$xfadeTransition`:duration=$durationText`:offset=$offsetText[$outputVideoLabel]"
				if ($hasSourceAudio) {
					$filterParts += "[$currentAudioLabel][a$index]acrossfade=d=$durationText`:c1=tri`:c2=tri[$outputAudioLabel]"
				}
				$segment.RenderedTransitionInKind = $transitionKind
				$segment.RenderedTransitionInSeconds = [Math]::Round($transitionSeconds, 6)
				$renderedTransitions += [pscustomobject]@{
					FromSegment = [int]$previousSegment.OriginalSampleIndex
					ToSegment = [int]$segment.OriginalSampleIndex
					Kind = $transitionKind
					DurationSeconds = [Math]::Round($transitionSeconds, 6)
					VideoFilter = $xfadeTransition
					AudioFilter = $(if ($hasSourceAudio) { "acrossfade" } else { "none" })
				}
				$currentDuration += [double]$segment.DurationSeconds - $transitionSeconds
			} else {
				$filterParts += "[$currentVideoLabel][v$index]concat=n=2:v=1:a=0[$outputVideoLabel]"
				if ($hasSourceAudio) {
					$filterParts += "[$currentAudioLabel][a$index]concat=n=2:v=0:a=1[$outputAudioLabel]"
				}
				$currentDuration += [double]$segment.DurationSeconds
			}
			$currentVideoLabel = $outputVideoLabel
			$currentAudioLabel = $outputAudioLabel
		}
	}
	$transitionRendering = if ($renderedTransitions.Count -gt 0) { "ffmpeg-xfade" } else { "hard-cut" }
	$filterComplex = $filterParts -join ";"

	$renderArguments = @(
		"-hide_banner", "-loglevel", "error",
		"-i", $resolvedVideo,
		"-filter_complex", $filterComplex,
		"-map", "[outv]",
		"-c:v", "libx264",
		"-preset", "medium",
		"-crf", "20",
		"-pix_fmt", "yuv420p"
	)
	if ($hasSourceAudio) {
		$renderArguments += @("-map", "[outa]", "-c:a", "aac", "-b:a", "192k")
	} else {
		$renderArguments += "-an"
	}
	$renderArguments += @("-movflags", "+faststart", "-y", $resolvedOutput)
	[void](Invoke-NativeCapture -Executable $ffmpeg -Label "FFmpeg montage render" -Arguments $renderArguments)
	if (!(Test-Path -LiteralPath $resolvedOutput -PathType Leaf) -or (Get-Item -LiteralPath $resolvedOutput).Length -le 0) {
		throw "FFmpeg did not create a non-empty montage video: $resolvedOutput"
	}
	$outputDuration = Get-VideoDuration -Ffprobe $ffprobe -Path $resolvedOutput
	$videoCodec = (Invoke-NativeCapture -Executable $ffprobe -Label "ffprobe output video stream" -Arguments @(
		"-v", "error",
		"-select_streams", "v:0",
		"-show_entries", "stream=codec_name",
		"-of", "default=noprint_wrappers=1:nokey=1",
		$resolvedOutput
	) | Select-Object -First 1).Trim()
	if ([string]::IsNullOrWhiteSpace($videoCodec)) {
		throw "Rendered montage did not contain a video stream."
	}
	$outputAudioCodec = [string](Invoke-NativeCapture -Executable $ffprobe -Label "ffprobe output audio stream" -Arguments @(
		"-v", "error",
		"-select_streams", "a:0",
		"-show_entries", "stream=codec_name",
		"-of", "default=noprint_wrappers=1:nokey=1",
		$resolvedOutput
	) | Select-Object -First 1)
	if ($hasSourceAudio -and [string]::IsNullOrWhiteSpace($outputAudioCodec)) {
		throw "Rendered montage did not preserve the input audio stream."
	}

	$result = [ordered]@{
		Name = "ofxGgmlVideo user-video montage workflow"
		Passed = $true
		InputVideo = $resolvedVideo
		OutputVideo = $resolvedOutput
		OutputBytes = (Get-Item -LiteralPath $resolvedOutput).Length
		OutputDurationSeconds = [Math]::Round($outputDuration, 6)
		OutputVideoCodec = $videoCodec
		OutputAudioCodec = $outputAudioCodec.Trim()
		AudioPreserved = $hasSourceAudio
		ManifestDriven = $manifestDriven
		MontageManifestPath = $resolvedManifestPath
		SamplingModeRequested = $(if ($manifestDriven) { "manifest" } else { $SamplingMode })
		SamplingModeUsed = $samplingModeUsed
		SamplingFallbackReason = $samplingFallbackReason
		SceneDetectionExercised = $sceneDetectionExercised
		SceneThreshold = $SceneThreshold
		SceneMinGapSeconds = $SceneMinGapSeconds
		SceneCutsSeconds = @($sceneCuts | ForEach-Object { [Math]::Round($_, 6) })
		SampleTimesSeconds = @($sampleTimes | ForEach-Object { [Math]::Round($_, 6) })
		TransitionRendering = $transitionRendering
		TransitionCount = $renderedTransitions.Count
		RenderedTransitions = $renderedTransitions
		ModelBacked = $modelBacked
		InferenceChecked = $modelBacked
		ScoringOwner = $scoringOwner
		VisionModel = $(if ($modelBacked) { $VisionModel } else { "" })
		VisionServerModel = $(if ($localVision) { $localVisionServerAlias } elseif ($modelBacked) { $VisionModel } else { "" })
		VisionBackend = $(if (!$modelBacked) { "none" } elseif ($localVision) { $VisionBackend } else { "external" })
		VisionGpuLayers = $(if ($localVision) { $visionGpuLayers } else { "" })
		SegmentCount = $segments.Count
		Segments = $segments
		FramesDirectory = $(if ($KeepFrames) { $framesRoot } else { "" })
		ElapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
	}
	if ($Json) { $result | ConvertTo-Json -Depth 8 } else { $result | Format-List }
} finally {
	if (!$KeepFrames -and (Test-Path -LiteralPath $workRoot -PathType Container)) {
		Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
	}
}
