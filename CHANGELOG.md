# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions match `FIP_VERSION` in `src/version.h`.

## [Unreleased]

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
