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
- model-specific video postprocessing
- focused root-level examples
- local media/model workflow documentation

## Not Owned Here

- ggml runtime setup and backend selection
- generic tensor, graph, model metadata, and result types
- unrelated companion workflows
