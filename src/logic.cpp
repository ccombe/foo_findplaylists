// See logic.h: pure logic only, no live foobar services.

#include "logic.h"

#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

std::string track_key_for(const char *path, unsigned subsong) {
    std::string k(path ? path : "");
    k.push_back('\x1F');
    k += std::to_string(subsong);
    return k;
}

void format_report(const std::vector<track_report> & reports, pfc::string_base & out) {
    pfc::string_formatter f;
    for (const auto & r : reports) {
        f << r.header.c_str() << "\n";
        if (r.lists.empty()) {
            f << "  (not in any playlist)\n";
        } else {
            f << "  Found in " << r.lists.size() << " playlist" << (r.lists.size() == 1 ? "" : "s") << ":\n";
            for (const auto & n : r.lists)
                f << "  - " << n.c_str() << "\n";
        }
    }
    out << f.c_str();
}

void format_dead_report(const std::vector<dead_entry> & dead, bool removed, pfc::string_base & out) {
    const std::vector<std::string> none;
    format_dead_report(dead, removed, none, out);
}

void format_dead_report(const std::vector<dead_entry> & dead, bool removed,
                        const std::vector<std::string> & failedPlaylists, pfc::string_base & out) {
    // Scan order is playlist by playlist, so groups stay together.
    pfc::string_formatter body;
    size_t affected = 0;
    std::string cur;
    for (size_t i = 0; i < dead.size(); ++i) {
        if (i == 0 || dead[i].playlistName != cur) {
            cur = dead[i].playlistName;
            affected++;
            size_t n = 0;
            for (size_t j = i; j < dead.size() && dead[j].playlistName == cur; ++j)
                n++;
            const bool failed = std::find(failedPlaylists.begin(), failedPlaylists.end(), cur) != failedPlaylists.end();
            body << cur.c_str() << ": " << n << (failed ? " (REMOVAL FAILED, untouched)" : "") << "\n";
        }
        body << "  - " << dead[i].header.c_str();
        if (!dead[i].path.empty())
            body << "\n      " << dead[i].path.c_str();
        body << "\n";
    }
    pfc::string_formatter head;
    if (dead.empty()) {
        head << "No dead tracks found. Nothing to do.\n";
    } else {
        head << "Dead tracks " << (removed ? "removed" : "found") << ": " << dead.size() << " entries across "
             << affected << " playlists.\n";
        head << (removed ? "(Undo per playlist with Ctrl+Z if anything looks wrong.)\n"
                         : "No changes made. Run Remove Dead Tracks to apply.\n");
    }
    head << body.c_str();
    out << head.c_str();
}

std::string safe_filename(const char *name) {
    std::string s(name ? name : "");
    for (char & c : s) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }
    while (!s.empty() && (s.back() == '.' || s.back() == ' '))
        s.pop_back();
    if (s.empty())
        s = "playlist";
    return s;
}

std::string write_report_file(const char *title, const pfc::string_base & body) {
    (void)title;
    std::string path;
#ifdef _WIN32
    char tmp[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tmp);
    path = std::string(tmp) + "foo_findplaylists_report.txt";
    if (FILE *f = fopen(path.c_str(), "w")) {
        const char *text = body.c_str();
        fwrite(text, 1, strlen(text), f);
        fclose(f);
    }
#else
    path = "/tmp/foo_findplaylists_report.txt";
    if (FILE *f = fopen(path.c_str(), "w")) {
        const char *text = body.c_str();
        fwrite(text, 1, strlen(text), f);
        fclose(f);
    }
#endif
    return path;
}
