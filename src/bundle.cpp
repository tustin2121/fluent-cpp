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

namespace fluent {

    void FluentBundle::addMessage(ast::Message&& message) {
        this->messages.insert(std::make_pair(message.getId(), message));
    }

    void FluentBundle::addTerm(ast::Term&& term) {
        this->terms.insert(std::make_pair(term.getId(), term));
    }

    std::optional<ast::Message> FluentBundle::getMessage(const std::string& identifier) const {
        auto result = this->messages.find(identifier);
        if (result != this->messages.end()) {
            return std::optional(result->second);
        } else {
            return std::optional<ast::Message>();
        }
    }

    std::optional<ast::Term> FluentBundle::getTerm(const std::string& identifier) const {
        auto result = this->terms.find(identifier);
        if (result != this->terms.end()) {
            return std::optional(result->second);
        } else {
            return std::optional<ast::Term>();
        }
    }

} // namespace fluent
