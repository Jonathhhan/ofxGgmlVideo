# ofxGgmlVideoMontageExample

Root-level MontageAutomat substrate example for `ofxGgmlVideo`.

This example does not make autonomous edit decisions and does not decode media.
It shows the video-side data shape an agent can produce or revise: source clip
windows, sampled frame references, ordered montage segments, transition
metadata, beat markers, and timeline duration. It also displays a deterministic
edit-decision-list text handoff that an app can save or translate into a richer
project format, plus a compact machine-readable manifest for agents, tests, and
bridge tools.

Generate the project with the openFrameworks projectGenerator using addons
`ofxGgmlVideo`, `ofxGgmlCore`, and `ofxImGui`.
