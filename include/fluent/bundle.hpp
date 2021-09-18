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

#ifndef BUNDLE_HPP_INCLUDED
#define BUNDLE_HPP_INCLUDED

#include <filesystem>
#include <optional>
#include <string>
#include <unicode/locid.h>
#include <unordered_map>

#include "ast.hpp"

namespace fluent {

class FluentBundle {
  private:
    // Mapping of message identifiers to Messages
    std::unordered_map<std::string, ast::Message> messages;
    // Mapping of term identifiers to Terms
    std::unordered_map<std::string, ast::Term> terms;

  public:
    void addMessage(ast::Message &&message);
    void addTerm(ast::Term &&term);
    std::optional<ast::Message> getMessage(const std::string &identifier) const;
    std::optional<ast::Term> getTerm(const std::string &identifier) const;
};

} // namespace fluent

#endif // BUNDLE_HPP_INCLUDED
