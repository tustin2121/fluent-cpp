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

#include "fluent/bundle.hpp"
#include "fluent/parser.hpp"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
using directory_entry = std::filesystem::recursive_directory_iterator;
using path = std::filesystem::path;

namespace fluent {
template <class> inline constexpr bool always_false_v = false;

void FluentBundle::addResource(const icu::Locale locId, const path &ftlpath) {
    std::vector<ast::Entry> entries = parse(ftlpath.c_str());
    std::vector<ast::Message> messages;
    for (ast::Entry entry : entries) {
        std::visit(
            [&messages](const auto &arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ast::Message>)
                    messages.push_back(arg);
                else if constexpr (std::is_same_v<T, ast::Comment>) {
                } else if constexpr (std::is_same_v<T, ast::Junk>) {
                } else {
                    static_assert(always_false_v<T>, "non-exhaustive visitor!");
                }
            },
            entry);
    }
    std::unordered_map<std::string, ast::Message> messageMap;
    for (ast::Message msg : messages)
        messageMap.insert(std::make_pair(msg.getId(), msg));
    this->data.insert(std::make_pair(std::string(locId.getName()), messageMap));
}

void FluentBundle::addDirectory(const std::string &dir) {
    for (const auto &dirEntry : recursive_directory_iterator(dir)) {
        if (dirEntry.is_regular_file()) {
            path file = dirEntry.path();
            if (file.extension() == ".ftl") {
                icu::Locale locId =
                    icu::Locale::createFromName(file.parent_path().stem().c_str());
                this->addResource(locId, file);
            }
        }
    }
}

std::optional<std::string> FluentBundle::formatMsg(
    const std::vector<icu::Locale> &locIdFallback, const std::string &resId,
    const std::map<std::string, fluent::ast::Variable> &args) const {
    for (icu::Locale locId : locIdFallback) {
        auto result = this->data.find(std::string(locId.getName()));
        if (result != this->data.end()) {
            auto innerResult = result->second.find(resId);
            if (innerResult != result->second.end())
                return std::optional(innerResult->second.format(std::move(args)));
        }
    }
    return std::optional<std::string>();
}

} // namespace fluent
