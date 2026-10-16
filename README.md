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

## Tests

Pure logic lives in `src/logic.*` (no live foobar services) and is covered by
`tests/` (assert runner, no framework). The build compiles with warnings as
errors (`/WX`). CI builds the DLL, builds the tests, runs them, and fails on
anything red. The Library menu commands double as integration tests: each writes
its findings to `%TEMP%\foo_findplaylists_report.txt`.

## Versioning and releases

- Version lives in one place: `FIP_VERSION` in `src/version.h`.
- Changes are recorded in `CHANGELOG.md`
  ([Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/): Added /
  Changed / Fixed) under `[Unreleased]` as they land.
- Commits use [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/)
  prefixes (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `ci:`, `chore:`)
  so history stays scannable.
- To release: move the `[Unreleased]` entries under a new `## [x.y]` heading,
  bump `FIP_VERSION`, commit, tag `v<version>` (e.g. `v1.4`), push the tag.
- The release workflow builds Release x64, runs the tests, packages
  `foo_findplaylists-<version>.fb2k-component` (the DLL zipped), and creates the
  GitHub release using the changelog section as the notes.

## Layout

- `src/` — component source (`foo_findplaylists.cpp`), pure logic (`logic.*`),
  version (`version.h`).
- `tests/` — unit tests for the pure logic.
- `sdk/` — vendored foobar2000 SDK (headers + libs the build references).
