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

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#ifdef TEST
#include <boost/property_tree/ptree.hpp>
#endif

#include <iostream>

namespace fluent {
namespace ast {
struct VariableReference {
    std::string identifier;
    VariableReference(std::string &&identifier) : identifier(std::move(identifier)) {}
};

struct MessageReference {
    std::string identifier;
    MessageReference(std::string &&identifier) : identifier(std::move(identifier)) {}
};

struct TermReference {
    std::string identifier;
    TermReference(std::string &&identifier) : identifier(std::move(identifier)) {}
};

/**
 * A string literal enclosed in an expression, often used for escaping values
 *
 * E.g. { "{" }
 */
struct StringLiteral {
    std::string value;

    StringLiteral(std::string &&value) : value(std::move(value)) {}
};

typedef std::variant<std::string, StringLiteral, VariableReference, MessageReference,
                     TermReference>
    PatternElement;
typedef std::variant<std::string> Variable;

struct Comment {
    std::string value;

    Comment(std::string &&value) : value(std::move(value)) {}
};

struct Junk {
    std::string value;

    Junk(std::string &&value) : value(std::move(value)) {}
};

struct Attribute {
  private:
    std::string id;
    std::vector<PatternElement> pattern;

  public:
#ifdef TEST
    boost::property_tree::ptree getPropertyTree() const;
#endif

    inline const std::string &getId() const { return this->id; }

    Attribute(std::string &&id, std::vector<PatternElement> &&pattern);
};

class Term;
class Message {
  protected:
    std::optional<std::string> comment;
    std::string id;
    std::vector<PatternElement> pattern;
    std::unordered_map<std::string, Attribute> attributes;

  public:
#ifdef TEST
    virtual std::string getPropertyTreeType() const { return "Message"; }
    boost::property_tree::ptree getPropertyTree() const;
#endif

    inline const std::string &getId() const { return this->id; }
    Message(std::optional<std::string> &&comment, std::string &&id,
            std::vector<PatternElement> &&pattern, std::vector<Attribute> &&attributes);

    const std::string format(
        const std::map<std::string, Variable> &args,
        const std::function<std::optional<Message>(const std::string &)> messageLookup,
        const std::function<std::optional<Term>(const std::string &)> termLookup) const;

    friend std::ostream &operator<<(std::ostream &out,
                                    const fluent::ast::Message &message);
};

class Term : public Message {
    using Message::Message;

  public:
#ifdef TEST
    std::string getPropertyTreeType() const override { return "Term"; }
#endif
};

typedef std::variant<Comment, Message, Term, Junk> Entry;

} // namespace ast
} // namespace fluent

#endif // FLUENT_HPP_INCLUDED
