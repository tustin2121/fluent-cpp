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

#include "fluent/loader.hpp"
#include "fluent/parser.hpp"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
using directory_entry = std::filesystem::recursive_directory_iterator;
using std::function;
using std::optional;
using std::string;
using std::filesystem::path;

namespace fluent {
template <class> inline constexpr bool always_false_v = false;

void FluentLoader::addResource(const icu::Locale locId, const path &ftlpath) {
    std::vector<ast::Entry> entries = parse(ftlpath.c_str());
    FluentBundle bundle;
    for (ast::Entry entry : entries) {
        std::visit(
            [&bundle](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ast::Message>)
                    bundle.addMessage(std::move(arg));
                else if constexpr (std::is_same_v<T, ast::Term>)
                    bundle.addTerm(std::move(arg));
                else if constexpr (std::is_same_v<T, ast::Comment>) {
                } else if constexpr (std::is_same_v<T, ast::Junk>) {
                } else {
                    static_assert(always_false_v<T>, "non-exhaustive visitor!");
                }
            },
            std::move(entry));
    }
    this->bundles.insert(std::make_pair(std::string(locId.getName()), bundle));
}

void FluentLoader::addDirectory(const string &dir) {
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

optional<ast::Message>
FluentLoader::getMessage(const std::vector<icu::Locale> &locIdFallback,
                         const string &resId) const {
    for (icu::Locale locId : locIdFallback) {
        auto result = this->bundles.find(string(locId.getName()));
        if (result != this->bundles.end()) {
            auto message = result->second.getMessage(resId);
            if (message)
                return message;
        }
    }
    return optional<ast::Message>();
}

optional<ast::Term> FluentLoader::getTerm(const std::vector<icu::Locale> &locIdFallback,
                                          const string &resId) const {
    for (icu::Locale locId : locIdFallback) {
        auto result = this->bundles.find(string(locId.getName()));
        if (result != this->bundles.end()) {
            auto term = result->second.getTerm(resId);
            if (term)
                return term;
        }
    }
    return optional<ast::Term>();
}

optional<string>
FluentLoader::formatMessage(const std::vector<icu::Locale> &locIdFallback,
                            const string &resId,
                            const std::map<string, ast::Variable> &args) const {
    function<optional<ast::Message>(const string &)> messageLookup =
        [&](const string &identifier) {
            return this->getMessage(locIdFallback, identifier);
        };
    function<optional<ast::Term>(const string &)> termLookup =
        [&](const string &identifier) {
            return this->getTerm(locIdFallback, identifier);
        };

    auto message = this->getMessage(locIdFallback, resId);
    if (message) {
        return optional(message->format(std::move(args), messageLookup, termLookup));
    }
    return optional<string>();
}

} // namespace fluent