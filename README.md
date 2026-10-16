# foo_findplaylists

Native foobar2000 component (Windows x64): reverse playlist lookup and playlist hygiene.

## What it does

- Right-click track(s) → **Find in Playlists** submenu lists every playlist holding
  them; picking one jumps to the track there.
- Properties dialog shows a **Playlists** group with the same data (the SDK exposes
  rows in the existing Properties tab, not whole new tabs).
- Library → **Find in Playlists** submenu (all file-based, no popups):
  - **Find Playlists Containing Playing Track** → report file.
  - **Collect Potential Duplicates into Playlist** → tracks occurring 2+ times
    anywhere (dead entries excluded) go to a `Potential Duplicates` playlist
    (created once, missing tracks added on rerun).
  - **Collect Tracks Not in Playlists** → library tracks in no playlist go to a
    `Tracks Not in Playlists` playlist.
  - **Find Duplicates in Active Playlist** → report file listing repeats.
  - **Find Dead Tracks in All Playlists** → dry run: report file listing entries
    whose file is gone (streams skipped), changes nothing.
  - **Remove Dead Tracks from All Playlists** → applies the dry-run list
    (per-playlist undo with Ctrl+Z).
  - **Export All Playlists to Folder...** → asks for a folder, then a format
    (M3U8 for Rekordbox, or legacy M3U), and saves every playlist there
    (generated working lists excluded).
  - **Export Active Playlist to File...** → save dialog for just the active
    playlist; the chosen extension decides the format.
  - Reports land in `%TEMP%\foo_findplaylists_report.txt`.

## Install

Use a release `.fb2k-component` file: foobar2000 → File → Preferences →
Components → Install, pick the file, restart when asked. For portable installs
the DLL can also be copied straight to `user-components-x64\foo_findplaylists\`.

## Build

Windows with VS Build Tools 2022 or Visual Studio 2022 (C++ workload), no IDE needed:

```powershell
C:\Users\chris\fb2k-build\build.ps1   # local dev loop: mirror, build, test, package
```

or directly:

```powershell
msbuild foo_findplaylists.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `x64\Release\foo_findplaylists.dll`. Components load at startup; there is
no hot reload. The dev loop is build, copy the DLL, restart foobar.

## Tests and code quality

Pure logic lives in `src/logic.*` (no live foobar services) and is covered by
`tests/` (assert runner, no framework). The build compiles with warnings as
errors (`/WX`). CI builds the DLL, builds the tests, runs them, and fails on
anything red. The Library menu commands double as integration tests: each writes
its findings to `%TEMP%\foo_findplaylists_report.txt`.

### Formatting, Linting & Complexity Checks

- **C++ Formatting**: `clang-format` (configured via `.clang-format`).
- **Markdown / YAML / JSON**: `prettier` (configured via `.prettierrc`).
- **Commit Messages**: `commitlint` validating Conventional Commits format.
- **Complexity & CRAP Score**: `scripts/check_quality.py` computes Cyclomatic Complexity (McCabe) and CRAP (Change Risk Anti-Patterns) scores for all functions.
- **Git Hooks**: Managed by Husky (`.husky/`):
  - `pre-commit`: runs `lint-staged` with `clang-format` and `prettier`.
  - `commit-msg`: runs `commitlint` on the commit message.
  - `pre-push`: runs the full lint and quality checks suite.
- Run checks manually:
  ```bash
  npm run lint          # runs format check + CRAP score / type safety analysis
  npm run format        # formats C++, Markdown, and YAML files
  ```

## Versioning and releases

- Version lives in one place: `FIP_VERSION` in `src/version.h`.
- Changes are recorded in `CHANGELOG.md`
  ([Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/): Added /
  Changed / Fixed) under `[Unreleased]` as they land.
- Commits use [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/)
  prefixes (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `ci:`, `chore:`)
  so history stays scannable.
- PR workflow: Every pull request automatically builds x64 Release binaries, executes the test suite, predicts the semantic version bump based on conventional commits, packages a testable `.fb2k-component` bundle uploaded to the Actions artifacts, and produces a release summary in GitHub Actions.
- Automated semantic releases: When a PR is merged to `master`, GitHub Actions automatically:
  1. Inspects merged commits for conventional commit types (`feat:` -> minor, `fix:` -> patch, `BREAKING CHANGE:` -> major).
  2. Increments `FIP_VERSION` in `src/version.h` and updates `CHANGELOG.md`.
  3. Tags `v<version>` and pushes the bump commit.
  4. Builds and packages `foo_findplaylists-<version>.fb2k-component` with SHA-256 checksums.
  5. Publishes a GitHub Release with:
     - Direct download link and installation instructions for foobar2000.
     - Curated changelog section from `CHANGELOG.md`.
     - Automatically categorized release notes based on the previous release.
- Manual triggers via GitHub Actions `workflow_dispatch` (with bump type selector: auto, patch, minor, major) are also supported.

## Layout

- `src/` — component source (`foo_findplaylists.cpp`), pure logic (`logic.*`),
  version (`version.h`).
- `tests/` — unit tests for the pure logic.
- `sdk/` — vendored foobar2000 SDK (headers + libs the build references).
