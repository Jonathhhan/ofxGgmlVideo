$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$workflowScript = Join-Path $scriptRoot "run-video-montage-workflow.ps1"
$rankingScript = Join-Path $scriptRoot "run-model-informed-montage-smoke.ps1"
$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
$ffprobe = Get-Command ffprobe -ErrorAction SilentlyContinue
if (!$ffmpeg -or !$ffprobe) {
	Write-Host "==> FFmpeg workflow test skipped because ffmpeg/ffprobe are unavailable"
	return
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("ofxGgmlVideo-workflow-test-" + [guid]::NewGuid().ToString("N"))
$inputVideo = Join-Path $testRoot "input.mp4"
$outputVideo = Join-Path $testRoot "output.mp4"
$sceneOutputVideo = Join-Path $testRoot "scene-output.mp4"
$manifestOutputVideo = Join-Path $testRoot "manifest-output.mp4"
$manifestPath = Join-Path $testRoot "montage-manifest.json"
$visionModel = Join-Path $testRoot "vision-model.gguf"
$visionMmproj = Join-Path $testRoot "mmproj-vision.gguf"
[void](New-Item -ItemType Directory -Path $testRoot -Force)

try {
	$generateOutput = @(& $ffmpeg.Source `
		-hide_banner -loglevel error `
		-f lavfi -i "color=c=red:size=320x180:rate=24:duration=1" `
		-f lavfi -i "color=c=blue:size=320x180:rate=24:duration=1" `
		-f lavfi -i "color=c=green:size=320x180:rate=24:duration=1" `
		-f lavfi -i "color=c=yellow:size=320x180:rate=24:duration=1" `
		-f lavfi -i "sine=frequency=440:sample_rate=48000" `
		-filter_complex "[0:v][1:v][2:v][3:v]concat=n=4:v=1:a=0[outv]" `
		-map "[outv]" -map 4:a -t 4 -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest -y $inputVideo 2>&1)
	if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $inputVideo -PathType Leaf)) {
		throw "Failed to generate workflow test video: $($generateOutput -join [Environment]::NewLine)"
	}
	$manifest = [ordered]@{
		kind = "ofxGgmlVideoMontageManifest"
		version = 1
		prompt = "use the exact planned clip windows"
		durationSeconds = 1.25
		options = [ordered]@{
			transitionKind = "crossfade"
			transitionSeconds = 0.25
			handleSeconds = 0.0
			beatBpm = 0.0
			beatsPerBar = 4
			overlapTransitions = $true
		}
		references = @()
		markers = @()
		segments = @(
			[ordered]@{
				index = 7
				sourcePath = $inputVideo
				label = "short opening"
				sourceStartSeconds = 0.25
				sourceEndSeconds = 0.75
				timelineStartSeconds = 0.0
				timelineEndSeconds = 0.5
				durationSeconds = 0.5
				handleInSeconds = 0.0
				handleOutSeconds = 0.0
				transitionIn = @{ kind = "cut"; durationSeconds = 0.0 }
				transitionOut = @{ kind = "crossfade"; durationSeconds = 0.25 }
				tags = @("planned")
				references = @()
				frameSamples = @()
			},
			[ordered]@{
				index = 9
				sourcePath = $inputVideo
				label = "long closing"
				sourceStartSeconds = 2.0
				sourceEndSeconds = 3.0
				timelineStartSeconds = 0.25
				timelineEndSeconds = 1.25
				durationSeconds = 1.0
				handleInSeconds = 0.0
				handleOutSeconds = 0.0
				transitionIn = @{ kind = "crossfade"; durationSeconds = 0.25 }
				transitionOut = @{ kind = "cut"; durationSeconds = 0.0 }
				tags = @("planned")
				references = @()
				frameSamples = @()
			}
		)
	}
	$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

	$manifestDryRunOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $manifestOutputVideo `
		-MontageManifestPath $manifestPath `
		-SkipVision `
		-DryRun `
		-Json)
	$manifestDryRun = ($manifestDryRunOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$manifestDryRun.Ready -or !$manifestDryRun.ManifestDriven -or
		$manifestDryRun.SampleCount -ne 2 -or $manifestDryRun.MaxOutputSegments -ne 2 -or
		$manifestDryRun.TransitionRendering -ne "ffmpeg-xfade" -or $manifestDryRun.TransitionCount -ne 1 -or
		[Math]::Abs([double]$manifestDryRun.SampleTimesSeconds[0] - 0.5) -gt 0.001 -or
		[Math]::Abs([double]$manifestDryRun.SampleTimesSeconds[1] - 2.5) -gt 0.001) {
		throw "Video montage workflow did not plan the exact manifest clip windows."
	}

	$dryRunOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $outputVideo `
		-SkipVision `
		-SampleCount 3 `
		-MaxOutputSegments 2 `
		-SegmentDurationSeconds 0.75 `
		-DryRun `
		-Json)
	$dryRun = ($dryRunOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$dryRun.Ready -or $dryRun.ModelBacked -or $dryRun.SampleTimesSeconds.Count -ne 3) {
		throw "Video montage workflow dry-run did not report the expected deterministic plan."
	}

	$sceneDryRunOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $sceneOutputVideo `
		-SkipVision `
		-SamplingMode scene `
		-SceneThreshold 0.1 `
		-SceneMinGapSeconds 0.5 `
		-SampleCount 4 `
		-MaxOutputSegments 3 `
		-DryRun `
		-Json)
	$sceneDryRun = ($sceneDryRunOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$sceneDryRun.Ready -or !$sceneDryRun.SceneDetectionExercised -or
		$sceneDryRun.SamplingModeRequested -ne "scene" -or $sceneDryRun.SamplingModeUsed -ne "scene" -or
		$sceneDryRun.SceneCutsSeconds.Count -ne 3 -or $sceneDryRun.SampleTimesSeconds.Count -ne 4 -or
		![string]::IsNullOrWhiteSpace([string]$sceneDryRun.SamplingFallbackReason)) {
		throw "Video montage workflow did not derive scene-centered samples from real FFmpeg cut detection."
	}

	$sceneFallbackOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $sceneOutputVideo `
		-SkipVision `
		-SamplingMode scene `
		-SceneThreshold 1.0 `
		-SampleCount 3 `
		-MaxOutputSegments 2 `
		-DryRun `
		-Json)
	$sceneFallback = ($sceneFallbackOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$sceneFallback.SceneDetectionExercised -or $sceneFallback.SamplingModeUsed -ne "uniform" -or
		$sceneFallback.SampleTimesSeconds.Count -ne 3 -or
		[string]::IsNullOrWhiteSpace([string]$sceneFallback.SamplingFallbackReason)) {
		throw "Video montage workflow did not expose its no-cut uniform fallback."
	}

	[void](New-Item -ItemType File -Path $visionModel)
	[void](New-Item -ItemType File -Path $visionMmproj)
	$testVisionModel = $visionModel
	$testVisionMmproj = $visionMmproj
	$null = . $rankingScript `
		-Images @($visionModel, $visionMmproj) `
		-MontagePrompt "Prefer the image with the most visual detail and varied structure." `
		-VisionModel "dry-run-model" `
		-DryRun `
		-Json
	$scoringTokens = @(Get-ScoringTokens -Text "Prefer the image with the most visual detail and varied structure.")
	if (($scoringTokens -join ",") -ne "detail,varied,structure") {
		throw "Model-informed ranking did not remove generic visual prompt words."
	}
	$sceneScoringTokens = @(Get-ScoringTokens -Text "Prefer the bright yellow scene.")
	if (($sceneScoringTokens -join ",") -ne "bright,yellow") {
		throw "Model-informed ranking treated generic scene language as visual evidence."
	}
	$relatedTokens = @(Get-MatchedPromptTokens `
		-PromptTokens @("detail", "varied", "structure") `
		-CaptionTokens @("detailed", "various", "structural"))
	if (($relatedTokens -join ",") -ne "detail,varied,structure") {
		throw "Model-informed ranking did not match related word forms."
	}
	$visionModel = $testVisionModel
	$visionMmproj = $testVisionMmproj
	$localVisionOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $outputVideo `
		-VisionModelPath $visionModel `
		-VisionMmprojPath $visionMmproj `
		-VisionBackend cpu `
		-SampleCount 3 `
		-MaxOutputSegments 2 `
		-DryRun `
		-Json)
	$localVisionPlan = ($localVisionOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$localVisionPlan.Ready -or !$localVisionPlan.ModelBacked -or !$localVisionPlan.LocalVision -or
		$localVisionPlan.VisionModel -ne "vision-model" -or $localVisionPlan.VisionServerUrl -ne "http://127.0.0.1:8082" -or
		$localVisionPlan.VisionBackend -ne "cpu" -or [string]$localVisionPlan.VisionGpuLayers -ne "0" -or
		$localVisionPlan.VisionServerModel -notmatch '^ofxggml-video-cpu-[0-9a-f]{12}$') {
		throw "Video montage workflow dry-run did not preserve the local Vision model handoff."
	}

	$localCudaOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $outputVideo `
		-VisionModelPath $visionModel `
		-VisionMmprojPath $visionMmproj `
		-VisionBackend cuda `
		-DryRun `
		-Json)
	$localCudaPlan = ($localCudaOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if ($localCudaPlan.VisionBackend -ne "cuda" -or [string]$localCudaPlan.VisionGpuLayers -ne "99") {
		throw "Video montage workflow dry-run did not preserve the CUDA backend selection."
	}
	if ($localCudaPlan.VisionServerModel -notmatch '^ofxggml-video-cuda-[0-9a-f]{12}$' -or
		$localCudaPlan.VisionServerModel -eq $localVisionPlan.VisionServerModel) {
		throw "Video montage workflow did not bind the local server identity to its backend configuration."
	}

	$renderOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $outputVideo `
		-SkipVision `
		-SampleCount 3 `
		-MaxOutputSegments 2 `
		-SegmentDurationSeconds 0.75 `
		-Json)
	$render = ($renderOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$render.Passed -or $render.ModelBacked -or $render.InferenceChecked) {
		throw "Video montage workflow did not report a passing deterministic render."
	}
	if ($render.SegmentCount -ne 2 -or $render.OutputDurationSeconds -lt 1.4 -or $render.OutputDurationSeconds -gt 1.6) {
		throw "Video montage workflow returned unexpected segment or duration evidence."
	}
	if (!(Test-Path -LiteralPath $outputVideo -PathType Leaf) -or (Get-Item -LiteralPath $outputVideo).Length -le 0) {
		throw "Video montage workflow did not create a non-empty MP4."
	}
	if ([string]$render.OutputVideoCodec -ne "h264") {
		throw "Video montage workflow did not create the expected H.264 video stream."
	}
	if (!$render.AudioPreserved -or [string]$render.OutputAudioCodec -ne "aac") {
		throw "Video montage workflow did not preserve the source audio as AAC."
	}

	$sceneRenderOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $sceneOutputVideo `
		-SkipVision `
		-SamplingMode scene `
		-SceneThreshold 0.1 `
		-SceneMinGapSeconds 0.5 `
		-SampleCount 4 `
		-MaxOutputSegments 3 `
		-SegmentDurationSeconds 0.5 `
		-Json)
	$sceneRender = ($sceneRenderOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$sceneRender.Passed -or !$sceneRender.SceneDetectionExercised -or
		$sceneRender.SamplingModeUsed -ne "scene" -or $sceneRender.SceneCutsSeconds.Count -ne 3 -or
		$sceneRender.SegmentCount -ne 3 -or !(Test-Path -LiteralPath $sceneOutputVideo -PathType Leaf)) {
		throw "Video montage workflow did not render a real scene-sampled MP4."
	}

	$manifestRenderOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $manifestOutputVideo `
		-MontageManifestPath $manifestPath `
		-SkipVision `
		-Json)
	$manifestRender = ($manifestRenderOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$manifestRender.Passed -or !$manifestRender.ManifestDriven -or
		$manifestRender.SegmentCount -ne 2 -or $manifestRender.TransitionRendering -ne "ffmpeg-xfade" -or
		$manifestRender.TransitionCount -ne 1 -or
		$manifestRender.OutputDurationSeconds -lt 1.15 -or $manifestRender.OutputDurationSeconds -gt 1.35) {
		throw "Video montage workflow did not render the manifest-driven timeline."
	}
	$renderedTransition = $manifestRender.RenderedTransitions | Select-Object -First 1
	if ($renderedTransition.Kind -ne "crossfade" -or $renderedTransition.VideoFilter -ne "fade" -or
		$renderedTransition.AudioFilter -ne "acrossfade" -or
		[Math]::Abs([double]$renderedTransition.DurationSeconds - 0.25) -gt 0.001) {
		throw "Video montage workflow did not exercise the requested video and audio crossfade."
	}
	if ([Math]::Abs([double]$manifestRender.Segments[0].SourceStartSeconds - 0.25) -gt 0.001 -or
		[Math]::Abs([double]$manifestRender.Segments[0].DurationSeconds - 0.5) -gt 0.001 -or
		[Math]::Abs([double]$manifestRender.Segments[1].SourceStartSeconds - 2.0) -gt 0.001 -or
		[Math]::Abs([double]$manifestRender.Segments[1].DurationSeconds - 1.0) -gt 0.001) {
		throw "Rendered segments did not preserve the manifest source starts and durations."
	}

	$transitionCases = @(
		@{ Kind = "dip"; VideoFilter = "fadeblack" },
		@{ Kind = "wipe"; VideoFilter = "wipeleft" }
	)
	foreach ($transitionCase in $transitionCases) {
		$manifest.options.transitionKind = $transitionCase.Kind
		$manifest.segments[0].transitionOut.kind = $transitionCase.Kind
		$manifest.segments[1].transitionIn.kind = $transitionCase.Kind
		$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
		$transitionOutputVideo = Join-Path $testRoot ("manifest-{0}.mp4" -f $transitionCase.Kind)
		$transitionRenderOutput = @(& $workflowScript `
			-Video $inputVideo `
			-OutputPath $transitionOutputVideo `
			-MontageManifestPath $manifestPath `
			-SkipVision `
			-Json)
		$transitionRender = ($transitionRenderOutput -join [Environment]::NewLine) | ConvertFrom-Json
		$transitionEvidence = $transitionRender.RenderedTransitions | Select-Object -First 1
		if (!$transitionRender.Passed -or $transitionRender.TransitionRendering -ne "ffmpeg-xfade" -or
			$transitionEvidence.Kind -ne $transitionCase.Kind -or
			$transitionEvidence.VideoFilter -ne $transitionCase.VideoFilter -or
			$transitionEvidence.AudioFilter -ne "acrossfade") {
			throw "Video montage workflow did not exercise the $($transitionCase.Kind) transition."
		}
	}

	$manifest.options.overlapTransitions = $false
	$manifest.durationSeconds = 1.5
	$manifest.segments[1].timelineStartSeconds = 0.5
	$manifest.segments[1].timelineEndSeconds = 1.5
	$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
	$nonOverlapOutputVideo = Join-Path $testRoot "manifest-non-overlap.mp4"
	$nonOverlapRenderOutput = @(& $workflowScript `
		-Video $inputVideo `
		-OutputPath $nonOverlapOutputVideo `
		-MontageManifestPath $manifestPath `
		-SkipVision `
		-Json)
	$nonOverlapRender = ($nonOverlapRenderOutput -join [Environment]::NewLine) | ConvertFrom-Json
	if (!$nonOverlapRender.Passed -or $nonOverlapRender.TransitionRendering -ne "hard-cut" -or
		$nonOverlapRender.TransitionCount -ne 0 -or
		$nonOverlapRender.OutputDurationSeconds -lt 1.4 -or $nonOverlapRender.OutputDurationSeconds -gt 1.6) {
		throw "Video montage workflow did not preserve the non-overlapping hard-cut behavior."
	}

	Write-Host "==> Video montage workflow produced sampled and transition-rendered manifest MP4s"
} finally {
	Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
