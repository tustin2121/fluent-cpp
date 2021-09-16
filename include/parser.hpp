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

#ifndef PARSER_HPP_INCLUDED
#define PARSER_HPP_INCLUDED

#include "ast.hpp"
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fluent {
// TODO: parse_pattern for parsing individual message patterns

// Fixme: return a map instead of a vector
std::vector<ast::Entry> parse(const char *filename);
} // namespace fluent

#endif // PARSER_HPP_INCLUDED
