# Release Checklist

Use this before tagging or announcing an `ofxGgmlVideo` release. The goal is to
prove the addon boundary and example layout without claiming a video runtime
that is not wired yet.

## Fresh Clone Layout

From the openFrameworks `addons` folder:

```powershell
git clone https://github.com/Jonathhhan/ofxGgmlCore.git
git clone https://github.com/Jonathhhan/ofxGgmlVideo.git
cd ofxGgmlVideo
```

Expected layout:

```text
addons/
  ofxGgmlCore/
  ofxGgmlVideo/
  ofxImGui/
```

## Local Validation

Run:

```powershell
scripts\validate-local.bat
```

macOS/Linux:

```sh
./scripts/validate-local.sh
```

For a pre-tag release candidate gate:

```powershell
scripts\release-candidate.bat
```

macOS/Linux:

```sh
./scripts/release-candidate.sh
```

## Example Scope

`ofxGgmlVideoFrameExample` is intentionally narrow in this release:

- root-level openFrameworks example
- `ofxImGui` dependency declared in `addons.make`
- video/frame request smoke surface
- clear future path for temporal analysis and video generation adapters

This release does not promise a complete model-backed video runtime.

## Before Tagging

- `git status --short --ignored` shows no unexpected generated outputs
- no model files, generated media, generated OF project files, or build outputs
  are staged
- `CHANGELOG.md` has an entry for the release
- `docs/releases/vX.Y.Z.md` matches the release scope
- release notes distinguish request/helper skeleton work from future runtime
  adapters
