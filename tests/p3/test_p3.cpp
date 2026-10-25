// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- your tests

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("first")));
    REQUIRE(r.add(std::make_unique<NamedTool>("second")));
    REQUIRE(r.add(std::make_unique<NamedTool>("third")));

    std::vector<std::string> seen;
    r.for_each([&](const Tool& t) { seen.push_back(t.name()); });
    CHECK(seen.size() == 3);
    CHECK(std::count(seen.begin(), seen.end(), "first") == 1);
    CHECK(std::count(seen.begin(), seen.end(), "second") == 1);
    CHECK(std::count(seen.begin(), seen.end(), "third") == 1);
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    std::vector<std::unique_ptr<Tool>> tools;
    tools.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        tools.push_back(std::make_unique<NamedTool>("tool_" + std::to_string(i)));
        REQUIRE(r.add(std::move(tools.back())));
    }

    std::printf("bucket_count=%zu load_factor=%.3f max_probe_length=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
    CHECK(r.size() == 10000);
    CHECK(r.load_factor() <= 1.0);
    CHECK(r.max_probe_length() <= 64);
    CHECK(r.find("tool_1234") != nullptr);
    CHECK(r.find("tool_9999") != nullptr);
}

TEST(reg_alternative_hash) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_0") != ToolRegistry::hash("tool_9999"));
    CHECK(ToolRegistry::hash("abc") != ToolRegistry::hash("abcd"));
}

TEST(ws_empty_path_rejected) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_absolute_path) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_accepts_normalized_relative_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");
    CHECK(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());
}

TEST(ws_rejects_parent_escape_after_normalization) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!ws.resolve("notes/../../ws/notes/todo.txt", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_accepts_dot_and_double_dots_in_name) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());
    CHECK(ws.resolve("my..notes.txt", &out, &reason));
    CHECK(out == ws.root() / "my..notes.txt");
}

TEST(ws_rejects_nul_in_path) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    std::string bad = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(bad, &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_left_to_right_for_equal_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}

TEST(calc_parentheses_and_negatives) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
}

TEST(calc_division_by_zero_message) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_rejects_mismatched_parentheses) {
    double v = 0;
    std::string err;
    CHECK(!eval("(1 + 2", &v, &err));
    CHECK(!err.empty());
}

TEST(calc_handles_large_expression) {
    std::string expr;
    expr.reserve(20000);
    for (int i = 0; i < 20000; ++i) expr += "1 + ";
    expr += "1";

    double v = 0;
    std::string err;
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 20001.0);
}

TEST(calc_nested_parentheses) {
    std::string expr;
    for (int i = 0; i < 5000; ++i) expr += "(";
    expr += "1";
    for (int i = 0; i < 5000; ++i) expr += ")";

    double v = 0;
    std::string err;
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 1.0);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
