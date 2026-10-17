# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions match `FIP_VERSION` in `src/version.h`.

## [Unreleased]

## [1.8.0] - 2026-10-17

### Changes

- ci: grant release workflow pull-requests write for bump PR (#5) (d4f621c)
- ci: release version bump via auto-merge PR (#4) (57c2f99)
- feat: richer About text with author, date and feature guide (#3) (56761a4)
- ci: auto-approve PRs from repo owner (#2) (b581093)
- chore(release): bump version to 1.7.0 [skip ci] (c1b6065)

## [1.7.0] - 2026-10-17

### Changes

- style: fix version.h clang-format (dfcf4ce)
- style: fix changelog formatting (a34dfbe)
- feat: add scan dead/empty directories with remove capability (5dfd75e)
- chore(release): bump version to 1.6.1 [skip ci] (2dd6eb7)

## [1.6.1] - 2026-10-17

### Added

- Scan Invalid Tags: finds tracks with invalid filename characters in tags and collects them into an Invalid Tags playlist.

### Changed

- Refactored foo_findplaylists.cpp into deep modules: context_menu.cpp, main_menu.cpp, properties.cpp, scanner.cpp, export.cpp, playlist_ops.cpp.
- Unified three playlist scans into a single pass (scan_playlists) with derive_duplicates / derive_orphans.
- Unified dialog boilerplate into pick_folder / pick_format / prompt_save_playlist_path.
- Pure logic (track_is_live, format_title, safe_filename, write_report_file, record_track_playlist_match, derive_duplicates, derive_orphans, has_invalid_tag_chars) lives in logic.cpp for unit testing.
- Fixed ext + 1 pointer arithmetic bug in export (was printing "m3u8" instead of ".m3u8").
- C++20: if init, structured bindings, try_emplace, [[nodiscard]].

### Fixed

- Bulk export wrote zero files (bad install-path handling); replaced by dialog flow.

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
