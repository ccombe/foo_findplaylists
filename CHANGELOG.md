# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions match `FIP_VERSION` in `src/version.h`.

## [Unreleased]

## [1.5.0] - 2026-10-16

### Changes
- perf: reduce nested logic, use guard clauses, and optimize algorithmic complexity (#1) (7162b1b)
- fix(release): fix commits string parsing in release and ci workflows (6ee8c05)
- perf: reduce nested logic, use guard clauses, and optimize algorithmic complexity (e67d3fa)
- ci: upgrade GitHub Actions to latest supported versions (v7/v6/v3) and Node 22 (a400154)
- fix(build): restore foobar2000-lite.h include order in stdafx.h (00c81a9)
- fix(build): preserve stdafx.h include order with SortIncludes: Never (254d4c9)
- fix(ci): remove unused foo_sample SDK solution breaking dependency submission (4e45315)
- feat: configure linter, formatter, code complexity, CRAP score, and git hooks (e2b312a)
- fix(release): format package name in install instructions [skip ci] (8fe0db8)
- chore(release): bump version to 1.4.0 [skip ci] (544e85d)

## [1.4] - 2026-10-16

### Added

- Export All Playlists to Folder...: folder picker plus M3U8/M3U format choice.
- Export Active Playlist to File...: save dialog for a single playlist.
- Confirmation popups with counts on every Library command, including explicit
  none-found messages.
- Dead-track report lines now include the full file path.
- Removal failures are reported per playlist instead of silently counted.

### Changed

- Generated playlists (`Potential Duplicates`, `Tracks Not in Playlists`) are
  rebuilt as snapshots on each run and are excluded from every scan and from
  export, so collected tracks can no longer re-flag themselves.

### Fixed

- Bulk export wrote zero files (bad install-path handling); replaced by the
  dialog flow above.

## [1.3] - 2026-10-16

### Added

- Find Dead Tracks (dry run) and Remove Dead Tracks, per-playlist Ctrl+Z safety.
- Unit tests for all pure logic (22 checks), warnings-as-errors build.

### Changed

- Plain-language names: Potential Duplicates, Tracks Not in Playlists.
- Duplicate scans skip dead entries.

## [1.2] - 2026-10-16

### Added

- Collect Potential Duplicates into Playlist.
- Collect Tracks Not in Playlists.
- Find Duplicates in Active Playlist.
- Library commands write file-based reports.

## [1.1] - 2026-10-16

### Added

- Dynamic right-click submenu listing the playlists inline; picking one jumps
  to the track in that playlist.

## [1.0] - 2026-10-16

### Added

- Right-click track(s) to see which playlists contain them (popup report).
- Library command reporting the playlists holding the now-playing track.
- Properties dialog gains a Playlists group for the selected track(s).

