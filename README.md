# ofxGgmlVideo

`ofxGgmlVideo` is the standalone companion addon for video montage workflows, clip
timelines, frame pipelines, temporal sampling, and video-to-agent handoff
planning.

It has no direct Core or ggml link dependency. Apps explicitly combine it with
`ofxGgmlVision`, `ofxGgmlAgents`, or another local model companion when a
montage needs model-backed decisions.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.1`.

## Features

- frame pipeline workflow boundary
- montage and video clip request boundary
- deterministic edit-decision timeline planning
- temporal frame sampling
- video-to-agent, vision, and CLIP-style scoring handoff planning
- runtime smoke validation entrypoint

## First Milestone

- define small request/result types
- define deterministic clip-window and frame-sampling helpers
- define deterministic montage segment and plan helpers
- keep one canonical root-level montage example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Example

`ofxGgmlVideoMontageExample` is the canonical root-level video request and
MontageAutomat substrate example. Generate it with the openFrameworks
projectGenerator using addons `ofxGgmlVideo` and `ofxImGui`.
It shows clip-window controls, deterministic frame references, ordered montage
segments, transition and handle controls, beat-marker controls, a simple
timeline preview, an edit-decision-list plus machine-readable manifest handoff,
a decoded video preview, and real FFmpeg MP4 rendering without claiming
autonomous editing.

The example subsumes the former single-request frame smoke: every clip exposes
its temporal start, duration, sample rate, and maximum frame count, and every
planned segment displays the resulting frame references. Headless helper
coverage remains in `tests/` and `scripts/run-video-runtime-smoke.*`. Real
captions remain a model-backed boundary exercised by
`scripts/run-model-informed-montage-smoke.*` against a vision-capable
OpenAI-compatible server; the GUI does not simulate captions. The GUI defaults
to local llama.cpp inference: choose the Vision-model GGUF and its matching
`mmproj` GGUF with file dialogs, select `CUDA` or `CPU`, then start a
Vision-ranked render. The workflow reuses `ofxGgmlVision` to launch the local
llama.cpp server with 99 GPU layers for CUDA or zero GPU layers for CPU, and it
reports the selected backend in dry-run and result metadata. The script exposes
the same choice as `-VisionBackend cuda|cpu`. An external OpenAI-compatible
Vision server remains available through an explicit checkbox.

The renderer can use either the visible planned timeline or automatic sampling
over the selected source video. Automatic `scene` mode runs FFmpeg scene-change
detection, filters cuts by a minimum temporal gap, samples the centers of the
resulting scenes, and deterministically limits the candidate count before the
existing optional Vision ranking. If no cut reaches the selected threshold, the
workflow reports that first-class condition and uses explicit uniform sampling;
an FFmpeg detection failure remains an error rather than being hidden by the
fallback. The script exposes these controls as `-SamplingMode scene|uniform`,
`-SceneThreshold`, and `-SceneMinGapSeconds`.

Local server reuse is configuration-safe: the workflow derives a stable server
alias from the Vision GGUF, mmproj GGUF, and selected backend, then checks the
running server's model identity. A server on the local Vision port with a
different model or backend is reported as a conflict instead of being silently
reused.

Frame ranking remains deterministic over real Vision captions. Generic request
words such as `image`, `visual`, and `prefer` do not create false ties, while
near word forms such as `varied`/`various` and `detail`/`detailed` match. The
workflow does not trust an unverified numeric score emitted by a small Vision
model.

For video-lane planning, temporal boundaries, and generated media rules, see
[docs/VIDEO_WORKFLOWS.md](docs/VIDEO_WORKFLOWS.md).

## Dependencies

- openFrameworks
- `ofxImGui` for examples

## Validate

```powershell
scripts\doctor-video.bat
scripts\run-video-runtime-smoke.bat -Json -SummaryOnly
scripts\validate-local.bat
```

On macOS/Linux:

```sh
./scripts/doctor-video.sh
./scripts/run-video-runtime-smoke.sh -Json -SummaryOnly
./scripts/validate-local.sh
```

Montage plans can stay as straight cuts or use deterministic transition
metadata. `ofxGgmlVideoMontageOptions` adds a default transition kind,
transition duration, source-handle duration, and optional overlapping timeline
placement for crossfade-style handoffs. The generated EDL includes source and
timeline ranges, handles, transition hints, tags, and frame references without
creating media.

Beat markers can be generated from BPM and beats-per-bar values. They are
timeline anchors only; audio analysis and beat detection belong in audio/music
lanes or the app layer.

`ofxGgmlVideoUtils::toMontageManifestJson()` creates deterministic JSON-shaped
manifest text for agents, tests, and bridge tools that need structured segment
timing, transitions, tags, and frame references without pulling in a JSON
runtime dependency.

`ofxGgmlVideoUtils::makeMontageHandoff()` creates a deterministic
`montage-handoff-v1` record from a montage plan. The handoff records segment
references and explicit owner slots for agent decisions, CLIP-style scoring,
embedding references, temporal summaries, and external bridge outputs while
leaving those runtime decisions in `ofxGgmlAgents`, `ofxGgmlVision`, or the app
layer.

`scripts\run-video-runtime-smoke.*` is the lane-owned runtime-smoke entrypoint
for ecosystem planning and CI rollouts. It currently proves the deterministic
video request/helper boundary, clip-window sampling helpers, montage plan
helpers, and doctor readiness without claiming agentic edit decisions,
model-backed video understanding, CLIP embedding, captioning, image-to-video,
or sequence-generation inference. Add model-backed checks here only after the
local backend, model paths, input media, generated frame/video outputs, and
cleanup contract are explicit.

For a real model-informed montage decision over extracted or prepared frames,
run the Vision-backed ranking smoke with at least two images:

```powershell
scripts\run-model-informed-montage-smoke.ps1 `
  -Images frame-a.png,frame-b.png `
  -MontagePrompt "red energetic abstract opening" `
  -VisionModel moondream:latest `
  -Json
```

Vision supplies observable frame descriptions. Video then ranks their explicit
prompt-token overlap deterministically and emits ordered timeline segments.
This path does not require or activate `ofxGgmlAgents`.

## Boundary

Keep video-specific decoding, frame sampling, temporal preprocessing and
postprocessing, video clip/window planning, montage timeline metadata, media
handling, and examples here. Keep agentic edit decisions in `ofxGgmlAgents` or
the app layer, keep CLIP model ownership in `ofxGgmlVision`, and keep Stable
Diffusion or diffusion-backed image/video generation in the diffusion addons.
Move code down into `ofxGgmlCore` only when it becomes a stable, domain-neutral
primitive with focused tests.
