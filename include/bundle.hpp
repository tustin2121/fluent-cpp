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

#include <string>
#include <unordered_map>
#include <optional>
#include <unicode/locid.h>
#include <filesystem>

#include "ast.hpp"

namespace fluent
{
class FluentBundle
{
private:
    /// Mapping of Locale identifiers to mappings of message identifiers to messages
    std::unordered_map<std::string, std::unordered_map<std::string, fluent::ast::Message>> data;
    void addResource(const icu::Locale locId, const std::filesystem::path &ftlpath);
public:
    void addDirectory(const std::string &dir);
    
    std::optional<std::string> formatMsg(
        const std::vector<icu::Locale> &locIdFallback,
        const std::string &resId,
        const std::map<std::string, fluent::ast::Variable> &args
    ) const;
};

} // namespace fluent

#endif // BUNDLE_HPP_INCLUDED
