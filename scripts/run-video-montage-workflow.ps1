param(
	[Parameter(Mandatory = $true)]
	[string]$Video,
	[string]$OutputPath = "",
	[string]$MontagePrompt = "strong visual montage",
	[string]$VisionModel = $(if ($env:OFXGGML_VISION_SERVER_MODEL) { $env:OFXGGML_VISION_SERVER_MODEL } else { "" }),
	[string]$VisionServerUrl = $(if ($env:OFXGGML_VISION_SERVER_URL) { $env:OFXGGML_VISION_SERVER_URL } else { "http://127.0.0.1:8080" }),
	[string]$VisionModelPath = $(if ($env:OFXGGML_VISION_MODEL) { $env:OFXGGML_VISION_MODEL } else { "" }),
	[string]$VisionMmprojPath = $(if ($env:OFXGGML_VISION_MMPROJ) { $env:OFXGGML_VISION_MMPROJ } else { "" }),
	[ValidateSet("cuda", "cpu")]
	[string]$VisionBackend = "cuda",
	[int]$SampleCount = 6,
	[int]$MaxOutputSegments = 4,
	[double]$SegmentDurationSeconds = 2.0,
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
	$output = @(& $Executable @Arguments 2>&1 | ForEach-Object { [string]$_ })
	if ($LASTEXITCODE -ne 0) {
		throw "$Label failed with exit code $LASTEXITCODE`n$($output -join [Environment]::NewLine)"
	}
	return @($output)
}

function Format-Seconds {
	param([double]$Value)
	return $Value.ToString("0.######", [Globalization.CultureInfo]::InvariantCulture)
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

if ($SampleCount -lt 2) {
	throw "SampleCount must be at least 2."
}
if ($MaxOutputSegments -lt 1 -or $MaxOutputSegments -gt $SampleCount) {
	throw "MaxOutputSegments must be between 1 and SampleCount."
}
if ($SegmentDurationSeconds -le 0) {
	throw "SegmentDurationSeconds must be greater than zero."
}
$localVision = !$SkipVision -and ![string]::IsNullOrWhiteSpace($VisionModelPath)
$visionGpuLayers = if ($VisionBackend -eq "cpu") { "0" } else { "99" }
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
	$VisionServerUrl = "http://127.0.0.1:8082"
} elseif (!$SkipVision -and [string]::IsNullOrWhiteSpace($VisionModel)) {
	throw "Pass local -VisionModelPath plus -VisionMmprojPath, pass -VisionModel for an external server, or use -SkipVision."
}

$ffmpeg = Resolve-Executable -ExplicitPath $FfmpegExecutable -CommandName "ffmpeg"
$ffprobe = Resolve-Executable -ExplicitPath $FfprobeExecutable -CommandName "ffprobe"
$durationSeconds = Get-VideoDuration -Ffprobe $ffprobe -Path $resolvedVideo

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

$sampleTimes = @()
for ($index = 0; $index -lt $SampleCount; $index++) {
	$sampleTimes += (($index + 0.5) * $durationSeconds / $SampleCount)
}

$plan = [ordered]@{
	Name = "ofxGgmlVideo user-video montage workflow"
	Ready = $true
	InputVideo = $resolvedVideo
	OutputVideo = $resolvedOutput
	DurationSeconds = [Math]::Round($durationSeconds, 6)
	SampleCount = $SampleCount
	MaxOutputSegments = $MaxOutputSegments
	SegmentDurationSeconds = $SegmentDurationSeconds
	MontagePrompt = $MontagePrompt
	ModelBacked = !$SkipVision
	LocalVision = $localVision
	VisionModel = $(if ($SkipVision) { "" } else { $VisionModel })
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
		-Alias $VisionModel `
		-GpuLayers $visionGpuLayers
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
			OriginalIndex = $index
			SourceTimeSeconds = $time
			FramePath = $framePath
			Caption = ""
			RelevanceScore = 0.0
		}
	}

	$modelBacked = !$SkipVision
	$scoringOwner = "ofxGgmlVideo chronological sampling"
	$orderedFrames = @($frameRecords)
	if ($modelBacked) {
		$rankingOutput = @(& $rankingScript `
			-Images @($frameRecords.FramePath) `
			-MontagePrompt $MontagePrompt `
			-VisionModel $VisionModel `
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
		$clipDuration = [Math]::Min($SegmentDurationSeconds, $durationSeconds)
		$maxStart = [Math]::Max(0.0, $durationSeconds - $clipDuration)
		$sourceStart = [Math]::Max(0.0, [Math]::Min($maxStart, [double]$frame.SourceTimeSeconds - ($clipDuration / 2.0)))
		$segments += [pscustomobject]@{
			Index = $index
			OriginalSampleIndex = $frame.OriginalIndex
			SourceTimeSeconds = [Math]::Round([double]$frame.SourceTimeSeconds, 6)
			SourceStartSeconds = [Math]::Round($sourceStart, 6)
			DurationSeconds = [Math]::Round($clipDuration, 6)
			FramePath = [string]$frame.FramePath
			Caption = [string]$frame.Caption
			RelevanceScore = [double]$frame.RelevanceScore
		}
	}

	$filterParts = @()
	$concatInputs = ""
	foreach ($segment in $segments) {
		$start = Format-Seconds ([double]$segment.SourceStartSeconds)
		$length = Format-Seconds ([double]$segment.DurationSeconds)
		$label = "v$($segment.Index)"
		$filterParts += "[0:v]trim=start=$start`:duration=$length,setpts=PTS-STARTPTS,scale=trunc(iw/2)*2:trunc(ih/2)*2[$label]"
		$concatInputs += "[$label]"
		if ($hasSourceAudio) {
			$audioLabel = "a$($segment.Index)"
			$filterParts += "[0:a]atrim=start=$start`:duration=$length,asetpts=PTS-STARTPTS[$audioLabel]"
			$concatInputs += "[$audioLabel]"
		}
	}
	if ($hasSourceAudio) {
		$filterParts += "$concatInputs`concat=n=$($segments.Count):v=1:a=1[outv][outa]"
	} else {
		$filterParts += "$concatInputs`concat=n=$($segments.Count):v=1:a=0[outv]"
	}
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
		ModelBacked = $modelBacked
		InferenceChecked = $modelBacked
		ScoringOwner = $scoringOwner
		VisionModel = $(if ($modelBacked) { $VisionModel } else { "" })
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
