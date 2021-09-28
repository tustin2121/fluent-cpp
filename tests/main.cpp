#include "fluent/parser.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>

namespace pt = boost::property_tree;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << "<filename.ftl>" << std::endl;
        return 2;
    }

    std::vector<fluent::ast::Entry> ftl = fluent::parseFile(argv[1]);

    pt::ptree actual, body;
    actual.put("type", "Resource");
    for (fluent::ast::Entry entry : ftl) {
        fluent::ast::processEntry(body, entry);
    }
    actual.add_child("body", body);
    pt::write_json(std::cout, actual);
}
