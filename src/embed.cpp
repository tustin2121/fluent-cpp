#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << "<filename.ftl> <out.cpp>" << std::endl;
        return 2;
    }

    std::filesystem::create_directories(std::filesystem::path(argv[2]).parent_path());

    std::string localeName =
        std::filesystem::path(argv[1]).parent_path().stem().string();

    std::ifstream input(argv[1], std::ifstream::in);
    std::ofstream output(argv[2], std::ofstream::out);

    output << "#include <fluent/loader.hpp>" << std::endl;
    output << "#include <unicode/locid.h>" << std::endl;

    output << "static char ftl_data[] = {";

    char c = input.get();
    while (input.good()) {
        output << "0x" << std::hex << (int)c << ",";
        c = input.get();
    }
    output << 0 << "};" << std::endl;
    output << std::endl;
    output << "static bool _ = [](){ fluent::addStaticResource(icu::Locale(\""
           << localeName << "\"), std::string(ftl_data)); return true; }();"
           << std::endl;

    input.close();
    output.close();
}
