# ofxGgmlVideoMontageExample

Root-level MontageAutomat substrate example for `ofxGgmlVideo`.

This example does not make autonomous edit decisions and does not decode media.
It shows the video-side data shape an agent can produce or revise: source clip
windows, sampled frame references, ordered montage segments, transition
metadata, beat markers, and timeline duration. It also displays a deterministic
edit-decision-list text handoff that an app can save or translate into a richer
project format, plus a compact machine-readable manifest for agents, tests, and
bridge tools.

This is also the canonical frame-sampling example. Each clip exposes temporal
start, duration, sample rate, and maximum frame count; selected segments show
the deterministic frame references produced by those settings. The same helper
behavior is covered headlessly by the addon tests and
`scripts/run-video-runtime-smoke.*`.

Captioning is deliberately not faked in the GUI. For real captions and a
model-informed ordering over extracted frames, run
`scripts/run-model-informed-montage-smoke.*` against a vision-capable
OpenAI-compatible llama-server. The server owns inference; this example and the
Video addon own the deterministic montage boundary.

Generate the project with the openFrameworks projectGenerator using addons
`ofxGgmlVideo`, `ofxGgmlCore`, and `ofxImGui`.
