#include "parser.hpp"
#include "gtest/gtest.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <iostream>

namespace pt = boost::property_tree;
namespace fs = std::filesystem;

template <class> inline constexpr bool always_false_v = false;

void processEntry(pt::ptree &parent, fluent::ast::Entry &entry) {
    std::visit(
        [&parent](const auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, fluent::ast::Message>) {
                parent.push_back(std::make_pair("", arg.getPropertyTree()));
            } else if constexpr (std::is_same_v<T, fluent::ast::Comment>) {
                pt::ptree comment;
                comment.put("type", "Comment");
                comment.put("content", arg.value);
                parent.push_back(std::make_pair("", comment));
            } else if constexpr (std::is_same_v<T, fluent::ast::Junk>) {
                pt::ptree comment;
                comment.put("type", "Junk");
                comment.put("annotations", "");
                comment.put("content", arg.value);
                parent.push_back(std::make_pair("", comment));
            } else
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
        },
        entry);
}

class TestParser : public testing::TestWithParam<std::string> {};

namespace boost::property_tree {
void PrintTo(const pt::ptree &node, std::ostream *os) { pt::write_json(*os, node); }
} // namespace boost::property_tree

TEST_P(TestParser, ChecksParserOutput) {
    fs::path inputPath(GetParam());
    std::vector<fluent::ast::Entry> ftl = fluent::parse(GetParam().c_str());
    pt::ptree expected;
    pt::read_json(inputPath.replace_extension(".json").c_str(), expected);

    pt::ptree actual, body;
    actual.put("type", "Resource");
    for (fluent::ast::Entry entry : ftl) {
        processEntry(body, entry);
    }
    actual.add_child("body", body);
    EXPECT_EQ(actual, expected);
}

static std::vector<std::string> collect_test_files() {
    std::vector<std::string> results;

    for (const auto &dirEntry : fs::recursive_directory_iterator("fixtures")) {
        if (dirEntry.is_regular_file()) {
            fs::path file = dirEntry.path();
            if (file.extension() == ".ftl") {
                results.push_back(file.string());
            }
        }
    }
    return results;
}
INSTANTIATE_TEST_SUITE_P(ParserTests, TestParser,
                         testing::ValuesIn(collect_test_files()));
