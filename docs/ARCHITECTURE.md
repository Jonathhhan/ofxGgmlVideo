# Architecture

`ofxGgmlVideo` owns video-specific montage and clip workflow code. It should
use `ofxGgmlCore` for stable runtime primitives and keep model-family workflow
details out of core.

## Dependency Direction

```text
openFrameworks app
  -> ofxGgmlVideo
      -> ofxGgmlCore
```

No dependency should point from `ofxGgmlCore` back to `ofxGgmlVideo`.

## Owned Here

- video montage and clip request/result helpers
- edit-decision timeline segments, labels, tags, and references
- deterministic edit-decision-list handoff text
- deterministic machine-readable montage manifest text
- frame sampling and temporal preprocessing boundaries
- video-to-agent and video-to-vision handoff boundaries
- CLIP-style scoring or embedding handoff metadata
- model-specific video postprocessing that is not image-first generation
- focused root-level examples
- local media/model workflow documentation

## Not Owned Here

- ggml runtime setup and backend selection
- generic tensor, graph, model metadata, and result types
- agentic planning loops, autonomous edit decisions, memory, or tool routing
- CLIP model ownership, single-image embeddings, captions, and visual search
- Stable Diffusion, diffusion-backed image-to-video, and image-first generation
- unrelated companion workflows

## Montage And Agent Boundaries

This addon should prepare and represent montage structure: decode or receive
media, choose temporal windows, sample frames, track frame references, create
ordered timeline segments, and hold edit-decision metadata. It can hand sampled
frames to a vision or CLIP scorer owned by `ofxGgmlVision`, and it can hold the
montage plan produced by an agent.

The agentic part of MontageAutomat belongs above this addon. `ofxGgmlAgents`
or an app layer should decide which clips matter, order them, revise a cut, and
route calls to vision, audio, music, or language models. `ofxGgmlVideo`
provides the video memory and edit-plan substrate that agent manipulates.

Generation belongs here only when the core problem is video-native temporal
media handling. Image-first generation, Stable Diffusion wrappers, and
diffusion-backed image-to-video should stay in `ofxGgmlDiffusion` or
`ofxGgmlStableDiffusion`, with this addon used only for montage planning,
clip scheduling, and video-side result organization.

See `docs/VIDEO_WORKFLOWS.md` before expanding this lane. It defines the
planning handoff, generated-media boundaries, backend-family split, and
validation ladder for montage plans, video clips, frame sampling, temporal
analysis, and cross-addon handoff work.
