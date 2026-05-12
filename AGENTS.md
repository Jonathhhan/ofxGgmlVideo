# AGENTS.md

This repository is `ofxGgmlVideo`, the video companion addon for the ofxGgml family.

Codex should treat `ofxGgmlCore` as the backend-neutral foundation. This repo owns video understanding, frame pipelines, temporal analysis, video generation workflows, and video-specific examples.

## Addon contract

Do:

- keep video-specific workflows in this addon
- depend on shared primitives from `ofxGgmlCore`
- preserve openFrameworks addon layout and `addon_config.mk`
- keep examples projectGenerator-friendly
- document runtime/model/video requirements clearly

Do not:

- move backend-neutral Core primitives into this repo
- commit generated videos, frames, binaries, or caches
- hardcode local absolute paths

## Codex workflow

1. Inspect existing files first.
2. Keep changes small and focused.
3. Preserve addon boundaries.
4. Update docs/examples/scripts with code changes.
5. Summarize validation honestly.
