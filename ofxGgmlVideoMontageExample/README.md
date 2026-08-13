# ofxGgmlVideoMontageExample

Root-level MontageAutomat substrate example for `ofxGgmlVideo`.

This example does not make autonomous edit decisions, but it does load and
preview a user-selected video and can render a real MP4 through FFmpeg. It shows
the video-side data shape an agent can produce or revise: source clip windows,
sampled frame references, ordered montage segments, transition metadata, beat
markers, and timeline duration. It also displays a deterministic edit-decision
list and a compact machine-readable manifest for agents, tests, and bridge
tools.

This is also the canonical frame-sampling example. Each clip exposes temporal
start, duration, sample rate, and maximum frame count; selected segments show
the deterministic frame references produced by those settings. The same helper
behavior is covered headlessly by the addon tests and
`scripts/run-video-runtime-smoke.*`.

The render-source selector keeps this planned timeline path and adds an
automatic source-sampling path. `Scene-aware` sampling uses FFmpeg to detect
hard visual changes and samples the center of each detected scene before the
existing deterministic or Vision-ranked render. Threshold, minimum scene gap,
candidate count, output count, and segment duration are visible controls.
Videos without a qualifying cut use a clearly reported uniform fallback; the
visible planned timeline remains unchanged by automatic rendering.

For model-informed ordering, select a local Vision model GGUF and its matching
mmproj GGUF, choose `CUDA` or `CPU`, and start a Vision-ranked render. CUDA
offloads model layers to the GPU; CPU passes zero GPU layers to the same local
llama.cpp server. An already-running OpenAI-compatible Vision server remains
available through the external-server toggle. The server owns inference; this
example and the Video addon own the montage boundary.

The local workflow also verifies that a reused server exposes the identity
derived from the selected model, projector, and backend. If port 8082 belongs to
another configuration, rendering stops with a clear conflict instead of
claiming the newly selected backend.

Generate the project with the openFrameworks projectGenerator using addons
`ofxGgmlVideo`, `ofxGgmlCore`, and `ofxImGui`.
