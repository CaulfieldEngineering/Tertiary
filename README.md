# Tertiary

Wonderland Audio's three-band tremolo (JUCE; VST3, AU and standalone). Settings that name the product live in
`config.cmake`. What changed in each version is in `CHANGELOG.md`; how the code changed for 1.1 is in
`CLAUDE_CHANGES.md`.

## Release flow

Work happens on **`development`**, the work branch (Joe, 2026-10-31; it replaced `ui-house-style`), then `staging`, then
`production`. Staging is the last look at what will be customer-facing. Nothing public changes until a `v*` tag.

**Push to `staging`** (`.github/workflows/workflow.yml`)

- Builds Mac and Windows, runs the tests and pluginval, signs where the secrets allow, and uploads the
  installers to `CaulfieldEngineering.com/plug-ins/Tertiary/installers-staging/` (not linked from the store; a tag uses `installers/`).
- Builds the web demo and deploys it only to the private preview:
  **https://caulfieldengineering.com/plug-ins/Tertiary/web-staging/**
  It sends `X-Robots-Tag: noindex` and nothing links to it. The run's summary prints the URL and the commit.
- Does not touch the store, the download redirects, the WooCommerce software version or either live demo.

**Push a `v*` tag** (on `production`, matching `config.cmake`)

- Builds again, then `publish` uploads the installers to WordPress, points the download redirects at them,
  removes older copies of the same version, publishes the approved changelog and home-page version line,
  and then sets the WooCommerce software version.
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

### The changelog

`CHANGELOG.md` is the only place the changelog is written. The website's changelog page and the version line on the
home page are filled from it by the pipeline; nobody edits them by hand at a release.

1. **Draft** (at the release-candidate stage, on the mini PC): `node scripts/draft-changelog.mjs`. It reads what
   changed since the last `v*` tag (commit messages, `CLAUDE_CHANGES.md`, the regression list in
   `HQ/plans/tertiary-test-plan.md`) and has `claude -p` write the entry in user-facing words: at most 8 bullets, no
   internal IDs or code words, no em-dashes, nothing that isn't in the changes. It goes into `CHANGELOG.md` as
   `## <version> (release date)` with a `<!-- draft ... -->` line under the heading. `--dry-run` only prints it.
2. **Approve** (Joe): read the entry, change any words, and delete the `<!-- draft` line. Or ask Claude to adjust it.
   Commit it to the release branch.
3. **Release**: on the `v*` tag the pipeline
   - stops before building anything if `CHANGELOG.md` has no entry for the tag's version or the entry is still a
     draft (preflight, "Check CHANGELOG.md Has an Approved Entry for This Version");
   - after both installers are uploaded and the download links point at them, runs
     `.github/scripts/changelog.py publish <version>` ("Publish the Changelog Page and the Home Page Version Line"):
     - dates the entry: a date written in its heading wins; `(release date)` becomes the tag's date in New York
       time (an annotated tag's own date; a lightweight tag has none, so the day the release runs);
     - finds the previous version's installers in the WordPress media library (titled
       `Tertiary Installer v<x.y.z> for Mac` / `for Windows`, as the upload step names them; older installers are
       never deleted, only older copies of the same version);
     - writes every entry up to this version, newest first, into the changelog page's block, with "Need the previous
       version? <x.y.z> for Mac · Windows" under the newest entry (left out if those installers aren't there);
     - sets the home page's version line to "<version>, released <date>";
     - clears Elementor's cache, reads both pages back, and checks that the live pages read exactly what was
       written (every word and link of the block, and the version line). Any mismatch fails the job with the
       reason; the store's "Software version" (which tells existing users about the update) is set only after this
       passes.

   Safety rules in that step: every entry it would show must be approved (an older one still marked draft stops
   it too); each page is read fresh immediately before its own write and the change is built on that copy, so an
   edit Joe saved meanwhile is kept, and the read-back checks only the block and the version line; if the home page
   can't be written, only the changelog block is put back (and read back), never the rest of the page, and not at
   all if someone changed the block meanwhile; whatever happens, the error says what each page shows now and the
   cache is cleared; a re-run of the same version keeps the date already on the page, and older entries keep the
   dates they were published with.

The draft script runs `claude -p` with the style guide as its system prompt, no tools, no MCP servers and no hooks,
from an empty folder: it can only answer with text, and its answer is checked (bullets only, at most 8, no
em-dashes, no internal IDs or hashes) before it is written. Honesty can't be checked by a script, which is why Joe
approves every entry.

The changelog page is `wCHANGELOG_PAGE_ID` in `config.cmake` (4716, wonderlandaudio.com/changelog/). Empty means
no changelog page.

**What the pipeline writes on the site, and nothing else:**
- the changelog page: the one Text Editor widget whose CSS Classes (Advanced tab) include `wla-changelog`,
  "Changelog - entries (written by the release pipeline...)". Its text is replaced whole on every release. The
  title, the intro text above it and anything Joe adds around it are his and never touched. Its spacing is in that
  widget's Advanced > Custom CSS. On a page not built with Elementor, the block is the text between
  `<!-- wla-changelog:start -->` and `<!-- wla-changelog:end -->`.
- the home page (whichever page is the site's front page): the Text Editor widget whose CSS Classes include
  `wla-version` (Specs > Version). Only its version number and date change; other words or a link in it stay. Remove
  the class to stop the pipeline touching it. (Another plugin sold on the same site uses its own class,
  `wla-version-<slug>`, set by the template's workflow, so the two never touch each other's line.)

Keep exactly one `wla-changelog` widget on the changelog page; the pipeline refuses to guess between two.

**Try it without the site:** `python3 .github/scripts/test_changelog.py` (fake site, also run in CI), and a dry run
against saved copies of the pages (REST JSON, `?context=edit`):

```
python .github/scripts/changelog.py publish 1.1.0 --dry-run --release-date 2026-10-30 \
  --page-id 4716 --plugin-name Tertiary --page-json page-4716.json --home-json page-4754.json \
  --media-json media.json --out dry-run
```

It writes the new page data and `block.html` to `dry-run/` and touches nothing online.
