// foo_findplaylists - reverse playlist lookup for foobar2000.
//
// Right-click track(s) -> "Find in Playlists" submenu lists every playlist
// holding them; picking one jumps to the track in that playlist.
// Same data also appears as a "Playlists" group in the Properties dialog,
// and Library -> "Find in Playlists" submenu holds library-wide commands
// (all file-based, no popups).

#include "stdafx.h"
#include "logic.h"
#include "version.h"

DECLARE_COMPONENT_VERSION("Find in Playlists", FIP_VERSION, "Shows which playlists contain the selected track(s).");
VALIDATE_COMPONENT_FILENAME("foo_findplaylists.dll");
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;

namespace {

// Identity of one playable track: file path + subsong index (covers .cue subsongs).
std::string track_key(metadb_handle_ptr h) {
    return track_key_for(h->get_path(), h->get_subsong_index());
}

static titleformat_object::ptr get_track_header_formatter() {
    static titleformat_object::ptr tf;
    if (tf.is_empty()) {
        static_api_ptr_t<titleformat_compiler> tc;
        tc->compile(tf, "[%artist% - ]%title%");
    }
    return tf;
}

static std::string track_header(metadb_handle_ptr h) {
    titleformat_object::ptr tf = get_track_header_formatter();
    if (tf.is_valid()) {
        pfc::string_formatter t;
        if (h->format_title(nullptr, t, tf, nullptr) && !t.is_empty())
            return t.c_str();
    }
    return h->get_path();
}

// Core: for each selected track, the playlists containing it.
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
        r.header = track_header(sel[i]);
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

// --- library-wide scans ----------------------------------------------------

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

// Playlists this component manages. Excluded from every scan and from export:
// otherwise collected tracks would re-flag themselves on the next run.
static bool is_managed_playlist(const char *name) {
    return strcmp(name, "Potential Duplicates") == 0 || strcmp(name, "Tracks Not in Playlists") == 0;
}

// Dead entry: local file that no longer exists. Streams etc (://) are not
// verifiable this way and are always treated as live.
static bool track_is_live(metadb_handle_ptr h, std::unordered_map<std::string, bool> *cache = nullptr) {
    const char *path = h->get_path();
    if (strstr(path, "://") != nullptr)
        return true;
    if (cache != nullptr) {
        auto it = cache->find(path);
        if (it != cache->end())
            return it->second;
    }
    const bool live = (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    if (cache != nullptr)
        cache->emplace(path, live);
    return live;
}

// Every track occurring more than once anywhere (2+ playlists, or repeated).
static void scan_duplicates(std::vector<dup_info> & out, std::unordered_map<std::string, metadb_handle_ptr> & handles) {
    out.clear();
    handles.clear();
    static_api_ptr_t<playlist_manager> pm;
    std::unordered_map<std::string, size_t> indexOf;
    std::unordered_map<std::string, bool> liveCache;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        if (is_managed_playlist(plname.c_str()))
            continue;
        const std::string name(plname.c_str());
        for (t_size t = 0; t < items.get_count(); ++t) {
            if (!track_is_live(items[t], &liveCache))
                continue; // dead entries are cleanup, not dupes
            const std::string k(track_key(items[t]));
            auto it = indexOf.find(k);
            size_t idx;
            if (it == indexOf.end()) {
                dup_info d;
                d.key = k;
                idx = out.size();
                indexOf.emplace(k, idx);
                handles.emplace(k, items[t]);
                out.push_back(std::move(d));
            } else
                idx = it->second;
            out[idx].occurrences++;
            if (out[idx].playlists.empty() || out[idx].playlists.back() != name)
                out[idx].playlists.push_back(name);
        }
    }
    std::vector<dup_info> dupes;
    for (auto & d : out) {
        if (d.occurrences > 1) {
            d.header = track_header(handles[d.key]);
            dupes.push_back(std::move(d));
        }
    }
    out.swap(dupes);
}

// Library tracks sitting in no playlist at all.
static void scan_orphans(std::vector<dup_info> & out, std::unordered_map<std::string, metadb_handle_ptr> & handles) {
    out.clear();
    handles.clear();
    static_api_ptr_t<playlist_manager> pm;
    std::unordered_set<std::string> placed;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        if (is_managed_playlist(plname.c_str()))
            continue;
        for (t_size t = 0; t < items.get_count(); ++t)
            placed.insert(track_key(items[t]));
    }
    metadb_handle_list all;
    static_api_ptr_t<library_manager>()->get_all_items(all);
    for (t_size t = 0; t < all.get_count(); ++t) {
        const std::string k(track_key(all[t]));
        if (placed.count(k))
            continue;
        dup_info d;
        d.header = track_header(all[t]);
        d.key = k;
        d.occurrences = 0;
        handles.emplace(k, all[t]);
        out.push_back(std::move(d));
    }
}

// Tracks repeated inside the active playlist.
static void scan_active_duplicates(std::vector<dup_info> & out, std::string & plname_out) {
    out.clear();
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
        auto it = indexOf.find(k);
        size_t idx;
        if (it == indexOf.end()) {
            dup_info d;
            d.key = k;
            idx = out.size();
            indexOf.emplace(k, idx);
            handles.emplace(k, items[t]);
            out.push_back(std::move(d));
        } else
            idx = it->second;
        out[idx].occurrences++;
    }
    std::vector<dup_info> dupes;
    for (auto & d : out) {
        if (d.occurrences > 1) {
            d.header = track_header(handles[d.key]);
            dupes.push_back(std::move(d));
        }
    }
    out.swap(dupes);
}

static t_size find_or_create_playlist(const char *name) {
    static_api_ptr_t<playlist_manager> pm;
    t_size idx = pm->find_playlist(name, ~0);
    if (idx == ~0)
        idx = pm->create_playlist(name, ~0, ~0);
    return idx;
}

// Rebuilds a generated playlist from scratch: clears it, then fills it with
// the fresh result set (skipping tracks already... none survive a rebuild).
// Generated playlists are snapshots: rerunning refreshes them. Do not curate
// them by hand; anything added manually is wiped on the next run.
static t_size collect_into(const char *plname, const std::vector<metadb_handle_ptr> & tracks) {
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
    for (const auto & h : tracks) {
        if (seen.insert(track_key(h)).second)
            add.add_item(h);
    }
    if (add.get_count() > 0) {
        bit_array_none sel;
        pm->playlist_add_items(pl, add, sel);
    }
    pm->set_active_playlist(pl); // show the result
    return add.get_count();
}

static void scan_dead_tracks(std::vector<dead_entry> & out) {
    out.clear();
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
        if (is_managed_playlist(plname.c_str()))
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
}

static t_size apply_dead_removal(const std::vector<dead_entry> & dead, std::vector<std::string> & failedPlaylists) {
    failedPlaylists.clear();
    if (dead.empty())
        return 0;
    static_api_ptr_t<playlist_manager> pm;
    t_size total = 0;
    size_t i = 0;
    metadb_handle_list items;
    while (i < dead.size()) {
        const t_size pl = (t_size)dead[i].playlist;
        items.remove_all();
        pm->playlist_get_all_items(pl, items);
        bit_array_marks mask(items.get_count());
        t_size marked = 0;
        size_t j = i;
        while (j < dead.size() && dead[j].playlist == dead[i].playlist &&
               dead[j].playlistName == dead[i].playlistName) {
            if (dead[j].item < (size_t)items.get_count()) {
                if (!mask.m[dead[j].item]) {
                    mask.m[dead[j].item] = 1;
                    ++marked;
                }
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

static std::string utf8_from_wide(const wchar_t *w) {
    if (!w || !*w)
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1)
        return {};
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

// Folder picker. Returns false when cancelled.
static bool pick_folder(std::string & out) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(co);
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        if (uninit)
            CoUninitialize();
        return false;
    }

    bool ok = false;
    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts)))
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    if (SUCCEEDED(dlg->Show(core_api::get_main_window()))) {
        IShellItem *item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                out = utf8_from_wide(path);
                ok = !out.empty();
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    if (uninit)
        CoUninitialize();
    return ok;
}

// Format picker: 0 cancelled, 1 .m3u8, 2 .m3u.
static int pick_format() {
    const TASKDIALOG_BUTTON buttons[] = {
        {100, L"M3U8\nUTF-8 playlist, works with Rekordbox"},
        {101, L"M3U\nLegacy ANSI playlist"},
    };
    TASKDIALOGCONFIG cfg = {};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = core_api::get_main_window();
    cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    cfg.pszWindowTitle = L"Export All Playlists";
    cfg.pszMainInstruction = L"Choose playlist file format";
    cfg.cButtons = 2;
    cfg.pButtons = buttons;
    int pressed = 0;
    if (FAILED(TaskDialogIndirect(&cfg, &pressed, nullptr, nullptr)))
        return 1;
    if (pressed == 100)
        return 1;
    if (pressed == 101)
        return 2;
    return 0;
}

static void run_export_all_impl(pfc::string_base & msg) {
    std::string folder;
    if (!pick_folder(folder)) {
        msg << "Export cancelled.\n";
        return;
    }
    const int fmt = pick_format();
    if (fmt == 0) {
        msg << "Export cancelled.\n";
        return;
    }
    const char *ext = (fmt == 2) ? ".m3u" : ".m3u8";
    static_api_ptr_t<playlist_manager> pm;
    abort_callback_dummy abort;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname, path;
    t_size ok = 0, skipped = 0;
    pfc::string_formatter body;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        if (is_managed_playlist(plname.c_str())) {
            skipped++;
            continue;
        }
        path.reset();
        path << folder.c_str() << "\\" << safe_filename(plname.c_str()).c_str() << ext;
        try {
            playlist_loader::g_save_playlist(path.c_str(), items, abort);
            ok++;
        } catch (const std::exception & e) {
            body << "FAILED: " << plname.c_str() << " (" << e.what() << ")\n";
        } catch (...) {
            body << "FAILED: " << plname.c_str() << "\n";
        }
    }
    pfc::string_formatter head;
    head << "Exported " << ok << " of " << npl << " playlists (" << (ext + 1) << ") to " << folder.c_str() << "\n";
    if (skipped > 0)
        head << "(skipped " << skipped << " generated working lists)\n";
    head << body.c_str();
    msg << head.c_str();
}

static std::string prompt_save_playlist_path(const char *playlistName) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(co);
    std::string chosen;
    IFileSaveDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        if (uninit)
            CoUninitialize();
        return chosen;
    }

    const COMDLG_FILTERSPEC filters[] = {
        {L"M3U8 Playlist", L"*.m3u8"},
        {L"M3U Playlist", L"*.m3u"},
    };
    dlg->SetFileTypes(2, filters);
    dlg->SetFileTypeIndex(1);
    pfc::string_formatter sug;
    sug << safe_filename(playlistName).c_str() << ".m3u8";
    wchar_t wsug[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, sug.c_str(), -1, wsug, MAX_PATH);
    dlg->SetFileName(wsug);
    if (SUCCEEDED(dlg->Show(core_api::get_main_window()))) {
        IShellItem *item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                chosen = utf8_from_wide(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    if (uninit)
        CoUninitialize();
    return chosen;
}

static void run_export_active_impl(pfc::string_base & msg) {
    static_api_ptr_t<playlist_manager> pm;
    pfc::string_formatter plname;
    if (!pm->activeplaylist_get_name(plname)) {
        msg << "No active playlist.\n";
        return;
    }

    const std::string chosen = prompt_save_playlist_path(plname.c_str());
    if (chosen.empty()) {
        msg << "Export cancelled.\n";
        return;
    }

    metadb_handle_list items;
    pm->activeplaylist_get_all_items(items);
    abort_callback_dummy abort;
    try {
        playlist_loader::g_save_playlist(chosen.c_str(), items, abort);
        msg << "Exported '" << plname.c_str() << "' (" << items.get_count() << " tracks) to " << chosen.c_str() << "\n";
    } catch (const std::exception & e) {
        msg << "FAILED: '" << plname.c_str() << "' (" << e.what() << ")\n";
    } catch (...) {
        msg << "FAILED: '" << plname.c_str() << "'\n";
    }
}

// Jump to a track inside a playlist: activate it, select + focus the track.
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

// --- dynamic submenu -----------------------------------------------------

static const GUID guid_ctx_item = {0x3d058e3b, 0xc4eb, 0x4a64, {0x84, 0xc6, 0x09, 0xa7, 0xe8, 0x4e, 0x84, 0xfe}};

struct menu_entry {
    std::string label;
    std::string playlist; // set on leaves
    std::string key;      // track key, set on leaves
    bool isPopup = false;
    bool disabled = false;
    std::vector<menu_entry> children;
};

class node_leaf : public contextmenu_item_node_leaf {
  public:
    node_leaf(menu_entry e) : m_e(std::move(e)) {}
    bool get_display_data(pfc::string_base & out, unsigned & flags, metadb_handle_list_cref, const GUID &) override {
        out = m_e.label.c_str();
        flags = m_e.disabled ? FLAG_DISABLED : 0;
        return true;
    }
    void execute(metadb_handle_list_cref, const GUID &) override {
        if (!m_e.disabled && !m_e.playlist.empty())
            goto_track_in_playlist(m_e.playlist, m_e.key);
    }
    bool get_description(pfc::string_base &) override { return false; }
    GUID get_guid() override { return guid_ctx_item; }
    bool is_mappable_shortcut() override { return false; }

  private:
    menu_entry m_e;
};

class node_popup : public contextmenu_item_node_popup {
  public:
    node_popup(std::string label, std::vector<menu_entry> children)
        : m_label(std::move(label)), m_children(std::move(children)) {}
    bool get_display_data(pfc::string_base & out, unsigned & flags, metadb_handle_list_cref, const GUID &) override {
        out = m_label.c_str();
        flags = 0;
        return true;
    }
    t_size get_children_count() override { return m_children.size(); }
    contextmenu_item_node *get_child(t_size i) override {
        const menu_entry & e = m_children[i];
        if (e.isPopup)
            return new node_popup(e.label, e.children);
        return new node_leaf(e);
    }
    GUID get_guid() override { return guid_ctx_item; }
    bool is_mappable_shortcut() override { return false; }

  private:
    std::string m_label;
    std::vector<menu_entry> m_children;
};

class node_root : public contextmenu_item_node_root_popup {
  public:
    node_root(std::vector<menu_entry> children) : m_children(std::move(children)) {}
    bool get_display_data(pfc::string_base & out, unsigned & flags, metadb_handle_list_cref, const GUID &) override {
        out = "Find in Playlists";
        flags = 0;
        return true;
    }
    t_size get_children_count() override { return m_children.size(); }
    contextmenu_item_node *get_child(t_size i) override {
        const menu_entry & e = m_children[i];
        if (e.isPopup)
            return new node_popup(e.label, e.children);
        return new node_leaf(e);
    }
    GUID get_guid() override { return guid_ctx_item; }
    bool is_mappable_shortcut() override { return false; }

  private:
    std::vector<menu_entry> m_children;
};

class ctx_find_in_playlists : public contextmenu_item_v2 {
  public:
    unsigned get_num_items() override { return 1; }
    GUID get_item_guid(unsigned) override { return guid_ctx_item; }
    void get_item_name(unsigned, pfc::string_base & out) override { out = "Find in Playlists"; }
    bool get_item_description(unsigned, pfc::string_base & out) override {
        out = "Lists every playlist containing the selected track(s).";
        return true;
    }
    GUID get_parent() override { return contextmenu_groups::root; }
    t_enabled_state get_enabled_state(unsigned) override { return DEFAULT_ON; }
    contextmenu_item_node_root *instantiate_item(unsigned, metadb_handle_list_cref data, const GUID &) override {
        if (data.get_count() == 0) {
            std::vector<menu_entry> entries;
            entries.push_back(menu_entry{"(no tracks selected)"});
            entries.back().disabled = true;
            return new node_root(std::move(entries));
        }

        std::vector<track_report> reports;
        find_playlists_for(data, reports);
        const bool multi = reports.size() > 1;
        std::vector<menu_entry> entries;
        for (const auto & r : reports) {
            std::vector<menu_entry> kids;
            if (r.lists.empty()) {
                kids.push_back(menu_entry{"(not in any playlist)"});
                kids.back().disabled = true;
            } else {
                for (const auto & n : r.lists) {
                    menu_entry e;
                    e.label = n;
                    e.playlist = n;
                    e.key = r.key;
                    kids.push_back(std::move(e));
                }
            }
            if (multi) {
                menu_entry group;
                group.label = r.header;
                group.isPopup = true;
                group.children = std::move(kids);
                entries.push_back(std::move(group));
            } else {
                entries.insert(entries.end(), std::make_move_iterator(kids.begin()),
                               std::make_move_iterator(kids.end()));
            }
        }
        return new node_root(std::move(entries));
    }
    void item_execute_simple(unsigned, const GUID &, metadb_handle_list_cref data, const GUID &) override {
        // Shortcut invocation path (no menu): classic popup report.
        if (data.get_count() == 0) {
            popup_message::g_show("No tracks selected.", "Find in Playlists");
            return;
        }

        pfc::string_formatter msg;
        std::vector<track_report> reports;
        find_playlists_for(data, reports);
        format_report(reports, msg);
        popup_message::g_show(msg, "Find in Playlists");
    }
};

static contextmenu_item_factory_t<ctx_find_in_playlists> g_ctx_factory;

// --- main menu: library-wide commands (file-based, no popups) ------------

static const GUID guid_main_group = {0x23adac65, 0xf18a, 0x4e1c, {0xa2, 0xef, 0xc0, 0x3a, 0x75, 0x0d, 0x96, 0x88}};
static const GUID guid_main_cmd = {0xbc83531a, 0x186a, 0x4de2, {0x97, 0x31, 0x94, 0x19, 0x80, 0xdb, 0xad, 0x1d}};
static const GUID guid_cmd_dupes = {0x79ae2e61, 0x1513, 0x4aca, {0xb8, 0x36, 0x31, 0x64, 0xca, 0x9c, 0x65, 0xaa}};
static const GUID guid_cmd_orphans = {0x6de60df6, 0x68ca, 0x4b4b, {0x8c, 0x89, 0x41, 0x29, 0xd8, 0xdd, 0x9c, 0xca}};
static const GUID guid_cmd_active_dupes = {
    0x3dc564c7, 0x80fa, 0x4793, {0x87, 0x50, 0x34, 0xac, 0x47, 0xd2, 0x5a, 0x64}};
static const GUID guid_cmd_dead_dry = {0x7cc1f128, 0x3396, 0x4e00, {0x97, 0x58, 0x89, 0xbd, 0xc6, 0x89, 0x32, 0x48}};
static const GUID guid_cmd_dead = {0x20809f94, 0xef38, 0x470e, {0x81, 0xaa, 0x31, 0x7b, 0x5a, 0x19, 0x26, 0x26}};
static const GUID guid_cmd_export = {0x3e717dde, 0xf086, 0x4e77, {0xa4, 0x2f, 0xd2, 0xa3, 0x81, 0x6e, 0xb7, 0xdf}};
static const GUID guid_cmd_export_active = {
    0x98304d3d, 0x4404, 0x4819, {0x98, 0x44, 0xf3, 0xf0, 0xb4, 0x89, 0x0e, 0xe1}};

static mainmenu_group_popup_factory g_main_group(guid_main_group, mainmenu_groups::library,
                                                 mainmenu_commands::sort_priority_dontcare, "Find in Playlists");

class main_find_playing : public mainmenu_commands {
  public:
    enum {
        cmd_find = 0,
        cmd_dupes,
        cmd_orphans,
        cmd_active_dupes,
        cmd_dead_dry,
        cmd_dead,
        cmd_export,
        cmd_export_active,
        cmd_total
    };
    t_uint32 get_command_count() override { return cmd_total; }
    GUID get_command(t_uint32 idx) override {
        switch (idx) {
        case cmd_find:
            return guid_main_cmd;
        case cmd_dupes:
            return guid_cmd_dupes;
        case cmd_orphans:
            return guid_cmd_orphans;
        case cmd_active_dupes:
            return guid_cmd_active_dupes;
        case cmd_dead_dry:
            return guid_cmd_dead_dry;
        case cmd_dead:
            return guid_cmd_dead;
        case cmd_export:
            return guid_cmd_export;
        case cmd_export_active:
            return guid_cmd_export_active;
        default:
            uBugCheck();
        }
    }
    GUID get_parent() override { return guid_main_group; }
    void get_name(t_uint32 idx, pfc::string_base & out) override {
        switch (idx) {
        case cmd_find:
            out = "Find Playlists Containing Playing Track";
            break;
        case cmd_dupes:
            out = "Collect Potential Duplicates into Playlist";
            break;
        case cmd_orphans:
            out = "Collect Tracks Not in Playlists";
            break;
        case cmd_active_dupes:
            out = "Find Duplicates in Active Playlist";
            break;
        case cmd_dead_dry:
            out = "Find Dead Tracks in All Playlists";
            break;
        case cmd_dead:
            out = "Remove Dead Tracks from All Playlists";
            break;
        case cmd_export:
            out = "Export All Playlists to Folder...";
            break;
        case cmd_export_active:
            out = "Export Active Playlist to File...";
            break;
        default:
            uBugCheck();
        }
    }
    bool get_description(t_uint32 idx, pfc::string_base & out) override {
        switch (idx) {
        case cmd_find:
            out = "Writes the playlists holding the now-playing track to a report file.";
            return true;
        case cmd_dupes:
            out = "Collects tracks occurring 2+ times into a Potential Duplicates playlist.";
            return true;
        case cmd_orphans:
            out = "Collects library tracks sitting in no playlist into a Tracks Not in Playlists playlist.";
            return true;
        case cmd_active_dupes:
            out = "Lists tracks repeated inside the active playlist.";
            return true;
        case cmd_dead_dry:
            out = "Lists entries pointing at missing files, without changing anything.";
            return true;
        case cmd_dead:
            out = "Removes entries pointing at missing files from every playlist.";
            return true;
        case cmd_export:
            out = "Asks for a folder and format, then saves every playlist there.";
            return true;
        case cmd_export_active:
            out = "Asks for a file, then saves the active playlist to it.";
            return true;
        default:
            return false;
        }
    }
    void execute(t_uint32 idx, service_ptr_t<service_base>) override {
        switch (idx) {
        case cmd_find:
            run_find_playing();
            break;
        case cmd_dupes:
            run_collect_dupes();
            break;
        case cmd_orphans:
            run_collect_orphans();
            break;
        case cmd_active_dupes:
            run_active_dupes();
            break;
        case cmd_dead_dry:
            run_dead_dry();
            break;
        case cmd_dead:
            run_remove_dead();
            break;
        case cmd_export:
            run_export_all();
            break;
        case cmd_export_active:
            run_export_active();
            break;
        default:
            uBugCheck();
        }
    }

  private:
    static void report(const char *title, const pfc::string_base & msg) {
        const std::string path(write_report_file(title, msg));
        FB2K_console_formatter() << title << " (report: " << path.c_str() << ")";
    }
    // Visible confirmation for click-driven commands: counts in a popup,
    // full detail in the report file.
    static void confirm(const char *title, const pfc::string_base & summary) { popup_message::g_show(summary, title); }
    static void run_find_playing() {
        pfc::string_formatter msg;
        metadb_handle_ptr now;
        if (!static_api_ptr_t<playback_control>()->get_now_playing(now)) {
            msg << "Nothing playing.";
            report("Find in Playlists: playing track", msg);
            confirm("Find in Playlists", msg);
            return;
        }

        metadb_handle_list sel;
        sel.add_item(now);
        std::vector<track_report> reports;
        find_playlists_for(sel, reports);
        format_report(reports, msg);
        report("Find in Playlists: playing track", msg);
        confirm("Find in Playlists", msg);
    }
    static void run_collect_dupes() {
        std::vector<dup_info> dupes;
        std::unordered_map<std::string, metadb_handle_ptr> handles;
        scan_duplicates(dupes, handles);
        std::vector<metadb_handle_ptr> hh;
        pfc::string_formatter msg;
        msg << "Tracks occurring more than once: " << dupes.size() << "\n";
        for (const auto & d : dupes) {
            const auto it = handles.find(d.key);
            if (it != handles.end())
                hh.push_back(it->second);
            msg << d.header.c_str() << " (" << d.occurrences << "x: ";
            for (size_t i = 0; i < d.playlists.size(); ++i)
                msg << (i ? ", " : "") << d.playlists[i].c_str();
            msg << ")\n";
        }
        const t_size added = collect_into("Potential Duplicates", hh);
        pfc::string_formatter head;
        head << "Potential duplicates: " << dupes.size() << " repeated tracks, " << added
             << " added to 'Potential Duplicates'\n";
        head << msg.c_str();
        report("Find in Playlists: duplicates", head);
        pfc::string_formatter sum;
        sum << "Potential duplicates: " << dupes.size() << " repeated tracks, 'Potential Duplicates' rebuilt with "
            << added << ".\nFull list in the report file.";
        confirm("Find in Playlists", sum);
    }
    static void run_collect_orphans() {
        std::vector<dup_info> orphans;
        std::unordered_map<std::string, metadb_handle_ptr> handles;
        scan_orphans(orphans, handles);
        std::vector<metadb_handle_ptr> hh;
        for (const auto & d : orphans) {
            const auto it = handles.find(d.key);
            if (it != handles.end())
                hh.push_back(it->second);
        }
        const t_size added = collect_into("Tracks Not in Playlists", hh);
        pfc::string_formatter msg;
        msg << "Tracks not in playlists: " << orphans.size() << " library tracks in no playlist, " << added
            << " added to 'Tracks Not in Playlists'\n";
        for (const auto & d : orphans)
            msg << d.header.c_str() << "\n";
        report("Find in Playlists: orphans", msg);
        pfc::string_formatter sum;
        sum << "Tracks not in playlists: " << orphans.size() << ", 'Tracks Not in Playlists' rebuilt with " << added
            << ".\nFull list in the report file.";
        confirm("Find in Playlists", sum);
    }
    static void run_active_dupes() {
        std::vector<dup_info> dupes;
        std::string plname;
        scan_active_duplicates(dupes, plname);
        pfc::string_formatter msg;
        msg << "Repeated tracks in '" << plname.c_str() << "': " << dupes.size() << "\n";
        for (const auto & d : dupes)
            msg << d.header.c_str() << " (" << d.occurrences << "x)\n";
        report("Find in Playlists: active duplicates", msg);
        pfc::string_formatter sum;
        if (dupes.empty())
            sum << "No repeated tracks in '" << plname.c_str() << "'.";
        else
            sum << dupes.size() << " repeated tracks in '" << plname.c_str() << "'.\nFull list in the report file.";
        confirm("Find in Playlists", sum);
    }
    static void run_dead_dry() {
        std::vector<dead_entry> dead;
        scan_dead_tracks(dead);
        pfc::string_formatter msg;
        format_dead_report(dead, false, msg);
        report("Find in Playlists: dead tracks (dry run)", msg);
        pfc::string_formatter sum;
        if (dead.empty())
            sum << "No dead tracks found. Nothing to do.";
        else
            sum << "Dead tracks found: " << dead.size() << " entries.\nFull list in the report file.";
        confirm("Find in Playlists", sum);
    }
    static void run_remove_dead() {
        std::vector<dead_entry> dead;
        scan_dead_tracks(dead);
        std::vector<std::string> failed;
        const t_size removed = apply_dead_removal(dead, failed);
        pfc::string_formatter msg;
        format_dead_report(dead, true, failed, msg);
        report("Find in Playlists: dead tracks", msg);
        pfc::string_formatter sum;
        if (dead.empty())
            sum << "No dead tracks found. Nothing changed.";
        else
            sum << "Removed " << removed << " of " << dead.size() << " dead entries."
                << (failed.empty() ? "" : " Some removals FAILED, see report.") << "\nUndo with Ctrl+Z per playlist.";
        confirm("Find in Playlists", sum);
    }
    static void run_export_all() {
        pfc::string_formatter msg;
        run_export_all_impl(msg);
        report("Find in Playlists: export", msg);
        confirm("Find in Playlists", msg);
    }
    static void run_export_active() {
        pfc::string_formatter msg;
        run_export_active_impl(msg);
        report("Find in Playlists: export active", msg);
        confirm("Find in Playlists", msg);
    }
};

static mainmenu_commands_factory_t<main_find_playing> g_main_factory;

// --- Properties dialog: "Playlists" group --------------------------------
// Note: the SDK exposes rows in the existing Properties tab, not whole new
// tabs, so the report lands as rows under a "Playlists" group.

class playlist_property_provider : public track_property_provider_v5 {
  public:
    void enumerate_properties_v5(trackListRef items, track_property_provider_v5_info_source &,
                                 track_property_callback_v2 & cb, abort_callback &) override {
        if (!core_api::is_main_thread())
            return; // playlist APIs are main-thread only
        if (!cb.is_group_wanted("Playlists"))
            return;
        metadb_handle_list sel;
        for (size_t i = 0; i < items.get_size(); ++i)
            sel.add_item(items[i]);
        std::vector<track_report> reports;
        find_playlists_for(sel, reports);
        for (const auto & r : reports) {
            pfc::string_formatter count;
            count << r.lists.size();
            cb.set_property("Playlists", 0, (std::string("Playlists holding: ") + r.header).c_str(), count.c_str());
            for (const auto & n : r.lists)
                cb.set_property("Playlists", 0, n.c_str(), "");
        }
    }
    bool is_our_tech_info(const char *) override { return false; }
};

static track_property_provider_factory_t<playlist_property_provider> g_prop_factory;

} // namespace
