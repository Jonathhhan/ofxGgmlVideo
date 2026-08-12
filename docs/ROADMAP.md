# Roadmap

## Current Milestone

- Seed the companion addon skeleton.
- Keep `ofxGgmlVideoMontageExample` as the single canonical root-level example,
  including clip-window frame sampling and montage handoff inspection.
- Keep montage planning independent of a concrete ggml runtime; examples may depend on `ofxImGui` and invoke model companions explicitly.
- Add deterministic clip-window frame sampling helpers.
- Add deterministic montage segment and timeline plan helpers.
- Add transition, handle, and overlapping timeline metadata for montage plans.
- Add deterministic beat/bar timeline marker helpers.
- Add deterministic edit-decision-list handoff helpers.
- Add deterministic machine-readable montage manifest helpers.
- Add deterministic MontageAutomat handoff contracts for agent decisions,
  CLIP-style scoring, embedding references, temporal summaries, and bridge
  outputs.
- Add local validation and headless tests.
- Add independent addon version metadata and release-candidate docs.

## Next Milestones

- Connect the first real local backend or bridge adapter.
- Build the montage example into a user-provided-video workflow once decoding
  and media asset rules are explicit.
- Add focused tests around request/result helpers.
- Document the `clone -> setup -> run` path from a new user's point of view.
