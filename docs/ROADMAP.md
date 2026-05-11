# Roadmap

## Current Milestone

- Seed the companion addon skeleton.
- Keep `ofxGgmlVideoFrameExample` as the first root-level smoke example.
- Keep `ofxGgmlCore` as the only required library dependency; examples may depend on `ofxImGui`.
- Add local validation and headless tests.

## Next Milestones

- Connect the first real local backend or bridge adapter.
- Define video generation backend families, including temporal diffusion,
  transformer/sequence models, GAN-style generators, and explicit external
  bridges.
- Add one useful openFrameworks example that runs with user-provided video assets.
- Add focused tests around request/result helpers.
- Document the `clone -> setup -> run` path from a new user's point of view.
