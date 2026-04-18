// Negative corpus — every must-reject entry must be rejected by the
// C++ oracle. Shares data with Python / TS via the on-disk index at
// spec/corpus/negative/index.json.
//
// The index is plain JSON (small enough to hand-parse) and the
// binary blobs are loaded from disk. A test fails if:
//   - an entry without `currently_accepted_by: ["cpp"]` decodes OK
//   - an entry *with* `currently_accepted_by: ["cpp"]` starts
//     getting rejected (time to remove the xfail annotation)
//
// We do not link a JSON library — the index is flat enough to parse
// with substring searches. If the shape grows, pull in nlohmann/json.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "oigtl/messages/register_all.hpp"
#include "oigtl/runtime/oracle.hpp"

namespace {

// Minimal parse of one entry block from the JSON index. Returns
// `true` on match and fills out the two fields we care about.
// Flaky JSON parsing is fine — the input is a generator-produced
// file with stable formatting.
struct ParsedEntry {
    std::string name;
    std::string path;
    bool accepted_by_cpp = false;
    std::string known_issue;
};

std::string extract_quoted(const std::string& block, const std::string& key) {
    const std::string needle = "\"" + key + "\": \"";
    auto pos = block.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    std::string out;
    while (pos < block.size() && block[pos] != '"') {
        if (block[pos] == '\\' && pos + 1 < block.size()) {
            out.push_back(block[pos + 1]);
            pos += 2;
        } else {
            out.push_back(block[pos++]);
        }
    }
    return out;
}

bool block_has_cpp_in_accepted(const std::string& block) {
    auto pos = block.find("\"currently_accepted_by\"");
    if (pos == std::string::npos) return false;
    auto end = block.find(']', pos);
    if (end == std::string::npos) return false;
    auto arr = block.substr(pos, end - pos);
    return arr.find("\"cpp\"") != std::string::npos;
}

std::vector<ParsedEntry> parse_index(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return {};
    }
    std::string text((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());

    std::vector<ParsedEntry> out;
    // Each entry is keyed by its name — "name_here": { ... }
    // The blocks are separated by "\n    },\n    \"next_name\":" so
    // we iterate over the top-level keys starting with 4 leading
    // spaces + quote.
    std::size_t i = 0;
    while (true) {
        auto key_start = text.find("\n    \"", i);
        if (key_start == std::string::npos) break;
        key_start += 6;  // skip "\n    \""
        auto key_end = text.find('"', key_start);
        if (key_end == std::string::npos) break;
        std::string name = text.substr(key_start, key_end - key_start);

        auto body_start = text.find('{', key_end);
        if (body_start == std::string::npos) break;
        // Match braces.
        int depth = 0;
        std::size_t body_end = body_start;
        for (; body_end < text.size(); ++body_end) {
            if (text[body_end] == '{') depth++;
            else if (text[body_end] == '}') {
                depth--;
                if (depth == 0) { body_end++; break; }
            }
        }
        std::string block = text.substr(body_start, body_end - body_start);

        ParsedEntry e;
        e.name = name;
        e.path = extract_quoted(block, "path");
        e.accepted_by_cpp = block_has_cpp_in_accepted(block);
        e.known_issue = extract_quoted(block, "known_issue");
        out.push_back(std::move(e));

        i = body_end;
    }
    return out;
}

std::vector<std::uint8_t> read_all(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<std::uint8_t> out(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    return out;
}

std::string find_repo_root(const std::string& start) {
    std::string p = start;
    for (int depth = 0; depth < 8; ++depth) {
        std::ifstream probe(p + "/spec/corpus/negative/index.json");
        if (probe) return p;
        // Accept both POSIX '/' and Windows '\\' path separators.
        // ctest on Windows may invoke us with a backslash-native
        // build-dir path.
        auto slash = p.find_last_of("/\\");
        if (slash == std::string::npos) break;
        p = p.substr(0, slash);
    }
    return {};
}

}  // namespace

int main() {
    // WORKING_DIRECTORY is set by CTest to the corpus-tools dir;
    // walk up to find spec/. std::filesystem::current_path() is
    // portable across POSIX and Windows (replacing getcwd from
    // <unistd.h>).
    std::error_code ec;
    const auto cwd_path = std::filesystem::current_path(ec);
    if (ec) {
        std::fprintf(stderr, "current_path failed: %s\n", ec.message().c_str());
        return 2;
    }
    const std::string cwd = cwd_path.generic_string();
    std::string root = find_repo_root(cwd);
    if (root.empty()) {
        std::fprintf(stderr, "repo root not found from %s\n", cwd.c_str());
        return 2;
    }
    const std::string neg_dir = root + "/spec/corpus/negative";
    const std::string index_path = neg_dir + "/index.json";
    auto entries = parse_index(index_path);
    if (entries.empty()) {
        std::fprintf(stderr, "no entries parsed from %s\n", index_path.c_str());
        return 1;
    }

    const auto registry = oigtl::messages::default_registry();

    int failures = 0;
    int xfail_unexpected_pass = 0;
    for (const auto& e : entries) {
        const auto data = read_all(neg_dir + "/" + e.path);
        const auto result = oigtl::runtime::oracle::verify_wire_bytes(
            data.data(), data.size(), registry, /*check_crc=*/true);
        const bool accepted = result.ok;
        const bool expected_accept = e.accepted_by_cpp;

        if (accepted && !expected_accept) {
            std::fprintf(stderr,
                         "FAIL  %s: expected rejection, oracle returned ok=true\n",
                         e.name.c_str());
            failures++;
        } else if (!accepted && expected_accept) {
            std::fprintf(stderr,
                         "FAIL  %s: xfail-annotated (cpp) but oracle now rejects "
                         "with %s. Remove \"cpp\" from currently_accepted_by.\n",
                         e.name.c_str(), result.error.c_str());
            xfail_unexpected_pass++;
        } else if (accepted) {
            std::fprintf(stdout, "  XF  %s (expected-accept)\n", e.name.c_str());
        } else {
            std::fprintf(stdout, "  OK  %s\n", e.name.c_str());
        }
    }
    if (failures || xfail_unexpected_pass) {
        std::fprintf(stderr, "\n%d rejection failures, %d unexpected passes\n",
                     failures, xfail_unexpected_pass);
        return 1;
    }
    std::fprintf(stdout, "\nAll %zu negative entries behave as declared.\n",
                 entries.size());
    return 0;
}
