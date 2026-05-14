# Architecture

`ofxGgmlVideo` owns video-specific workflow code. It should use `ofxGgmlCore` for stable runtime primitives and keep model-family workflow details out of core.

## Dependency Direction

```text
openFrameworks app
  -> ofxGgmlVideo
      -> ofxGgmlCore
```

No dependency should point from `ofxGgmlCore` back to `ofxGgmlVideo`.

## Owned Here

- video-specific request/result helpers
- frame sampling and temporal preprocessing boundaries
- temporal GAN and sequence-generation backend boundaries
- model-specific video postprocessing
- focused root-level examples
- local media/model workflow documentation

## Not Owned Here

- ggml runtime setup and backend selection
- generic tensor, graph, model metadata, and result types
- unrelated companion workflows

## GAN Video Generation

GAN work that produces frame sequences, motion, image-to-video output, or video
restoration belongs here. Reuse `ofxGgmlDiffusion` only when the workflow is
really image-first; keep temporal state, frame scheduling, media IO, and video
result handling in this addon.

See `docs/VIDEO_WORKFLOWS.md` before expanding this lane. It defines the
planning handoff, generated-media boundaries, backend-family split, and
validation ladder for video understanding, temporal analysis, and sequence
generation work.
