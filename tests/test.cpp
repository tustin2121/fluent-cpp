#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "parser.hpp"
    
namespace pt = boost::property_tree;

template<class> inline constexpr bool always_false_v = false;


void processEntry(pt::ptree &parent, fluent::ast::Entry &entry)
{
    std::visit([&parent](const auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, fluent::ast::Message>)
        {
            parent.push_back(std::make_pair("", arg.getPropertyTree()));
        }
        else if constexpr (std::is_same_v<T, fluent::ast::Comment>)
        {
            pt::ptree comment;
            comment.put("type", "Comment");
            comment.put("content", arg.value);
            parent.push_back(std::make_pair("", comment));
        }
        else if constexpr (std::is_same_v<T, fluent::ast::Junk>)
        {
            pt::ptree comment;
            comment.put("type", "Junk");
            comment.put("annotations", "");
            comment.put("content", arg.value);
            parent.push_back(std::make_pair("", comment));
        }
        else
            static_assert(always_false_v<T>, "non-exhaustive visitor!");
    }, entry);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: " << argv[0] << "<filename.ftl> <filename.json>" << std::endl; 
        return 2;
    }

    std::vector<fluent::ast::Entry> ftl = fluent::parse(argv[1]);
    pt::ptree expected;
    pt::read_json(argv[2], expected);

    pt::ptree actual, body;
    actual.put("type", "Resource");
    for (fluent::ast::Entry entry: ftl)
    {
        processEntry(body, entry); 
    }
    actual.add_child("body", body);
    if (actual != expected)
    {
        std::cout << "Result:" << std::endl;
        pt::write_json(std::cout, actual);
        std::cout << "Expected:" << std::endl;
        pt::write_json(std::cout, expected);
    }
    return actual != expected;
}
