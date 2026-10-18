// Unit tests for src/logic.* (pure logic, no live foobar services).
// Minimal assert runner: prints failures, nonzero exit on any failure.

#include "../src/logic.h"

#include <cstdio>
#include <string>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        ++g_checks;                                                                                                    \
        if (!(cond)) {                                                                                                 \
            ++g_failures;                                                                                              \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                     \
        }                                                                                                              \
    } while (0)
#define CHECK_STR_EQ(a, b)                                                                                             \
    do {                                                                                                               \
        ++g_checks;                                                                                                    \
        std::string _a(a), _b(b);                                                                                      \
        if (_a != _b) {                                                                                                \
            ++g_failures;                                                                                              \
            printf("FAIL %s:%d:\n  got:      [%s]\n  expected: [%s]\n", __FILE__, __LINE__, _a.c_str(), _b.c_str());   \
        }                                                                                                              \
    } while (0)

static std::string fmt_report(const std::vector<track_report> & r) {
    pfc::string_formatter f;
    format_report(r, f);
    return f.c_str();
}

static std::string fmt_dead(const std::vector<dead_entry> & d, bool removed) {
    pfc::string_formatter f;
    format_dead_report(d, removed, f);
    return f.c_str();
}

static void test_track_key() {
    CHECK(track_key_for("C:\\a\\b.mp3", 0) == track_key_for("C:\\a\\b.mp3", 0));
    CHECK(track_key_for("C:\\a\\b.mp3", 0) != track_key_for("C:\\a\\b.mp3", 1));
    CHECK(track_key_for("a.mp3", 0) != track_key_for("b.mp3", 0));
    CHECK(!track_key_for(nullptr, 0).empty());
}

static void test_safe_filename() {
    CHECK_STR_EQ(safe_filename("2025-03"), "2025-03");
    CHECK_STR_EQ(safe_filename("a/b\\c:d*e?f\"g<h>i|j"), "a_b_c_d_e_f_g_h_i_j");
    CHECK_STR_EQ(safe_filename("name...   "), "name");
    CHECK_STR_EQ(safe_filename(""), "playlist");
    CHECK_STR_EQ(safe_filename("..."), "playlist");
    CHECK_STR_EQ(safe_filename(nullptr), "playlist");
}

static void test_format_report() {
    track_report r;
    r.header = "Artist - Title";
    r.key = "k";
    r.lists = {"2025-03", "2025"};
    CHECK_STR_EQ(fmt_report({r}), "Artist - Title\n  Found in 2 playlists:\n  - 2025-03\n  - 2025\n");

    track_report solo = r;
    solo.lists = {"2025-03"};
    CHECK_STR_EQ(fmt_report({solo}), "Artist - Title\n  Found in 1 playlist:\n  - 2025-03\n");

    track_report none = r;
    none.lists.clear();
    CHECK_STR_EQ(fmt_report({none}), "Artist - Title\n  (not in any playlist)\n");

    CHECK_STR_EQ(fmt_report({}), "");
}

static void test_format_dead() {
    dead_entry a{0, "Mix A", 3, "Artist - One", "Z:\\path\\one.mp3"};
    dead_entry b{0, "Mix A", 7, "Artist - Two", "Z:\\path\\two.mp3"};
    dead_entry c{1, "Mix B", 0, "Artist - Three", "Z:\\path\\three.mp3"};
    CHECK_STR_EQ(fmt_dead({a, b, c}, false), "Dead tracks found: 3 entries across 2 playlists.\n"
                                             "No changes made. Run Remove Dead Tracks to apply.\n"
                                             "Mix A: 2\n  - Artist - One\n      Z:\\path\\one.mp3\n  - Artist - Two\n"
                                             "      Z:\\path\\two.mp3\n"
                                             "Mix B: 1\n  - Artist - Three\n      Z:\\path\\three.mp3\n");
    CHECK_STR_EQ(fmt_dead({a}, true), "Dead tracks removed: 1 entries across 1 playlists.\n"
                                      "(Undo per playlist with Ctrl+Z if anything looks wrong.)\n"
                                      "Mix A: 1\n  - Artist - One\n      Z:\\path\\one.mp3\n");
    CHECK_STR_EQ(fmt_dead({}, false), "No dead tracks found. Nothing to do.\n");
}

static void test_write_report_file() {
    pfc::string_formatter body;
    body << "hello";
    const std::string path(write_report_file("t", body));
    CHECK(path.size() > 0);
    CHECK(path.find("foo_findplaylists_report.txt") != std::string::npos);
    FILE *f = fopen(path.c_str(), "r");
    CHECK(f != nullptr);
    if (f) {
        char buf[16] = {};
        const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        CHECK(n == 5);
        CHECK_STR_EQ(buf, "hello");
    }
}

static void test_has_invalid_tag_chars() {
    CHECK(!has_invalid_tag_chars("Normal Artist"));
    CHECK(!has_invalid_tag_chars("Artist - Title"));
    CHECK(has_invalid_tag_chars("Artist < Name"));
    CHECK(has_invalid_tag_chars("Artist > Name"));
    CHECK(has_invalid_tag_chars("Artist : Name"));
    CHECK(has_invalid_tag_chars("Artist \" Name"));
    CHECK(has_invalid_tag_chars("Artist / Name"));
    CHECK(has_invalid_tag_chars("Artist \\ Name"));
    CHECK(has_invalid_tag_chars("Artist | Name"));
    CHECK(has_invalid_tag_chars("Artist ? Name"));
    CHECK(has_invalid_tag_chars("Artist * Name"));
    // Control characters are invalid
    CHECK(has_invalid_tag_chars("Artist\x01 Name"));
    CHECK(!has_invalid_tag_chars(""));
    CHECK(!has_invalid_tag_chars(nullptr));
}

static void test_is_music_file() {
    CHECK(is_music_file("song.mp3"));
    CHECK(is_music_file("song.flac"));
    CHECK(is_music_file("song.wav"));
    CHECK(is_music_file("song.m4a"));
    CHECK(is_music_file("song.ogg"));
    CHECK(is_music_file("song.wma"));
    CHECK(is_music_file("song.aiff"));
    CHECK(is_music_file("song.opus"));
    CHECK(!is_music_file("cover.jpg"));
    CHECK(!is_music_file("desktop.ini"));
    CHECK(!is_music_file("readme.txt"));
    CHECK(!is_music_file("song"));
    CHECK(!is_music_file(nullptr));
}

static void test_is_ignorable_file() {
    CHECK(is_ignorable_file("desktop.ini"));
    CHECK(is_ignorable_file(".DS_Store"));
    CHECK(is_ignorable_file("thumbs.db"));
    CHECK(is_ignorable_file("cover.jpg"));
    CHECK(is_ignorable_file("album.png"));
    CHECK(is_ignorable_file("readme.txt"));
    CHECK(is_ignorable_file("info.nfo"));
    CHECK(is_ignorable_file("playlist.m3u"));
    CHECK(is_ignorable_file("playlist.m3u8"));
    CHECK(!is_ignorable_file("song.mp3"));
    CHECK(!is_ignorable_file("song.flac"));
    CHECK(!is_ignorable_file("unknown.xyz"));
    CHECK(!is_ignorable_file(nullptr));
}

static void test_classify_directory() {
    // Empty directory -> empty_or_dead
    CHECK(classify_directory({}) == dir_status::empty_or_dead);
    // Only ignorable files -> empty_or_dead
    CHECK(classify_directory({"desktop.ini", ".DS_Store"}) == dir_status::empty_or_dead);
    // Contains music -> has_music
    CHECK(classify_directory({"song.mp3", "cover.jpg"}) == dir_status::has_music);
    // Contains useful non-music files -> has_useful_files
    CHECK(classify_directory({"readme.txt", "info.pdf"}) == dir_status::has_useful_files);
    // Music takes priority over useful
    CHECK(classify_directory({"song.flac", "readme.txt"}) == dir_status::has_music);
}

static void test_common_scan_roots() {
    // Deepest shared ancestor of sibling leaf dirs
    std::vector<std::string> r1 = common_scan_roots({"H:\\Music\\Rock\\Album A", "H:\\Music\\Pop\\Album B"});
    CHECK(r1.size() == 1);
    CHECK(r1[0] == "H:\\Music\\");

    // Mixed-case and forward slashes normalize to one volume
    std::vector<std::string> r2 = common_scan_roots({"h:/music/rock/album a", "H:\\MUSIC\\JAZZ\\"});
    CHECK(r2.size() == 1);
    for (auto & c : r2[0]) {
        if (c == '/')
            c = '\\';
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    CHECK(r2[0] == "h:\\music\\");

    // Two volumes -> one root per volume
    std::vector<std::string> r3 = common_scan_roots({"H:\\Music\\Rock", "I:\\Music\\Jazz"});
    CHECK(r3.size() == 2);

    // A dir directly in the volume -> root is the volume itself
    std::vector<std::string> r4 = common_scan_roots({"H:\\Music\\Rock", "H:\\Track.mp3"});
    CHECK(r4.size() == 1);
    CHECK(r4[0] == "H:\\");

    // Non-file paths are ignored
    CHECK(common_scan_roots({"http://server/stream"}).empty());
    CHECK(common_scan_roots({}).empty());
}

static void test_record_track_playlist_match() {
    std::unordered_map<std::string, size_t> indexOf;
    std::vector<track_report> reports;

    track_report r1;
    r1.header = "Artist 1 - Song 1";
    r1.key = "track_1";
    indexOf.emplace(r1.key, reports.size());
    reports.push_back(std::move(r1));

    track_report r2;
    r2.header = "Artist 2 - Song 2";
    r2.key = "track_2";
    indexOf.emplace(r2.key, reports.size());
    reports.push_back(std::move(r2));

    // Unknown track key: should return false and leave reports unchanged
    CHECK(!record_track_playlist_match("unknown_track", "Rock Hits", indexOf, reports));
    CHECK(reports[0].lists.empty());
    CHECK(reports[1].lists.empty());

    // First match for track 1 in playlist "Rock Hits": succeeds
    CHECK(record_track_playlist_match("track_1", "Rock Hits", indexOf, reports));
    CHECK(reports[0].lists.size() == 1);
    CHECK_STR_EQ(reports[0].lists[0], "Rock Hits");

    // Repeated occurrence in same playlist: should be deduplicated (return false)
    CHECK(!record_track_playlist_match("track_1", "Rock Hits", indexOf, reports));
    CHECK(reports[0].lists.size() == 1);

    // Track 1 in second playlist: succeeds
    CHECK(record_track_playlist_match("track_1", "Favorites", indexOf, reports));
    CHECK(reports[0].lists.size() == 2);
    CHECK_STR_EQ(reports[0].lists[1], "Favorites");

    // Track 2 in "Favorites": succeeds
    CHECK(record_track_playlist_match("track_2", "Favorites", indexOf, reports));
    CHECK(reports[1].lists.size() == 1);
    CHECK_STR_EQ(reports[1].lists[0], "Favorites");
}

static void test_extended_path() {
    // Normal drive paths get the extended-length prefix
    CHECK_STR_EQ(extended_path("G:\\My Drive\\Music"), "\\\\?\\G:\\My Drive\\Music");
    // Forward slashes become backslashes
    CHECK_STR_EQ(extended_path("g:/music"), "\\\\?\\g:\\music");
    // Already extended paths pass through untouched
    CHECK_STR_EQ(extended_path("\\\\?\\G:\\Music"), "\\\\?\\G:\\Music");
    // UNC shares use the UNC form
    CHECK_STR_EQ(extended_path("\\\\server\\share\\dir"), "\\\\?\\UNC\\server\\share\\dir");
    // Relative paths pass through
    CHECK_STR_EQ(extended_path("relative\\dir"), "relative\\dir");
}

static void test_derive_duplicates() {
    scan_result result;
    result.tracks = {
        {"k1", "Song A", {"List 1", "List 2"}, true},
        {"k2", "Song B", {"List 1"}, true},
        {"k3", "Song C", {"List 1", "List 2", "List 3"}, true},
        {"k4", "Song D", {}, true},
    };
    auto dupes = derive_duplicates(result);
    CHECK(dupes.size() == 2);
    CHECK(dupes[0].header == "Song A");
    CHECK(dupes[0].occurrences == 2);
    CHECK(dupes[1].header == "Song C");
    CHECK(dupes[1].occurrences == 3);
}

static void test_derive_orphans() {
    scan_result result;
    result.tracks = {
        {"k1", "Song A", {"List 1"}, true},
        {"k2", "Song B", {}, true},
    };
    std::unordered_set<std::string> library_keys = {"k1", "k2", "k3", "k4"};
    auto orphans = derive_orphans(result, library_keys);
    CHECK(orphans.size() == 2);
    CHECK(orphans[0].key == "k3" || orphans[0].key == "k4");
    CHECK(orphans[1].key == "k3" || orphans[1].key == "k4");
    CHECK(orphans[0].key != orphans[1].key);
    for (const auto & o : orphans)
        CHECK(o.occurrences == 0);
}

int main() {
    test_track_key();
    test_safe_filename();
    test_format_report();
    test_format_dead();
    test_write_report_file();
    test_record_track_playlist_match();
    test_has_invalid_tag_chars();
    test_is_music_file();
    test_is_ignorable_file();
    test_classify_directory();
    test_common_scan_roots();
    test_extended_path();
    test_derive_duplicates();
    test_derive_orphans();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
