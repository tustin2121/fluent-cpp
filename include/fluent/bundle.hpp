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
 *  \file bundle.hpp
 *  \brief Storage of messages and associated data for a specific locale
 */

#ifndef _FLUENT_BUNDLE_HPP_
#define _FLUENT_BUNDLE_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <unicode/locid.h>
#include <unordered_map>

#include "ast.hpp"

namespace fluent {

  /**
   *  \class FluentBundle
   *  \brief A class storing messages and associated data for a specific locale
   */
  class FluentBundle {
    private:
      // Mapping of message identifiers to Messages
      std::unordered_map<std::string, ast::Message> messages;
      // Mapping of term identifiers to Terms
      std::unordered_map<std::string, ast::Term> terms;

    public:
      /**
       * \brief Adds the given ast::Message to the bundle
       */
      void addMessage(ast::Message &&message);
      /**
       * \brief Adds the given ast::Term to the bundle
       */
      void addTerm(ast::Term &&term);
      /**
       * \brief Fetches an ast::Message from this bundle
       * \returns The ast::Message, or an empty optional if the ast::Message was not found
       */
      std::optional<ast::Message> getMessage(const std::string &identifier) const;
      /**
       * \brief Fetches an ast::Term from this bundle
       * \returns The ast::Term, or an empty optional if the ast::Term was not found
       */
      std::optional<ast::Term> getTerm(const std::string &identifier) const;
  };

};

#endif
