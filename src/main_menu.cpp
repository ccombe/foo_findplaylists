#include "stdafx.h"
#include "scanner.h"
#include "playlist_ops.h"
#include "export.h"
#include "logic.h"

// Command GUIDs.
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
static const GUID guid_cmd_dead_dirs = {0xb1c2d3e4, 0xf5a6, 0x7890, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1}};
static const GUID guid_cmd_remove_dead_dirs = {
    0xc2d3e4f5, 0xa6b7, 0x8901, {0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01}};
static const GUID guid_cmd_invalid_tags = {
    0xa1b2c3d4, 0xe5f6, 0x7890, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}};

static const GUID guid_main_group = {0x23adac65, 0xf18a, 0x4e1c, {0xa2, 0xef, 0xc0, 0x3a, 0x75, 0x0d, 0x96, 0x88}};

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
        cmd_invalid_tags,
        cmd_dead_dirs,
        cmd_remove_dead_dirs,
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
        case cmd_invalid_tags:
            return guid_cmd_invalid_tags;
        case cmd_dead_dirs:
            return guid_cmd_dead_dirs;
        case cmd_remove_dead_dirs:
            return guid_cmd_remove_dead_dirs;
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
        case cmd_invalid_tags:
            out = "Scan Invalid Tags";
            break;
        case cmd_dead_dirs:
            out = "Find Dead/Empty Directories";
            break;
        case cmd_remove_dead_dirs:
            out = "Remove Dead/Empty Directories";
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
        case cmd_invalid_tags:
            out = "Finds tracks with invalid filename characters in tags and collects them into an Invalid Tags "
                  "playlist.";
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
        case cmd_invalid_tags:
            run_invalid_tags();
            break;
        case cmd_dead_dirs:
            run_dead_dirs();
            break;
        case cmd_remove_dead_dirs:
            run_remove_dead_dirs();
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
        auto result = scan_playlists();
        auto dupes = derive_duplicates(result);
        std::vector<metadb_handle_ptr> handles;
        static_api_ptr_t<playlist_manager> pm;
        metadb_handle_list items;
        for (const auto & d : dupes) {
            for (const auto & pl : d.playlists) {
                t_size plIdx = pm->find_playlist(pl.c_str());
                if (plIdx == ~0)
                    continue;
                items.remove_all();
                pm->playlist_get_all_items(plIdx, items);
                for (t_size t = 0; t < items.get_count(); ++t) {
                    if (track_key(items[t]) == d.key) {
                        handles.push_back(items[t]);
                        break;
                    }
                }
                if (!handles.empty())
                    break;
            }
        }
        const t_size added = collect_into("Potential Duplicates", handles);
        pfc::string_formatter msg;
        msg << "Tracks occurring more than once: " << dupes.size() << "\n";
        for (const auto & d : dupes) {
            msg << d.header.c_str() << " (" << d.occurrences << "x: ";
            for (size_t i = 0; i < d.playlists.size(); ++i)
                msg << (i ? ", " : "") << d.playlists[i].c_str();
            msg << ")\n";
        }
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
        auto result = scan_playlists();
        static_api_ptr_t<library_manager> lm;
        metadb_handle_list all;
        lm->get_all_items(all);
        std::unordered_set<std::string> library_keys;
        for (t_size t = 0; t < all.get_count(); ++t)
            library_keys.insert(track_key(all[t]));
        auto orphans = derive_orphans(result, library_keys);
        std::vector<metadb_handle_ptr> handles;
        for (const auto & d : orphans) {
            for (t_size t = 0; t < all.get_count(); ++t) {
                if (track_key(all[t]) == d.key) {
                    handles.push_back(all[t]);
                    break;
                }
            }
        }
        const t_size added = collect_into("Tracks Not in Playlists", handles);
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
        std::string plname;
        auto dupes = scan_active_duplicates(plname);
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
        auto dead = scan_dead_tracks();
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
        auto dead = scan_dead_tracks();
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

    static void run_dead_dirs() {
        auto dirs = scan_dead_directories();
        pfc::string_formatter msg;
        msg << "Dead/empty directories found: " << dirs.size() << "\n";
        for (const auto & d : dirs)
            msg << d.c_str() << "\n";
        report("Find in Playlists: dead directories", msg);
        pfc::string_formatter sum;
        if (dirs.empty())
            sum << "No dead/empty directories found.";
        else
            sum << dirs.size() << " dead/empty directories found.\nFull list in the report file.";
        confirm("Find in Playlists", sum);
    }

    static void run_remove_dead_dirs() {
        auto dirs = scan_dead_directories();
        if (dirs.empty()) {
            pfc::string_formatter _sum;
            _sum << "No dead/empty directories found.";
            confirm("Find in Playlists", _sum);
            return;
        }
        pfc::string_formatter msg;
        msg << "Dead/empty directories to remove: " << dirs.size() << "\n";
        for (const auto & d : dirs)
            msg << d.c_str() << "\n";
        report("Find in Playlists: dead directories (removal)", msg);

        // Remove each directory recursively.
        t_size removed = 0;
        t_size failed = 0;
        for (const auto & dir : dirs) {
            // Delete all files in the directory first. std::filesystem is
            // used because FindFirstFileA parses [abc] in the path as a
            // wildcard pattern, so folder names like "[unnamed]" fail.
            namespace fs = std::filesystem;
            std::error_code ec;
            for (fs::directory_iterator it(fs::path(dir), fs::directory_options::skip_permission_denied, ec), end;
                 it != end && !ec; it.increment(ec)) {
                std::string name;
                try {
                    name = it->path().filename().string();
                } catch (const std::system_error &) {
                    continue; // undecodable name: skip the entry
                }
                const std::string fullPath = dir + std::string("\\") + name;
                if (it->is_directory(ec)) {
                    if (ec)
                        break;
                    RemoveDirectoryA(fullPath.c_str());
                } else if (!ec) {
                    DeleteFileA(fullPath.c_str());
                }
            }
            ec.clear();
            if (RemoveDirectoryA(dir.c_str()))
                ++removed;
            else
                ++failed;
        }

        pfc::string_formatter sum;
        sum << "Removed " << removed << " of " << dirs.size() << " directories.";
        if (failed > 0)
            sum << " " << failed << " failed (see report).";
        confirm("Find in Playlists", sum);
    }

    static void run_export_active() {
        pfc::string_formatter msg;
        run_export_active_impl(msg);
        report("Find in Playlists: export active", msg);
        confirm("Find in Playlists", msg);
    }

    static void run_invalid_tags() {
        auto invalid = scan_invalid_tags();
        std::vector<metadb_handle_ptr> handles;
        static_api_ptr_t<playlist_manager> pm;
        metadb_handle_list items;
        for (const auto & d : invalid) {
            t_size plIdx = d.playlist;
            items.remove_all();
            pm->playlist_get_all_items(plIdx, items);
            if (d.item < items.get_count())
                handles.push_back(items[d.item]);
        }
        const t_size added = collect_into("Invalid Tags", handles);
        pfc::string_formatter msg;
        msg << "Tracks with invalid tag characters: " << invalid.size() << "\n";
        for (const auto & d : invalid)
            msg << d.header.c_str() << " (in '" << d.playlistName.c_str() << "')\n";
        pfc::string_formatter head;
        head << "Invalid tags: " << invalid.size() << " tracks with invalid characters, " << added
             << " added to 'Invalid Tags'\n";
        head << msg.c_str();
        report("Find in Playlists: invalid tags", head);
        pfc::string_formatter sum;
        if (invalid.empty())
            sum << "No tracks with invalid tag characters found.";
        else
            sum << invalid.size() << " tracks with invalid tag characters, 'Invalid Tags' rebuilt with " << added
                << ".\nFull list in the report file.";
        confirm("Find in Playlists", sum);
    }
};

static mainmenu_commands_factory_t<main_find_playing> g_main_factory;
