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
    std::vector<ast::Entry> entries = parseFile(ftlpath);
    this->addResource(locId, std::move(entries));
}

void FluentLoader::addResource(const icu::Locale locId, std::string &&input) {
    std::vector<ast::Entry> entries = parse(std::move(input));
    this->addResource(locId, std::move(entries));
}

void FluentLoader::addResource(const icu::Locale locId,
                               std::vector<ast::Entry> &&entries) {
    // FIXME: Handle bundle already existing for this resource by merging with
    // existing bundle
    FluentBundle bundle;
    for (ast::Entry entry : entries) {
        std::visit(
            [&bundle](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ast::Message>)
                    bundle.addMessage(std::move(arg));
                else if constexpr (std::is_same_v<T, ast::Term>)
                    bundle.addTerm(std::move(arg));
                else if constexpr (std::is_same_v<T, ast::AnyComment>) {
                } else if constexpr (std::is_same_v<T, ast::Junk>) {
                } else {
                    static_assert(always_false_v<T>, "non-exhaustive visitor!");
                }
            },
            std::move(entry));
    }
    this->bundles.insert(std::make_pair(string(locId.getName()), bundle));
}

void FluentLoader::addMessage(icu::Locale &locId, string &&identifier,
                              string &&messageContents) {
    string localeName = string(locId.getName());
    optional<std::vector<ast::PatternElement>> pattern =
        parsePattern(std::move(messageContents));
    if (pattern) {
        ast::Message message(std::move(identifier), std::move(*pattern));
        auto iter = this->bundles.find(localeName);
        if (iter != this->bundles.end()) {
            iter->second.addMessage(std::move(message));
        } else {
            FluentBundle bundle;
            bundle.addMessage(std::move(message));
            this->bundles.insert(std::make_pair(localeName, bundle));
        }
    } else {
        throw std::runtime_error("Failed to parse message contents: " +
                                 messageContents);
    }
}

void FluentLoader::addDirectory(const string &dir) {
    for (const auto &dirEntry : recursive_directory_iterator(dir)) {
        if (dirEntry.is_regular_file()) {
            path file = dirEntry.path();
            if (file.extension() == ".ftl") {
                icu::Locale locId =
                    icu::Locale(file.parent_path().stem().string().c_str());
                this->addResource(locId, file);
            }
        }
    }
}

void FluentLoader::addDirectory(const std::string &dir,
                                const std::set<std::string> &resources) {
    for (const auto &dirEntry : recursive_directory_iterator(dir)) {
        if (dirEntry.is_regular_file()) {
            path file = dirEntry.path();
            if (file.extension() == ".ftl" &&
                resources.find(file.stem().string()) != resources.end()) {
                icu::Locale locId =
                    icu::Locale(file.parent_path().stem().string().c_str());
                this->addResource(locId, file);
            }
        }
    }
}

optional<std::pair<ast::Message, icu::Locale>>
FluentLoader::getMessage(const std::vector<icu::Locale> &locIdFallback,
                         const string &resId) const {
    for (icu::Locale locId : locIdFallback) {
        auto result = this->bundles.find(string(locId.getName()));
        if (result != this->bundles.end()) {
            auto message = result->second.getMessage(resId);
            if (message)
                return std::make_optional(std::make_pair(*message, locId));
        }
    }
    return optional<std::pair<ast::Message, icu::Locale>>();
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
            auto messagePair = this->getMessage(locIdFallback, identifier);
            if (messagePair)
                return std::make_optional(messagePair->first);
            return std::optional<ast::Message>();
        };
    function<optional<ast::Term>(const string &)> termLookup =
        [&](const string &identifier) {
            return this->getTerm(locIdFallback, identifier);
        };

    ast::MessageReference messageRef = parseMessageReference(resId);
    auto messagePair = this->getMessage(locIdFallback, messageRef.identifier);
    if (messagePair) {
        ast::Message message = messagePair->first;
        icu::Locale locid = messagePair->second;
        if (messageRef.attribute) {
            std::optional<ast::Attribute> attr =
                message.getAttribute(*messageRef.attribute);
            if (attr) {
                return optional(
                    attr->format(locid, std::move(args), messageLookup, termLookup));
            }
        } else {
            return optional(
                message.format(locid, std::move(args), messageLookup, termLookup));
        }
    }
    return optional<string>();
}

FluentLoader *STATIC_LOADER = nullptr;

void addStaticResource(const icu::Locale locId, std::string &&resource) {
    if (STATIC_LOADER == nullptr)
        STATIC_LOADER = new FluentLoader();
    STATIC_LOADER->addResource(locId, std::move(resource));
}

std::optional<std::string>
formatStaticMessage(const std::vector<icu::Locale> &locIdFallback,
                    const std::string &resId,
                    const std::map<std::string, fluent::ast::Variable> &args) {
    if (STATIC_LOADER == nullptr)
        return std::optional<std::string>();
    return STATIC_LOADER->formatMessage(locIdFallback, resId, args);
}

} // namespace fluent
