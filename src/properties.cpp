#include "stdafx.h"
#include "scanner.h"
#include "logic.h"

// Properties dialog: "Playlists" group.
// Note: the SDK exposes rows in the existing Properties tab, not whole new
// tabs, so the report lands as rows under a "Playlists" group.

class playlist_property_provider : public track_property_provider_v5 {
  public:
    // v5 property properties are enumerated off the main thread in fb2k v2;
    // playlist APIs are main-thread only, so hop there synchronously.
    void enumerate_properties_v5(trackListRef items, track_property_provider_v5_info_source &,
                                 track_property_callback_v2 & cb, abort_callback & abort) override {
        if (!cb.is_group_wanted("Playlists"))
            return;
        metadb_handle_list sel;
        for (size_t i = 0; i < items.get_size(); ++i)
            sel.add_item(items[i]);
        std::vector<track_report> reports;
        if (core_api::is_main_thread()) {
            find_playlists_for(sel, reports);
        } else {
            fb2k::inMainThreadSynchronous([&] { find_playlists_for(sel, reports); }, abort);
        }
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
