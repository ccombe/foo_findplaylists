#pragma once

#include <pfc/pfc.h>
#include <string>

// Folder picker. Returns false when cancelled.
[[nodiscard]] bool pick_folder(std::string &out);

// Format picker: 0 cancelled, 1 .m3u8, 2 .m3u.
[[nodiscard]] int pick_format();

// Save-dialog picker for a single playlist. Returns empty string on cancelled.
[[nodiscard]] std::string prompt_save_playlist_path(const char *playlistName);

void run_export_all_impl(pfc::string_base &msg);
void run_export_active_impl(pfc::string_base &msg);
