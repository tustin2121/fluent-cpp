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

#ifndef LOADER_HPP_INCLUDED
#define LOADER_HPP_INCLUDED

#include <filesystem>
#include <optional>
#include <string>
#include <unicode/locid.h>
#include <unordered_map>

#include "bundle.hpp"

namespace fluent {

class FluentLoader {
  private:
    /// Mapping of Locale identifiers to the bundle for that locale.
    std::unordered_map<std::string, FluentBundle> bundles;
    void addResource(const icu::Locale locId, const std::filesystem::path &ftlpath);
    void addResource(const icu::Locale locId, std::vector<ast::Entry> &&entries);
    void addResource(const icu::Locale locId, std::string &&input);

    std::optional<ast::Message>
    getMessage(const std::vector<icu::Locale> &locIdFallback,
               const std::string &resId) const;

    std::optional<ast::Term> getTerm(const std::vector<icu::Locale> &locIdFallback,
                                     const std::string &resId) const;

  public:
    void addDirectory(const std::string &dir);

    std::optional<std::string>
    formatMessage(const std::vector<icu::Locale> &locIdFallback,
                  const std::string &resId,
                  const std::map<std::string, fluent::ast::Variable> &args) const;

    friend void addStaticResource(const icu::Locale locId, std::string &&resource);
};

/**
 * \brief Adds resource to the static loader.
 *
 * This is generally expected to be used with the ftlembed program, and shouldn't
 * need to be called manually.
 */
void addStaticResource(const icu::Locale locId, std::string &&resource);

/**
 * \brief Formats a message from the static loader.
 *
 * As FluentLoader.formatMessage, but uses a static shared loader initialized
 * via ftlembed and addStaticResource
 */
std::optional<std::string>
formatStaticMessage(const std::vector<icu::Locale> &locIdFallback,
                    const std::string &resId,
                    const std::map<std::string, fluent::ast::Variable> &args);

} // namespace fluent

#endif // LOADER_HPP_INCLUDED
