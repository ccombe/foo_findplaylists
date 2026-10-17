#include "stdafx.h"
#include "scanner.h"

#include <algorithm>

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

static bool tag_has_invalid_chars(metadb_handle_ptr h);

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

std::vector<dup_info> scan_active_duplicates(std::string & plname_out) {
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
    for (auto & d : out) {
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

bool is_managed_playlist(const char *name) {
    return strcmp(name, "Potential Duplicates") == 0 || strcmp(name, "Tracks Not in Playlists") == 0;
}

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

std::vector<std::string> scan_dead_directories() {
    std::vector<std::string> candidates;
    static_api_ptr_t<library_manager> lm;
    metadb_handle_list all;
    lm->get_all_items(all);

    // Collect unique parent directories from all library tracks.
    std::unordered_set<std::string> roots;
    for (t_size t = 0; t < all.get_count(); ++t) {
        const char *path = all[t]->get_path();
        if (!path)
            continue;
        std::string dir(path);
        size_t lastSlash = dir.find_last_of("\\/");
        if (lastSlash != std::string::npos)
            dir = dir.substr(0, lastSlash);
        roots.insert(dir);
    }

    // For each unique directory, enumerate all subdirectories recursively.
    std::unordered_set<std::string> visited;
    for (const auto & root : roots) {
        std::vector<std::string> stack;
        stack.push_back(root);
        while (!stack.empty()) {
            std::string dir = stack.back();
            stack.pop_back();

            if (!dir.empty() && (dir.back() == '\\' || dir.back() == '/'))
                dir.pop_back();

            if (visited.count(dir))
                continue;
            visited.insert(dir);

            // Enumerate entries in this directory.
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA((dir + "\\*").c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE)
                continue;

            std::vector<std::string> filenames;
            std::vector<std::string> subdirs;
            do {
                const char *name = fd.cFileName;
                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                    continue;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    subdirs.push_back(dir + "\\" + name);
                } else {
                    filenames.push_back(name);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);

            // Classify this directory.
            dir_status st = classify_directory(filenames);
            if (st == dir_status::empty_or_dead) {
                candidates.push_back(dir);
            }

            // Push subdirs for traversal.
            for (const auto & sd : subdirs) {
                if (!visited.count(sd))
                    stack.push_back(sd);
            }
        }
    }

    // Sort and deduplicate.
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    return candidates;
}

void find_playlists_for(metadb_handle_list_cref sel, std::vector<track_report> & out) {
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

void goto_track_in_playlist(const std::string & playlist, const std::string & key) {
    static_api_ptr_t<playlist_manager> pm;
    const t_size pl = pm->find_playlist(playlist.c_str());
    if (pl == ~0)
        return;
    metadb_handle_list items;
    pm->playlist_get_all_items(pl, items);
    for (t_size t = 0; t < items.get_count(); ++t) {
        if (track_key(items[t]) == key) {
            pm->set_active_playlist(pl);
            pm->playlist_set_selection_single(pl, t, true);
            pm->playlist_set_focus_item(pl, t);
            pm->playlist_ensure_visible(pl, t);
            return;
        }
    }
}