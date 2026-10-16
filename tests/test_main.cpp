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
    dead_entry a{0, "Mix A", 3, "Artist - One"};
    dead_entry b{0, "Mix A", 7, "Artist - Two"};
    dead_entry c{1, "Mix B", 0, "Artist - Three"};
    CHECK_STR_EQ(fmt_dead({a, b, c}, false), "Dead tracks found: 3 entries across 2 playlists.\n"
                                             "No changes made. Run Remove Dead Tracks to apply.\n"
                                             "Mix A: 2\n  - Artist - One\n  - Artist - Two\n"
                                             "Mix B: 1\n  - Artist - Three\n");
    CHECK_STR_EQ(fmt_dead({a}, true), "Dead tracks removed: 1 entries across 1 playlists.\n"
                                      "(Undo per playlist with Ctrl+Z if anything looks wrong.)\n"
                                      "Mix A: 1\n  - Artist - One\n");
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

int main() {
    test_track_key();
    test_safe_filename();
    test_format_report();
    test_format_dead();
    test_write_report_file();
    test_record_track_playlist_match();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
