#pragma once

#include <pfc/pfc.h>
#include <functional>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "logic.h"

// Identity of one playable track: file path + subsong index (covers .cue subsongs).
std::string track_key(metadb_handle_ptr h);

// Dead entry: local file that no longer exists. Streams etc (://) are not
// verifiable this way and are always treated as live.
bool track_is_live(metadb_handle_ptr h, std::unordered_map<std::string, bool> *cache = nullptr);

// Single pass over all playlists: builds track->playlists map plus dead set.
scan_result scan_playlists();

// Tracks appearing more than once anywhere (2+ playlists, or repeated).
std::vector<dup_info> scan_active_duplicates(std::string & plname_out);

// Dead entries pointing at missing files, grouped by playlist.
std::vector<dead_entry> scan_dead_tracks();

// For a set of selected tracks, find every playlist containing each.
void find_playlists_for(metadb_handle_list_cref sel, std::vector<track_report> & out);

// Jump to a track inside a playlist: activate it, select + focus the track.
void goto_track_in_playlist(const std::string & playlist, const std::string & key);

// Tracks whose tags contain invalid filename characters (e.g. < > : " / \ | ? *).
// Groups results by playlist for reporting/cleanup.
std::vector<dead_entry> scan_invalid_tags();

// Media-library scan roots (one per volume). MUST be called on the main thread:
// it queries library_manager. info receives library item/dir counts.
std::vector<std::string> library_scan_roots(std::string *info = nullptr);

// Walk the given roots off-thread. onFound, when given, is called once per
// candidate as it is discovered (results are sorted at the end).
std::vector<std::string> scan_dead_directories(const std::vector<std::string> & roots, std::string *diag = nullptr,
                                               const std::function<void(const std::string &)> & onFound = {});

// Convenience wrapper: main-thread roots + filesystem walk in one call.
// diag receives a one-line summary (items seen, roots, dirs visited, errors).
std::vector<std::string> scan_dead_directories(std::string *diag = nullptr);

// Playlists this component manages. Excluded from every scan and from export:
// otherwise collected tracks would re-flag themselves on the next run.
bool is_managed_playlist(const char *name);
