#include "stdafx.h"
#include "scanner.h"

std::string track_key(metadb_handle_ptr h) {
    return track_key_for(h->get_path(), h->get_subsong_index());
}

bool track_is_live(metadb_handle_ptr h, std::unordered_map<std::string, bool> *cache) {
    const char *path = h->get_path();
    if (strstr(path, "://") != nullptr)
        return true;
    if (cache != nullptr) {
        if (auto it = cache->find(path); it != cache->end())
            return it->second;
    }
    const bool live = (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    if (cache != nullptr)
        cache->emplace(path, live);
    return live;
}

static std::string track_header(metadb_handle_ptr h) {
    pfc::string_formatter header;
    static_api_ptr_t<titleformat_compiler> tc;
    titleformat_object::ptr tf;
    if (tc->compile(tf, "[%artist% - ]%title%")) {
        pfc::string_formatter t;
        if (h->format_title(nullptr, t, tf, nullptr) && !t.is_empty())
            return t.c_str();
    }
    return h->get_path();
}

scan_result scan_playlists() {
    scan_result result;
    static_api_ptr_t<playlist_manager> pm;
    std::unordered_map<std::string, bool> liveCache;
    std::unordered_map<std::string, size_t> keyToIndex;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        const std::string name(plname.c_str());
        for (t_size t = 0; t < items.get_count(); ++t) {
            if (!track_is_live(items[t], &liveCache))
                continue; // dead entries are cleanup, not dupes
            const std::string k(track_key(items[t]));
            auto [it, inserted] = keyToIndex.try_emplace(k, result.tracks.size());
            if (inserted) {
                scanned_track st;
                st.key = k;
                st.header = track_header(items[t]);
                st.playlists.push_back(name);
                st.is_live = true;
                result.tracks.push_back(std::move(st));
            } else {
                result.tracks[it->second].playlists.push_back(name);
            }
        }
    }
    return result;
}

std::vector<dup_info> scan_active_duplicates(std::string &plname_out) {
    std::vector<dup_info> out;
    static_api_ptr_t<playlist_manager> pm;
    pfc::string_formatter nm;
    pm->activeplaylist_get_name(nm);
    plname_out = nm.c_str();
    metadb_handle_list items;
    pm->activeplaylist_get_all_items(items);
    std::unordered_map<std::string, size_t> indexOf;
    std::unordered_map<std::string, metadb_handle_ptr> handles;
    for (t_size t = 0; t < items.get_count(); ++t) {
        if (!track_is_live(items[t]))
            continue;
        const std::string k(track_key(items[t]));
        auto [it, inserted] = indexOf.try_emplace(k, out.size());
        if (inserted) {
            dup_info d;
            d.key = k;
            handles.emplace(k, items[t]);
            out.push_back(std::move(d));
        }
        out[it->second].occurrences++;
    }
    std::vector<dup_info> dupes;
    for (auto &d : out) {
        if (d.occurrences > 1) {
            d.header = track_header(handles[d.key]);
            dupes.push_back(std::move(d));
        }
    }
    out.swap(dupes);
    return dupes;
}

std::vector<dead_entry> scan_dead_tracks() {
    std::vector<dead_entry> out;
    static_api_ptr_t<playlist_manager> pm;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname;
    std::unordered_map<std::string, bool> liveCache;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        for (t_size t = 0; t < items.get_count(); ++t) {
            if (track_is_live(items[t], &liveCache))
                continue;
            dead_entry e;
            e.playlist = p;
            e.playlistName = plname.c_str();
            e.item = t;
            e.header = track_header(items[t]);
            e.path = items[t]->get_path();
            out.push_back(std::move(e));
        }
    }
    return out;
}

void find_playlists_for(metadb_handle_list_cref sel, std::vector<track_report> &out) {
    out.clear();
    const t_size selCount = sel.get_count();
    if (selCount == 0)
        return;

    out.reserve(selCount);
    std::unordered_map<std::string, size_t> indexOf;
    indexOf.reserve(selCount);
    for (t_size i = 0; i < selCount; ++i) {
        track_report r;
        pfc::string_formatter header;
        static_api_ptr_t<titleformat_compiler> tc;
        titleformat_object::ptr tf;
        if (tc->compile(tf, "[%artist% - ]%title%")) {
            pfc::string_formatter t;
            if (sel[i]->format_title(nullptr, t, tf, nullptr) && !t.is_empty())
                header << t.c_str();
        }
        if (header.is_empty())
            header << sel[i]->get_path();
        r.header = header.c_str();
        r.key = track_key(sel[i]);
        indexOf.emplace(r.key, out.size());
        out.push_back(std::move(r));
    }

    static_api_ptr_t<playlist_manager> pm;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        const std::string name(plname.c_str());
        for (t_size t = 0; t < items.get_count(); ++t) {
            record_track_playlist_match(track_key(items[t]), name, indexOf, out);
        }
    }
}

// Playlists this component manages. Excluded from every scan and from export:
// otherwise collected tracks would re-flag themselves on the next run.
bool is_managed_playlist(const char *name) {
    return strcmp(name, "Potential Duplicates") == 0 || strcmp(name, "Tracks Not in Playlists") == 0;
}

// Characters invalid in Windows filenames (and problematic in tags).
// Any of these in artist/title/album flags the track.
bool tag_has_invalid_chars(metadb_handle_ptr h) {
    if (!h)
        return false;
    file_info_impl info;
    if (!h->get_info(info))
        return false;
    const char *field_names[] = {"artist", "title", "album"};
    for (size_t f = 0; f < sizeof(field_names) / sizeof(field_names[0]); ++f) {
        const char *value = info.info_get(field_names[f]);
        if (value && has_invalid_tag_chars(value))
            return true;
    }
    return false;
}

std::vector<dead_entry> scan_invalid_tags() {
    std::vector<dead_entry> out;
    static_api_ptr_t<playlist_manager> pm;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        const std::string name(plname.c_str());
        for (t_size t = 0; t < items.get_count(); ++t) {
            if (!track_is_live(items[t]))
                continue;
            if (!tag_has_invalid_chars(items[t]))
                continue;
            dead_entry e;
            e.playlist = p;
            e.playlistName = name;
            e.item = t;
            e.header = track_header(items[t]);
            e.path = items[t]->get_path();
            out.push_back(std::move(e));
        }
    }
    return out;
}

