// foo_findplaylists - reverse playlist lookup for foobar2000.
//
// Right-click track(s) -> "Find in Playlists" submenu lists every playlist
// holding them; picking one jumps to the track in that playlist.
// Same data also appears as a "Playlists" group in the Properties dialog,
// and Library -> "Find in Playlists" submenu holds library-wide commands
// (all file-based, no popups).

#include "stdafx.h"
#include "logic.h"
#include "scanner.h"
#include "version.h"

DECLARE_COMPONENT_VERSION(
    "Find in Playlists", FIP_VERSION,
    "Created by Chris Combe. Last updated 2026-10-18.\n"
    "\n"
    "Reverse playlist lookup for foobar2000: see which playlists contain a track and jump to it.\n"
    "\n"
    "Where to find the features:\n"
    "- Right-click track(s): \"Find in Playlists\" lists every playlist holding them; picking one jumps to the "
    "track. Playlists also show in the Properties dialog as a \"Playlists\" group.\n"
    "- Library menu > Find in Playlists:\n"
    "  * Find Playlists Containing Playing Track\n"
    "  * Collect Potential Duplicates into Playlist\n"
    "  * Collect Tracks Not in Playlists\n"
    "  * Find Duplicates in Active Playlist\n"
    "  * Find / Remove Dead Tracks in All Playlists\n"
    "  * Scan Invalid Tags\n"
    "  * Find / Remove Dead/Empty Directories\n"
    "  * Export All Playlists to Folder... (m3u / m3u8)\n"
    "  * Export Active Playlist to File...\n"
    "\n"
    "All commands are file-based and report to a text file under %TEMP% instead of popups.");
VALIDATE_COMPONENT_FILENAME("foo_findplaylists.dll");
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;

namespace {

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
        std::vector<menu_entry> entries;
        if (data.get_count() == 0) {
            entries.push_back(menu_entry{"(no tracks selected)"});
            entries.back().disabled = true;
            return new node_root(std::move(entries));
        }

        std::vector<track_report> reports;
        find_playlists_for(data, reports);
        const bool multi = reports.size() > 1;
        std::vector<menu_entry> entries2;
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
                entries2.push_back(std::move(group));
            } else {
                entries2.insert(entries2.end(), std::make_move_iterator(kids.begin()),
                                std::make_move_iterator(kids.end()));
            }
        }
        return new node_root(std::move(entries2));
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

} // namespace
