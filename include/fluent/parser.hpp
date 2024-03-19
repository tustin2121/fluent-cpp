/*
 *  This file is part of fluent-cpp.
 *
 *  Copyright (C) 2021 Benjamin Winger
 *
 *  fluent-cpp is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  fluent-cpp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fluent-cpp.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  \file parser.hpp
 *  \brief Parsing of fluent resources from raw data
 */

#ifndef _FLUENT_PARSER_HPP_
#define _FLUENT_PARSER_HPP_

#include "fluent/ast.hpp"
#include <filesystem>
#include <vector>

namespace fluent {
    // Fixme: return a map instead of a vector
    std::vector<ast::Entry> parseFile(const std::filesystem::path& file, bool strict = false);
    std::vector<ast::Entry> parse(std::string&& contents, bool strict = false);
    std::vector<ast::PatternElement> parsePattern(const std::string& input);
    ast::MessageReference parseMessageReference(const std::string& input);
} // namespace fluent

#endif
