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

#include <sstream>
#include "ast.hpp"

namespace fluent
{
namespace ast
{
template<class> inline constexpr bool always_false_v = false;

std::ostream& operator<< (std::ostream& out, const VariableReference &var)
{
    return out << "{ $" << var.variable << " }";
}

std::ostream& operator<< (std::ostream& out, const StringLiteral &literal)
{
    return out << "{ \"" << literal.value << "\" }";
}

std::ostream& operator<< (std::ostream& out, const PatternElement &elem)
{
    std::visit([&out](auto&& arg) { out << arg; }, elem);
    return out;
}

std::ostream& operator<< (std::ostream& out, const Message &message)
{
    out << message.id << " = ";
    for (fluent::ast::PatternElement value : message.pattern)
        out << value;

    return out;
}

std::ostream& operator<< (std::ostream& out, const Variable &var)
{
    std::visit([&out](auto&& arg) { out << arg; }, var);
    return out;
}
                

const std::string Message::format(const std::map<std::string, Variable> &args) const
{
    std::stringstream values;
    for (const PatternElement &elem : this->pattern)
    {
        std::visit([&values, &args](const auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>)
                values << arg;
            else if constexpr (std::is_same_v<T, StringLiteral>)
                values << arg.value;
            else if constexpr (std::is_same_v<T, VariableReference>)
                values << args.at(arg.variable);
            else
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
        }, elem);
    }
    return values.str();
}

} // namespace ast
} // namespace fluent
