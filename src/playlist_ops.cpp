#include "stdafx.h"
#include "playlist_ops.h"
#include "scanner.h"

class bit_array_none : public bit_array {
  public:
    bool get(t_size) const override { return false; }
};

class bit_array_marks : public bit_array {
  public:
    std::vector<char> m;
    bit_array_marks(t_size n) : m(n, 0) {}
    bool get(t_size n) const override { return n < m.size() && m[n]; }
};

t_size find_or_create_playlist(const char *name) {
    static_api_ptr_t<playlist_manager> pm;
    t_size idx = pm->find_playlist(name, ~0);
    if (idx == ~0)
        idx = pm->create_playlist(name, ~0, ~0);
    return idx;
}

t_size collect_into(const char *plname, const std::vector<metadb_handle_ptr> &tracks) {
    static_api_ptr_t<playlist_manager> pm;
    const t_size pl = find_or_create_playlist(plname);
    if (pl == ~0)
        return 0;
    metadb_handle_list existing;
    pm->playlist_get_all_items(pl, existing);
    if (existing.get_count() > 0) {
        bit_array_marks all(existing.get_count());
        for (size_t i = 0; i < all.m.size(); ++i)
            all.m[i] = 1;
        pm->playlist_remove_items(pl, all);
    }
    metadb_handle_list add;
    std::unordered_set<std::string> seen;
    seen.reserve(tracks.size());
    for (const auto &h : tracks) {
        std::string key = track_key(h);
        if (seen.insert(key).second)
            add.add_item(h);
    }
    if (add.get_count() > 0) {
        bit_array_none sel;
        pm->playlist_add_items(pl, add, sel);
    }
    pm->set_active_playlist(pl); // show the result
    return add.get_count();
}

t_size apply_dead_removal(const std::vector<dead_entry> &dead, std::vector<std::string> &failedPlaylists) {
    failedPlaylists.clear();
    if (dead.empty())
        return 0;
    static_api_ptr_t<playlist_manager> pm;
    t_size total = 0;
    size_t i = 0;
    metadb_handle_list items;
    while (i < dead.size()) {
        const t_size pl = static_cast<t_size>(dead[i].playlist);
        items.remove_all();
        pm->playlist_get_all_items(pl, items);
        bit_array_marks mask(items.get_count());
        t_size marked = 0;
        size_t j = i;
        while (j < dead.size() && dead[j].playlist == dead[i].playlist &&
               dead[j].playlistName == dead[i].playlistName) {
            if (dead[j].item < items.get_count() && !mask.m[dead[j].item]) {
                mask.m[dead[j].item] = 1;
                ++marked;
            }
            ++j;
        }
        if (marked == 0) {
            i = j;
            continue;
        }
        if (pm->playlist_remove_items(pl, mask))
            total += marked;
        else
            failedPlaylists.push_back(dead[i].playlistName);
        i = j;
    }
    return total;
}
