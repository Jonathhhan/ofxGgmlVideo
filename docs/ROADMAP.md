# Roadmap

## Current Milestone

- Seed the companion addon skeleton.
- Keep `ofxGgmlVideoFrameExample` as the first root-level smoke example.
- Keep `ofxGgmlCore` as the only required library dependency; examples may depend on `ofxImGui`.
- Add deterministic clip-window frame sampling helpers.
- Add deterministic montage segment and timeline plan helpers.
- Add deterministic edit-decision-list handoff helpers.
- Add local validation and headless tests.
- Add independent addon version metadata and release-candidate docs.

## Next Milestones

- Connect the first real local backend or bridge adapter.
- Define MontageAutomat handoff contracts for agent-chosen clips, CLIP-style
  scoring, embeddings, temporal summaries, and external bridge outputs.
- Build the montage example into a user-provided-video workflow once decoding
  and media asset rules are explicit.
- Add focused tests around request/result helpers.
- Document the `clone -> setup -> run` path from a new user's point of view.
