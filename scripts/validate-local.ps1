param()

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-Path {
	param(
		[string]$Path,
		[string]$Label,
		[switch]$Directory
	)

	if ($Directory) {
		if (!(Test-Path -LiteralPath $Path -PathType Container)) {
			throw "$Label was not found: $Path"
		}
	} elseif (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
		throw "$Label was not found: $Path"
	}
}

function Assert-FileContains {
	param(
		[string]$Path,
		[string]$Pattern,
		[string]$Label
	)

	$content = Get-Content -LiteralPath $Path -Raw
	if ($content -notmatch $Pattern) {
		throw "$Label did not contain expected pattern: $Pattern"
	}
}
function Assert-FileNotContains {
	param(
		[string]$Path,
		[string]$Pattern,
		[string]$Label
	)

	$content = Get-Content -LiteralPath $Path -Raw
	if ($content -match $Pattern) {
		throw "$Label contained forbidden pattern: $Pattern"
	}
}
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Split-Path -Parent $scriptRoot
$addonsRoot = Split-Path -Parent $addonRoot

Write-Step "Checking addon skeleton"
Assert-Path (Join-Path $addonRoot "addon_config.mk") "addon config"
Assert-FileNotContains (Join-Path $addonRoot "addon_config.mk") "ADDON_DEPENDENCIES.*ofxGgmlCore" "addon config"
Assert-Path (Join-Path $addonRoot "README.md") "README"
Assert-Path (Join-Path $addonRoot "LICENSE") "license"
Assert-Path (Join-Path $addonRoot "docs\VIDEO_WORKFLOWS.md") "video workflow docs"
Assert-FileContains (Join-Path $addonRoot "README.md") "docs/VIDEO_WORKFLOWS.md" "README"
Assert-FileContains (Join-Path $addonRoot "docs\VIDEO_WORKFLOWS.md") "Planning handoff" "video workflow docs"
Assert-FileContains (Join-Path $addonRoot "docs\VIDEO_WORKFLOWS.md") "Validation ladder" "video workflow docs"
Assert-FileContains (Join-Path $addonRoot "docs\VIDEO_WORKFLOWS.md") "generated artifacts" "video workflow docs"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlVideo.h") "public header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlVideoVersion.h") "version header"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlVideo.h") "ofxGgmlVideoVersion.h" "public header"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlVideoVersion.h") "OFXGGML_VIDEO_VERSION_STRING" "version header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlVideo\ofxGgmlVideoTypes.h") "types header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlVideo\ofxGgmlVideoUtils.h") "utility header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlVideo\ofxGgmlVideoUtils.cpp") "utility source"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlVideo\ofxGgmlVideoTypes.h") "ofxGgmlVideoMontageOptions" "montage options type"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlVideo\ofxGgmlVideoTypes.h") "transitionOut" "montage transition metadata"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlVideo\ofxGgmlVideoTypes.h") "ofxGgmlVideoMontageHandoff" "montage handoff type"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlVideo\ofxGgmlVideoUtils.h") "makeMontageHandoff" "montage handoff helper"
Assert-FileContains (Join-Path $addonRoot "docs\VIDEO_WORKFLOWS.md") "montage-handoff-v1" "montage handoff docs"

Write-Step "Checking dependency layout"
Assert-Path (Join-Path $addonsRoot "ofxImGui") "sibling ofxImGui addon for examples" -Directory

Write-Step "Checking example layout"
$montageExampleRoot = Join-Path $addonRoot "ofxGgmlVideoMontageExample"
Assert-Path $montageExampleRoot "root-level montage example" -Directory
Assert-Path (Join-Path $montageExampleRoot "addons.make") "montage example addons.make"
Assert-FileNotContains (Join-Path $montageExampleRoot "addons.make") "(?m)^ofxGgmlCore\s*$" "montage example addons.make"
Assert-FileContains (Join-Path $montageExampleRoot "addons.make") "(?m)^ofxImGui\s*$" "montage example addons.make"
Assert-Path (Join-Path $montageExampleRoot "src\main.cpp") "montage example main.cpp"
Assert-Path (Join-Path $montageExampleRoot "src\ofApp.h") "montage example ofApp.h"
Assert-Path (Join-Path $montageExampleRoot "src\ofApp.cpp") "montage example ofApp.cpp"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "transition duration" "montage example transition controls"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "overlap transitions" "montage example overlap controls"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "Montage Handoff" "montage example handoff panel"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "sample fps" "montage example frame sampling controls"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "frame references" "montage example frame references"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "ofDisableArbTex\(\)" "montage example GL_TEXTURE_2D preview allocation"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "Browse local Vision model GGUF" "montage example local Vision model browser"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "Browse Vision mmproj GGUF" "montage example local Vision projector browser"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "Render Vision-ranked MP4 \(local CUDA\)" "montage example local CUDA render path"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "Render Vision-ranked MP4 \(local CPU\)" "montage example local CPU render path"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "MontageManifestPath" "montage example manifest-driven render handoff"
Assert-FileContains (Join-Path $montageExampleRoot "src\ofApp.cpp") "overlapping transitions come from the visible montage" "montage example transition render disclosure"
Assert-FileContains (Join-Path $scriptRoot "run-video-montage-workflow.ps1") "VisionBackend" "montage workflow local backend selection"
Assert-FileContains (Join-Path $scriptRoot "run-video-montage-workflow.ps1") "ManifestDriven" "montage workflow manifest-driven render evidence"
Assert-FileContains (Join-Path $scriptRoot "run-video-montage-workflow.ps1") "Resolve-XfadeTransition" "montage workflow transition renderer"
Assert-FileNotContains (Join-Path $scriptRoot "run-video-montage-workflow.ps1") "Assert-LocalVisionServerIdentity" "montage workflow duplicate server identity check"
Assert-FileContains (Join-Path $scriptRoot "run-video-montage-workflow.ps1") "start-local-vision-server.ps1" "montage workflow local Vision launcher handoff"

Assert-Path (Join-Path $addonRoot "tests\CMakeLists.txt") "test CMakeLists"
Assert-Path (Join-Path $addonRoot "tests\test_main.cpp") "test source"
Assert-Path (Join-Path $scriptRoot "doctor-video.ps1") "Video doctor script"
Assert-Path (Join-Path $scriptRoot "doctor-video.bat") "Video doctor Windows wrapper"
Assert-Path (Join-Path $scriptRoot "doctor-video.sh") "Video doctor shell wrapper"
Assert-Path (Join-Path $scriptRoot "test-doctor-video.ps1") "Video doctor smoke test"
Assert-Path (Join-Path $scriptRoot "run-video-runtime-smoke.ps1") "Video runtime smoke script"
Assert-Path (Join-Path $scriptRoot "run-video-runtime-smoke.bat") "Video runtime smoke Windows wrapper"
Assert-Path (Join-Path $scriptRoot "run-video-runtime-smoke.sh") "Video runtime smoke shell wrapper"
Assert-Path (Join-Path $scriptRoot "test-video-runtime-smoke.ps1") "Video runtime smoke contract test"
Assert-Path (Join-Path $scriptRoot "run-model-informed-montage-smoke.ps1") "model-informed montage smoke"
Assert-Path (Join-Path $scriptRoot "run-model-informed-montage-smoke.bat") "model-informed montage smoke Windows wrapper"
Assert-FileContains (Join-Path $scriptRoot "run-model-informed-montage-smoke.ps1") "Get-ScoringTokens" "model-informed montage prompt filtering"
Assert-FileContains (Join-Path $scriptRoot "run-model-informed-montage-smoke.ps1") "Get-MatchedPromptTokens" "model-informed montage word-form matching"
Assert-Path (Join-Path $scriptRoot "test-video-montage-workflow.ps1") "video montage workflow test"

$nestedExamples = Join-Path $addonRoot "examples"
if (Test-Path -LiteralPath $nestedExamples -PathType Container) {
	throw "Examples should live at the addon root, not under: $nestedExamples"
}

Write-Step "Checking generated artifact hygiene"
$forbidden = @(
	"build",
	".vs",
	"ofxGgmlVideoMontageExample\bin",
	"ofxGgmlVideoMontageExample\obj",
	"ofxGgmlVideoMontageExample\.vs",
	"models"
)

foreach ($relative in $forbidden) {
	$path = Join-Path $addonRoot $relative
	if (Test-Path -LiteralPath $path) {
		$gitPath = $relative.Replace("\", "/")
		$tracked = @(& git -C $addonRoot ls-files -- $gitPath "$gitPath/**")
		if ($tracked.Count -gt 0) {
			throw "Generated or local-only path is tracked by Git: $relative"
		}

		& git -C $addonRoot check-ignore -q -- "$gitPath/"
		if ($LASTEXITCODE -ne 0) {
			throw "Generated or local-only path is not ignored by Git: $relative"
		}
	}
}

Write-Step "Checking Video doctor"
& (Join-Path $scriptRoot "test-doctor-video.ps1")
if (!$?) {
	throw "Video doctor smoke test failed"
}

Write-Step "Checking Video runtime smoke contract"
& (Join-Path $scriptRoot "test-video-runtime-smoke.ps1")

Write-Step "Checking rendered video montage workflow"
& (Join-Path $scriptRoot "test-video-montage-workflow.ps1")

Write-Step "Running headless tests"
& (Join-Path $scriptRoot "test-addon.ps1")
if ($LASTEXITCODE -ne 0) {
	throw "Headless tests failed with exit code $LASTEXITCODE"
}

Write-Step "ofxGgmlVideo local validation passed"
