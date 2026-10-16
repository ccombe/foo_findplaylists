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

int main() {
    test_track_key();
    test_safe_filename();
    test_format_report();
    test_format_dead();
    test_write_report_file();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
