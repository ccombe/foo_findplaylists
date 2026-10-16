// Pure logic: no foobar services, unit-testable. Anything needing a live
// playlist_manager / library / metadb stays in foo_findplaylists.cpp.

#pragma once

#include <pfc/pfc.h>

#include <string>
#include <unordered_map>
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
bool record_track_playlist_match(const std::string & key, const std::string & playlistName,
                                 const std::unordered_map<std::string, size_t> & indexOf,
                                 std::vector<track_report> & reports);

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

void format_report(const std::vector<track_report> & reports, pfc::string_base & out);
void format_dead_report(const std::vector<dead_entry> & dead, bool removed, pfc::string_base & out);
void format_dead_report(const std::vector<dead_entry> & dead, bool removed,
                        const std::vector<std::string> & failedPlaylists, pfc::string_base & out);
std::string safe_filename(const char *name);

// Writes body to %TEMP%\foo_findplaylists_report.txt. Returns the path.
std::string write_report_file(const char *title, const pfc::string_base & body);
