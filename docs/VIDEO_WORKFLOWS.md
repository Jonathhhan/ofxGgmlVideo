# Video Workflow Boundaries

`ofxGgmlVideo` owns video understanding, frame pipelines, temporal analysis,
and sequence-generation workflows for the ofxGgml ecosystem. This document is
for Codex, GitHub Copilot, Hermes Agent, and human contributors planning
video-lane work before changing runtime behavior.

This guide follows the split rule from the legacy/reference `ofxGgml` docs:
domain workflows, generated media, model-specific preprocessing, and heavy
optional dependencies belong in companion addons. Shared code should move down
only when it is stable, domain-neutral, dependency-light, and covered by
focused tests.

## Owned workflow surface

This addon may define:

- video request/result shapes
- frame sampling and temporal window planning
- video decoding and media handoff documentation
- temporal analysis, event, montage, and scene workflows
- temporal diffusion, image-to-video, GAN, and sequence-generation boundaries
- video restoration, frame interpolation, and frame-to-frame workflows
- focused frame and video examples

## Not owned here

Keep these responsibilities out of `ofxGgmlVideo`:

- ggml setup, backend selection, and runtime discovery owned by `ofxGgmlCore`
- single-image understanding, CLIP, captions, or VLM workflows owned by
  `ofxGgmlVision`
- image-first diffusion, inpainting, identity adapters, or still-image
  generation owned by `ofxGgmlDiffusion`
- audio transcription, music, or voice workflows owned by audio/music lanes
- committed video files, extracted frame caches, generated videos, model files,
  native build trees, or generated openFrameworks project files
- reusable GitHub Actions policy owned by `ofxGgmlWorkflows`

## Planning handoff

Before changing video behavior, write down:

```text
Workflow:
Input media:
Temporal window:
Backend family:
Generated local artifacts:
User-visible output:
Out of scope:
Validation:
```

Runtime changes should name whether the path changes decoding, frame sampling,
temporal analysis, sequence generation, frame export, or example UI.

## Validation ladder

Use the smallest command that proves the changed layer:

| Change type | Suggested validation |
| --- | --- |
| Docs or planning only | `scripts\validate-local.bat` |
| Local setup diagnosis | `scripts\doctor-video.bat` |
| Request/result/helper changes | `scripts\test-addon.bat` |
| Example layout changes | `scripts\validate-local.bat` |

## Safe first tasks

Good early video-lane tasks are:

- documenting frame sampling and temporal-window assumptions
- defining generated frame and video artifact rules
- clarifying image-first work that should remain in `ofxGgmlDiffusion`
- clarifying image-understanding work that should remain in `ofxGgmlVision`
- adding deterministic tests around request/result helpers

Avoid broadening runtime behavior until input media, generated artifacts,
backend-family expectations, user-visible outputs, and validation commands are
explicit.
