#pragma once

#include <pfc/pfc.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Identity of one playable track: file path + subsong index.
std::string track_key_for(const char *path, unsigned subsong);

struct track_report {
    std::string header;
    std::string key;
    std::vector<std::string> lists;
};

// Matches a track key against indexed tracks and appends the playlist name if not already recorded.
// Returns true if a new association was recorded.
bool record_track_playlist_match(const std::string &key, const std::string &playlistName,
                                 const std::unordered_map<std::string, size_t> &indexOf,
                                 std::vector<track_report> &reports);

struct dup_info {
    std::string header;
    std::string key; // track_key_for() identity, for handle lookup
    size_t occurrences = 0;
    std::vector<std::string> playlists;
};

struct dead_entry {
    size_t playlist;
    std::string playlistName;
    size_t item;
    std::string header;
    std::string path;
};

// Returns true if a tag field contains characters invalid in filenames
// (e.g. < > : " / \ | ? *) or control characters.
bool has_invalid_tag_chars(const char *field);

// One entry produced by a single unified scan of all playlists.
struct scanned_track {
    std::string key;
    std::string header;
    std::vector<std::string> playlists; // playlists this track appears in
    bool is_live;                        // false if the file no longer exists
};

// Result of a single unified pass over all playlists.
struct scan_result {
    std::vector<scanned_track> tracks;      // every track found in any playlist
    std::vector<dead_entry> dead;           // dead entries (track not live)
    std::vector<std::string> orphan_keys;   // track keys in library but in no playlist
};

// Derive dup_info (tracks appearing 2+ times) from a scan result.
std::vector<dup_info> derive_duplicates(const scan_result &result);

// Derive orphan library tracks (in library, in no playlist) from a scan result.
// library_keys: all track keys in the media library.
std::vector<dup_info> derive_orphans(const scan_result &result,
                                     const std::unordered_set<std::string> &library_keys);

// Formatters ----------------------------------------------------------------

void format_report(const std::vector<track_report> &reports, pfc::string_base &out);
void format_dead_report(const std::vector<dead_entry> &dead, bool removed, pfc::string_base &out);
void format_dead_report(const std::vector<dead_entry> &dead, bool removed,
                        const std::vector<std::string> &failedPlaylists, pfc::string_base &out);
std::string safe_filename(const char *name);

// Writes body to %TEMP%\foo_findplaylists_report.txt. Returns the path.
std::string write_report_file(const char *title, const pfc::string_base &body);
