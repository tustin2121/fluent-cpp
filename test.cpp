#include <iostream>
#include "parser.hpp"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: " << argv[0] << "<filename>" << std::endl; 
        return 2;
    }

    std::vector<fluent::ast::Message> ftl = fluent::parse(argv[1]);

    for (fluent::ast::Message message: ftl)
    {
        std::cout << message << std::endl;
    }
}
