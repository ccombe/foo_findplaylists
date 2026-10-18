// See logic.h: pure logic only, no live foobar services.

#include "logic.h"

#include <algorithm>
#include <cctype>
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

bool has_invalid_tag_chars(const char *field) {
    if (!field)
        return false;
    static const char *invalidChars = "<>:\"/\\|?*";
    for (const char *p = field; *p; ++p) {
        if (static_cast<unsigned char>(*p) < 0x20) // control characters
            return true;
        if (strchr(invalidChars, *p))
            return true;
    }
    return false;
}

// Deepest shared ancestor (per drive volume) of the given directory paths:
// the top folder to scan so empty sibling folders in the watched tree are found.
// Returns one root per volume that actually holds library directories.
std::vector<std::string> common_scan_roots(const std::unordered_set<std::string> & dirPaths) {
    auto toNorm = [](std::string s) {
        for (auto & c : s) {
            if (c == '/')
                c = '\\';
            else
                c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };

    // volume ("g:\") -> (component lists, one original-case dir to cut the root from)
    struct vol {
        std::vector<std::vector<std::string>> comps;
        const std::string *sample = nullptr;
    };
    std::unordered_map<std::string, vol> byVolume;
    for (const auto & raw : dirPaths) {
        const std::string norm = toNorm(raw);
        const size_t colon = norm.find(":\\");
        if (colon == std::string::npos) // not a file path (network, stream...)
            continue;
        const std::string volume = norm.substr(0, colon + 2);
        std::vector<std::string> parts;
        size_t i = volume.size();
        while (i < norm.size()) {
            size_t next = norm.find('\\', i);
            if (next == std::string::npos)
                next = norm.size();
            if (i < next) // ignore empty segments (e.g. doubleslash)
                parts.push_back(norm.substr(i, next - i));
            i = next + 1;
        }
        auto & v = byVolume[volume];
        v.comps.push_back(std::move(parts));
        v.sample = &raw;
    }

    std::vector<std::string> roots;
    roots.reserve(byVolume.size());
    for (auto & [volume, v] : byVolume) {
        size_t rootLen = volume.size(); // original-case root cut from the sample dir
        for (size_t i = 0; i < v.comps[0].size(); ++i) {
            const auto & part = v.comps[0][i];
            bool shared = true;
            for (const auto & c : v.comps)
                if (i >= c.size() || c[i] != part)
                    shared = false;
            if (!shared)
                break;
            rootLen += part.size() + 1; // component + separator
        }
        roots.push_back(v.sample->substr(0, rootLen));
    }
    return roots;
}

bool record_track_playlist_match(const std::string & key, const std::string & playlistName,

bool record_track_playlist_match(const std::string & key, const std::string & playlistName,

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

bool is_music_file(const char *filename) {
    if (!filename)
        return false;
    const char *dot = strrchr(filename, '.');
    if (!dot)
        return false;
    static const char *musicExts[] = {".mp3", ".flac", ".wav", ".m4a",  ".aac",  ".ogg", ".wma", ".aiff", ".aif",
                                      ".ape", ".wv",   ".mpc", ".opus", ".dsf",  ".dff", ".mp4", ".m4v",  ".mkv",
                                      ".avi", ".wmv",  ".mov", ".mpg",  ".mpeg", ".ts",  ".webm"};
    const size_t numExts = sizeof(musicExts) / sizeof(musicExts[0]);
    for (size_t i = 0; i < numExts; ++i) {
        if (stricmp(dot, musicExts[i]) == 0)
            return true;
    }
    return false;
}

bool is_ignorable_file(const char *filename) {
    if (!filename)
        return false;
    const char *base = filename;
    const char *slash = strrchr(filename, '\\');
    if (slash)
        base = slash + 1;
    slash = strrchr(base, '/');
    if (slash)
        base = slash + 1;

    static const char *ignorableNames[] = {"desktop.ini", ".ds_store", "thumbs.db", ".nfo", ".sfv", ".m3u",  ".m3u8",
                                           ".pls",        ".cue",      ".log",      ".txt", ".jpg", ".jpeg", ".png",
                                           ".gif",        ".bmp",      ".tbn",      ".nfo", ".srt", ".sub",  ".idx",
                                           ".zip",        ".rar",      ".7z",       ".tar", ".gz",  ".bz2",  ".xz",
                                           ".html",       ".htm",      ".url",      ".lnk", ".db",  ".ini"};
    const size_t numNames = sizeof(ignorableNames) / sizeof(ignorableNames[0]);
    for (size_t i = 0; i < numNames; ++i) {
        if (stricmp(base, ignorableNames[i]) == 0)
            return true;
    }
    // Also ignorable if extension matches ignorable extensions
    static const char *ignorableExts[] = {".jpg", ".jpeg", ".png", ".gif",  ".bmp", ".tbn",     ".nfo",  ".srt",
                                          ".sub", ".idx",  ".log", ".txt",  ".sfv", ".m3u",     ".m3u8", ".pls",
                                          ".cue", ".db",   ".ini", ".html", ".htm", ".url",     ".zip",  ".rar",
                                          ".7z",  ".tar",  ".gz",  ".bz2",  ".xz",  ".ds_store"};
    const char *dot = strrchr(base, '.');
    if (dot) {
        const size_t numIgExts = sizeof(ignorableExts) / sizeof(ignorableExts[0]);
        for (size_t i = 0; i < numIgExts; ++i) {
            if (stricmp(dot, ignorableExts[i]) == 0)
                return true;
        }
    }
    return false;
}

dir_status classify_directory(const std::vector<std::string> & filenames) {
    bool hasMusic = false;
    bool hasUseful = false;
    bool hasIgnorable = false;
    for (const auto & name : filenames) {
        if (is_music_file(name.c_str()))
            hasMusic = true;
        else if (is_ignorable_file(name.c_str()))
            hasIgnorable = true;
        else
            hasUseful = true;
    }
    if (hasMusic)
        return dir_status::has_music;
    if (hasUseful)
        return dir_status::has_useful_files;
    return dir_status::empty_or_dead;
}

// Derive tracks appearing 2+ times from a unified scan result.
std::vector<dup_info> derive_duplicates(const scan_result & result) {
    std::vector<dup_info> dupes;
    for (const auto & t : result.tracks) {
        if (t.playlists.size() > 1) {
            dup_info d;
            d.header = t.header;
            d.key = t.key;
            d.occurrences = t.playlists.size();
            d.playlists = t.playlists;
            dupes.push_back(std::move(d));
        }
    }
    return dupes;
}

// Derive library tracks sitting in no playlist at all.
// library_keys: all track keys in the media library.
// result: scan result containing tracks found in playlists.
std::vector<dup_info> derive_orphans(const scan_result & result, const std::unordered_set<std::string> & library_keys) {
    std::vector<dup_info> orphans;

    // Build set of track keys present in at least one playlist.
    std::unordered_set<std::string> placed;
    for (const auto & t : result.tracks)
        placed.insert(t.key);

    for (const auto & k : library_keys) {
        if (placed.count(k))
            continue;

        dup_info d;
        d.header = k; // caller will replace with real header
        d.key = k;
        d.occurrences = 0;
        orphans.push_back(std::move(d));
    }
    return orphans;
}
