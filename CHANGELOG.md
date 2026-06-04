# Changelog

## Unreleased

- Added deterministic clip-window frame sampling helpers.
- Added deterministic montage segment and timeline plan helpers.
- Added montage transition, handle, timeline-end, and overlapping crossfade
  metadata for deterministic edit-decision handoffs.
- Added deterministic montage edit-decision-list handoff export.
- Updated the frame smoke example to show a MontageAutomat-style plan for later
  agent and vision/CLIP handoff.
- Added `ofxGgmlVideoMontageExample` as a root-level ImGui montage substrate
  example.
- Clarified that agentic edit decisions belong above this addon in
  `ofxGgmlAgents` or the app layer.
- Clarified that CLIP model ownership belongs in `ofxGgmlVision`, while
  Stable Diffusion and diffusion-backed image-to-video remain in diffusion
  addons.

## 1.0.1 - 2026-05-12

- Added independent Video addon version metadata.
- Exposed version metadata through the public umbrella header.
- Documented the release checklist, release policy, and `v1.0.1` scope.
- Kept temporal analysis and video-to-agent/vision handoff adapters as explicit
  future work.

## 1.0.0

- Started `ofxGgmlVideo` as the companion addon for video understanding, frame
  pipelines, temporal analysis, and video workflow boundaries on top of
  `ofxGgmlCore`.
- Added the initial request/result helpers, root-level frame example skeleton,
  local validation, and headless tests.
