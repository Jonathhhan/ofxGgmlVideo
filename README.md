# ofxGgmlVideo

`ofxGgmlVideo` is the companion addon for video understanding, frame pipelines,
temporal analysis, and video generation workflows on top of `ofxGgmlCore`.
Temporal GANs, image-to-video generators, and other sequence generators belong
here when the output is frames or video.

`ofxGgmlCore` stays the dependency. This addon owns video-specific workflow code so core can stay small and boring.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.1`.

## First Milestone

- define small request/result types
- keep one root-level smoke example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Example

`ofxGgmlVideoFrameExample` is a root-level video request smoke test. Generate it with the openFrameworks projectGenerator using addons `ofxGgmlVideo`, `ofxGgmlCore`, and `ofxImGui`.

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

`scripts\run-video-runtime-smoke.*` is the lane-owned runtime-smoke entrypoint
for ecosystem planning and CI rollouts. It currently proves the deterministic
video request/helper boundary and doctor readiness without claiming
model-backed video understanding, temporal analysis, image-to-video, or
sequence-generation inference. Add model-backed checks here only after the
local backend, model paths, input media, generated frame/video outputs, and
cleanup contract are explicit.

## Boundary

Keep video-specific decoding, frame sampling, temporal preprocessing and
postprocessing, model launch, media handling, sequence-generation backends, and
examples here. Move code down into `ofxGgmlCore` only when it becomes a stable,
domain-neutral primitive with focused tests.
