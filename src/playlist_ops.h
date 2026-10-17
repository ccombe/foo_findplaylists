#pragma once

#include <pfc/pfc.h>
#include <string>
#include <vector>
#include "logic.h"

t_size find_or_create_playlist(const char *name);

// Rebuilds a generated playlist from scratch: clears it, then fills it with
// the fresh result set. Generated playlists are snapshots: rerunning refreshes
// them; anything added manually is wiped on the next run.
t_size collect_into(const char *plname, const std::vector<metadb_handle_ptr> &tracks);

t_size apply_dead_removal(const std::vector<dead_entry> &dead, std::vector<std::string> &failedPlaylists);
