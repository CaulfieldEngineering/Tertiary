# Tertiary

Wonderland Audio's multiband tremolo (JUCE).

## Release flow

Staging is the last look at what will be customer-facing. Nothing public changes until a `v*` tag.

**Push to `staging`** (`.github/workflows/workflow.yml`)

- Builds Mac and Windows, runs the tests and pluginval, signs where the secrets allow, and uploads the
  installers to `CaulfieldEngineering.com/plug-ins/Tertiary/installers-staging/` (not linked from the store; a tag uses `installers/`).
- Builds the web demo and deploys it only to the private preview:
  **https://caulfieldengineering.com/plug-ins/Tertiary/web-staging/**
  It sends `X-Robots-Tag: noindex` and nothing links to it. The run's summary prints the URL and the commit.
- Does not touch the store, the download redirects, the WooCommerce software version or either live demo.

**Push a `v*` tag** (on `production`, matching `config.cmake`)

- Builds again, then `publish` uploads the installers to WordPress, points the download redirects at them,
  removes older copies and sets the WooCommerce software version.
- After `publish` succeeds, the web demo is released. If the staging preview was built from the tag's commit,
  or from a commit with the same source tree (the usual "Merge branch 'staging' into production"), and
  every file matches its checksum, those exact files go live. Otherwise the run's own build goes live, and
  the summary says why.
- Live demos: `https://wonderlandaudio.com/demo/Tertiary/` (the "Try it in your browser" iframe on
  WonderlandAudio.com) and `https://caulfieldengineering.com/plug-ins/Tertiary/web/`. The CaulfieldEngineering.com
  folder it replaces is kept as `web.previous`.
- Files the previous release listed and this one does not are removed from the WordPress demo folder.
  A re-run of an older tag never deploys its demo over a newer release, and an older staging run never
  replaces a newer preview.

The folder names come from `wWEB_STAGING_DIR` and `wWEB_LIVE_DIR` in `config.cmake`.
