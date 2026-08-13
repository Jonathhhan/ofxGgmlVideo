# Video Workflow Boundaries

`ofxGgmlVideo` owns video montage workflows, clip timelines, frame pipelines,
temporal sampling, and video-to-agent handoff planning for the ofxGgml
ecosystem. This document is for Codex, GitHub Copilot, Hermes Agent, and human
contributors planning video-lane work before changing runtime behavior.

This guide follows the split rule from the legacy/reference `ofxGgml` docs:
domain workflows, generated media, model-specific preprocessing, and heavy
optional dependencies belong in companion addons. Shared code should move down
only when it is stable, domain-neutral, dependency-light, and covered by
focused tests.

## Owned workflow surface

This addon may define:

- video request/result shapes
- montage plan, edit-decision segment, and timeline metadata shapes
- transition, source-handle, and overlapping montage timing metadata
- deterministic beat and bar markers for timeline anchoring
- deterministic edit-decision-list handoff text
- deterministic montage manifest text for agent and bridge handoff
- frame sampling and temporal window planning
- video decoding and media handoff documentation
- temporal analysis, event, montage, scene, and clip-selection workflows
- handoff points for agentic MontageAutomat decisions owned by
  `ofxGgmlAgents` or the app layer
- handoff points for CLIP-style scoring, embeddings, and visual search owned by
  `ofxGgmlVision` or the app layer
- video-side result organization for generated or transformed clips owned by
  another addon
- focused frame and video examples

## Not owned here

Keep these responsibilities out of `ofxGgmlVideo`:

- ggml setup, backend selection, and runtime discovery owned by `ofxGgmlCore`
- agent planning, memory, autonomous edit decisions, and tool routing owned by
  `ofxGgmlAgents` or the app layer
- single-image understanding, CLIP, captions, or VLM workflows owned by
  `ofxGgmlVision`
- image-first diffusion, inpainting, identity adapters, or still-image
  generation owned by `ofxGgmlDiffusion`
- Stable Diffusion image-to-video generation owned by
  `ofxGgmlStableDiffusion` or the diffusion lane
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

Runtime changes should name whether the path changes decoding, montage
planning, clip planning, frame sampling, temporal analysis, agent handoff,
vision/CLIP handoff, frame export, or example UI.

### Rendering an existing montage manifest

`scripts/run-video-montage-workflow.ps1 -MontageManifestPath <path>` consumes
the existing `ofxGgmlVideoMontageManifest` version 1 contract. Its segment
source starts and durations become the actual FFmpeg render windows instead of
being replaced by evenly spaced samples. The renderer accepts one input video
per invocation. Overlapping `crossfade`, `dip`, and `wipe` transitions render
through FFmpeg `xfade`, with `acrossfade` preserving the corresponding audio
transition. Cuts and transitions planned without overlap remain hard cuts.

Without `-MontageManifestPath`, the workflow preserves its original evenly
spaced sampling behavior. Model-backed mode ranks the representative frame
from each supplied segment while retaining that segment's source window.

## Validation ladder

Use the smallest command that proves the changed layer:

| Change type | Suggested validation |
| --- | --- |
| Docs or planning only | `scripts\validate-local.bat` |
| Local setup diagnosis | `scripts\doctor-video.bat` |
| Montage transition or handle helpers | `scripts\test-addon.bat` |
| Request/result/helper changes | `scripts\test-addon.bat` |
| Ecosystem runtime smoke evidence | `scripts\run-video-runtime-smoke.bat -Json -SummaryOnly` |
| Example layout changes | `scripts\validate-local.bat` |

`scripts\run-video-runtime-smoke.*` is intentionally request-boundary-only
until this addon owns a real local video backend. It compiles and runs the
deterministic helper tests, checks doctor readiness, and emits JSON for Core
planning without downloading models, requiring video media, extracting frame
caches, or committing generated videos.

## Safe first tasks

Good early video-lane tasks are:

- documenting frame sampling and temporal-window assumptions
- defining montage segment, timeline, and edit-decision metadata
- adding deterministic transition or handle metadata without decoding media
- adding deterministic beat/bar marker metadata without audio analysis
- defining edit-decision-list export and handoff metadata
- defining machine-readable montage manifest export metadata
- defining MontageAutomat handoff contracts with decision owner, scoring owner,
  embedding references, temporal summaries, and external bridge output slots
- defining video clip planning and agent/CLIP/vision handoff metadata
- defining generated frame and video artifact rules
- clarifying image-first work that should remain in `ofxGgmlDiffusion`
- clarifying image-understanding work that should remain in `ofxGgmlVision`
- adding deterministic tests around request/result helpers

Avoid broadening runtime behavior until input media, generated artifacts,
backend-family expectations, user-visible outputs, and validation commands are
explicit.

## MontageAutomat handoff contract

`ofxGgmlVideo` may create a deterministic `montage-handoff-v1` record from an
existing montage plan. The contract records the video-side source clips,
sampled frame references, labels, and timeline context, then leaves agentic
fields as explicit slots for other owners:

- `decisionOwner`: the agent or app layer that chose clips and order
- `scoringOwner`: the vision or CLIP lane that scored frames, clips, or
  embeddings
- `bridgeOwner`: the external bridge or app layer that produced media-side
  outputs
- `agentReason`: the reason an agent selected or revised a segment
- `temporalSummary`: a text summary of the segment's visual/temporal content
- `scoreKind` and `score`: a CLIP-style, embedding, or app-specific score
- `embeddingReference` and `embeddingDimensions`: references to embeddings
  stored outside this addon
- `bridgeOutputKind` and `bridgeOutputReference`: references to external bridge
  outputs stored outside this addon

Keep the handoff deterministic and text-exportable. Do not add autonomous edit
decisions, CLIP inference, embedding storage, video decoding, or generated media
ownership to this addon when filling these slots.
