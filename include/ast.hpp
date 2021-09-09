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

#ifndef AST_HPP_INCLUDED
#define AST_HPP_INCLUDED

#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <map>

#include <iostream>

namespace fluent
{
namespace ast
{
struct VariableReference
{
    std::string variable;

    VariableReference(std::string &&variable): variable(std::move(variable)) {}
};

/**
 * A string literal enclosed in an expression, often used for escaping values
 *
 * E.g. { "{" }
 */
struct StringLiteral
{
    std::string value;

    StringLiteral(std::string &&value): value(std::move(value)) {}
};

using PatternElement = std::variant<std::string, StringLiteral, VariableReference>;
using Variable = std::variant<std::string>;

class Message
{
private:
    std::string id;
    std::vector<PatternElement> pattern;

public:
    Message(std::string &&id, std::vector<PatternElement> &&pattern): id(std::move(id)), pattern(std::move(pattern)) {}
    std::string format(std::map<std::string, Variable> &&args);

    friend std::ostream& operator<< (std::ostream &out, const fluent::ast::Message &message);
};

} // namespace ast
} // namespace fluent


#endif // FLUENT_HPP_INCLUDED
