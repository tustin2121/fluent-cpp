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
 *  \file loader.hpp
 *  \brief High-level storage and formatting of messages
 */

#ifndef LOADER_HPP_INCLUDED
#define LOADER_HPP_INCLUDED

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <unicode/locid.h>
#include <unordered_map>

#include "bundle.hpp"

namespace fluent {

/**
 * \class FluentLoader
 * \brief A high-level loader for storing and accessing fluent resources
 */
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
    /**
     * \brief Loads the fluent resource files contained in the given directory.
     *
     * These files should be stored in subdirectories with names equal to the locales
     * for the messages they contain.
     *
     * All ftl files within these directories will be loaded.
     *
     * E.g.
     *  - ${root}/en-GB/main.ftl
     *  - ${root}/en-US/main.ftl
     *
     *  \param dir: Directory to be processed.
     */
    void addDirectory(const std::string &dir);
    /**
     *  \overload  void addDirectory(const std::string &dir)
     *  \param dir: Directory to be processed.
     *  \param resources: A list of resources to be used. Only ftl files matching
     *                    these names will be loaded.
     */
    void addDirectory(const std::string &dir, const std::set<std::string> &resources);

    /**
     *  \brief Loads a single message
     *
     *  E.g. for the message
     *
     *  ``foo = Bar { $baz } baf``
     *
     *  you would call ``FluentLoader.addMessage(locId, "foo", "Bar { $baz } baf")``,
     *  for some ``locId``
     *
     *  \param locId: The locale of the message
     *  \param identifier: The identifier for the message to be loaded
     *  \param messageContents: The contents of the message as a raw string.
     */
    void addMessage(icu::Locale &locId, std::string &&identifier,
                    std::string &&messageContents);

    /**
     * \brief Formats a message
     *
     * \param locIdFallback: A list of locale identifiers listing the priority to use
     * when looking up messages. Generally it is expected that the source locale is the
     * last in this list. \param resId: The identifier for the message. This can include
     * attributes in the form "messageId.attributeId" \param args: A map of message
     * argument names to their values. The argument names should match the arguments
     * used by the message, and all arguments must be provided if the message uses them.
     *              Arguments which are not used will be ignored.
     */
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
