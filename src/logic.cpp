// See logic.h: pure logic only, no live foobar services.

#include "logic.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

std::string track_key_for(const char *path, unsigned subsong) {
    if (!path)
        path = "";
    const size_t len = strlen(path);
    std::string k;
    k.reserve(len + 12);
    k.append(path, len);
    k.push_back('\x1F');
    if (subsong == 0) {
        k.push_back('0');
    } else {
        k += std::to_string(subsong);
    }
    return k;
}

bool record_track_playlist_match(const std::string & key, const std::string & playlistName,
                                 const std::unordered_map<std::string, size_t> & indexOf,
                                 std::vector<track_report> & reports) {
    const auto it = indexOf.find(key);
    if (it == indexOf.end())
        return false;

    auto & lists = reports[it->second].lists;
    if (!lists.empty() && lists.back() == playlistName)
        return false;

    lists.push_back(playlistName);
    return true;
}

static void append_track_report_entry(pfc::string_base & f, const track_report & r) {
    f << r.header.c_str() << "\n";
    if (r.lists.empty()) {
        f << "  (not in any playlist)\n";
        return;
    }
    const size_t count = r.lists.size();
    f << "  Found in " << count << (count == 1 ? " playlist:\n" : " playlists:\n");
    for (const auto & n : r.lists)
        f << "  - " << n.c_str() << "\n";
}

void format_report(const std::vector<track_report> & reports, pfc::string_base & out) {
    if (reports.empty())
        return;

    for (const auto & r : reports)
        append_track_report_entry(out, r);
}

void format_dead_report(const std::vector<dead_entry> & dead, bool removed, pfc::string_base & out) {
    const std::vector<std::string> none;
    format_dead_report(dead, removed, none, out);
}

static void append_dead_entry_item(pfc::string_base & body, const dead_entry & entry) {
    body << "  - " << entry.header.c_str();
    if (!entry.path.empty())
        body << "\n      " << entry.path.c_str();
    body << "\n";
}

static void append_playlist_group_header(pfc::string_base & body, const std::string & cur, size_t count,
                                         const std::vector<std::string> & failedPlaylists) {
    const bool failed = std::find(failedPlaylists.begin(), failedPlaylists.end(), cur) != failedPlaylists.end();
    body << cur.c_str() << ": " << count << (failed ? " (REMOVAL FAILED, untouched)" : "") << "\n";
}

static void append_dead_summary_header(pfc::string_base & head, size_t deadCount, size_t affectedPlaylists,
                                       bool removed) {
    head << "Dead tracks " << (removed ? "removed" : "found") << ": " << deadCount << " entries across "
         << affectedPlaylists << " playlists.\n";
    head << (removed ? "(Undo per playlist with Ctrl+Z if anything looks wrong.)\n"
                     : "No changes made. Run Remove Dead Tracks to apply.\n");
}

void format_dead_report(const std::vector<dead_entry> & dead, bool removed,
                        const std::vector<std::string> & failedPlaylists, pfc::string_base & out) {
    if (dead.empty()) {
        out << "No dead tracks found. Nothing to do.\n";
        return;
    }

    // Pass 1: O(N) single forward pass to count distinct affected playlists
    size_t affected = 0;
    for (size_t i = 0; i < dead.size();) {
        affected++;
        const std::string & cur = dead[i].playlistName;
        size_t j = i + 1;
        while (j < dead.size() && dead[j].playlistName == cur)
            ++j;
        i = j;
    }

    append_dead_summary_header(out, dead.size(), affected, removed);

    // Pass 2: Stream group headers and items directly into out with O(1) group counts
    for (size_t i = 0; i < dead.size();) {
        const std::string & cur = dead[i].playlistName;
        size_t j = i + 1;
        while (j < dead.size() && dead[j].playlistName == cur)
            ++j;

        const size_t count = j - i;
        append_playlist_group_header(out, cur, count, failedPlaylists);
        for (size_t k = i; k < j; ++k)
            append_dead_entry_item(out, dead[k]);

        i = j;
    }
}

static bool is_forbidden_filename_char(char c) {
    return strchr("\\/:*?\"<>|", c) != nullptr;
}

static bool is_trailing_filename_char(char c) {
    return c == '.' || c == ' ';
}

std::string safe_filename(const char *name) {
    if (name == nullptr)
        return "playlist";

    std::string s(name);
    for (char & c : s) {
        if (is_forbidden_filename_char(c))
            c = '_';
    }
    while (!s.empty() && is_trailing_filename_char(s.back()))
        s.pop_back();

    if (s.empty())
        return "playlist";
    return s;
}

static std::string get_report_file_path() {
#ifdef _WIN32
    char tmp[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tmp);
    return std::string(tmp) + "foo_findplaylists_report.txt";
#else
    return "/tmp/foo_findplaylists_report.txt";
#endif
}

std::string write_report_file(const char *title, const pfc::string_base & body) {
    (void)title;
    const std::string path = get_report_file_path();
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return path;

    const char *text = body.c_str();
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    return path;
}
