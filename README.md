# ofxGgmlVideo

`ofxGgmlVideo` is the companion addon for video montage workflows, clip
timelines, frame pipelines, temporal sampling, and video-to-agent handoff
planning on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the dependency. This addon owns video-specific workflow code so core can stay small and boring.

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
- keep one root-level smoke example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Example

`ofxGgmlVideoFrameExample` is a root-level video request smoke test. Generate it with the openFrameworks projectGenerator using addons `ofxGgmlVideo`, `ofxGgmlCore`, and `ofxImGui`.

`ofxGgmlVideoMontageExample` is a root-level MontageAutomat substrate example.
It shows clip-window controls, deterministic frame references, ordered montage
segments, transition and handle controls, a simple timeline preview, and an
edit-decision-list plus machine-readable manifest handoff without claiming
autonomous editing or media decoding.

For video-lane planning, temporal boundaries, and generated media rules, see
[docs/VIDEO_WORKFLOWS.md](docs/VIDEO_WORKFLOWS.md).

## Dependencies

- openFrameworks
- `ofxGgmlCore`
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

## Boundary

Keep video-specific decoding, frame sampling, temporal preprocessing and
postprocessing, video clip/window planning, montage timeline metadata, media
handling, and examples here. Keep agentic edit decisions in `ofxGgmlAgents` or
the app layer, keep CLIP model ownership in `ofxGgmlVision`, and keep Stable
Diffusion or diffusion-backed image/video generation in the diffusion addons.
Move code down into `ofxGgmlCore` only when it becomes a stable, domain-neutral
primitive with focused tests.
