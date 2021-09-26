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
 *  \file ast.hpp
 *  \brief Elements of the AST used to store fluent resources in memory
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

/**
 *  \class VariableReference
 *  \brief A reference to a Variable within an expression.
 */
struct VariableReference {
    std::string identifier;
    VariableReference(std::string &&identifier) : identifier(std::move(identifier)) {}
};

/**
 *  \class MessageReference
 *  \brief A reference to a Message within an expression.
 */
struct MessageReference {
    std::string identifier;
    std::optional<std::string> attribute;
    MessageReference(std::string &&identifier, std::optional<std::string> &&attribute)
        : identifier(std::move(identifier)), attribute(std::move(attribute)) {}

#ifdef TEST
    virtual std::string getPropertyTreeType() const { return "MessageReference"; }
    virtual boost::property_tree::ptree getPropertyTree() const;
#endif
};

/**
 *  \class TermReference
 *  \brief A reference to a Term within an expression.
 */
struct TermReference : public MessageReference {
    using MessageReference::MessageReference;

#ifdef TEST
  public:
    boost::property_tree::ptree getPropertyTree() const override;
    std::string getPropertyTreeType() const override { return "TermReference"; }
#endif
};

/**
 * \class StringLiteral
 * \brief A string literal enclosed in an expression, often used for escaping values
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
/**
 *  \typedef Variable
 *  \brief data which may be passed as an argument when formatting messages.
 */
typedef std::variant<std::string> Variable;

/**
 * \class Comment
 * \brief Data stored in a comment within a fluent resource.
 *
 * This class only stores isolated comments.
 * Comments attached to messages are embedded in the Message.
 */
struct Comment {
    std::string value;

    Comment(std::string &&value) : value(std::move(value)) {}
};

/**
 * \class Junk
 * \brief Unparseable data in a fluent resource.
 */
struct Junk {
    std::string value;

    Junk(std::string &&value) : value(std::move(value)) {}
};

class Term;
class Message;
/**
 *  \class Attribute
 *  \brief A subentity within a Message or Term.
 *
 *  Attributes cannot have their own attributes, but are otherwise functionally the same
 *  as a Message.
 */
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

    const std::string format(
        const std::map<std::string, Variable> &args,
        const std::function<std::optional<Message>(const std::string &)> messageLookup,
        const std::function<std::optional<Term>(const std::string &)> termLookup) const;
};

/**
 *  \class Message
 *  \brief The core localisation unit of fluent.
 *
 *  A fluent resource file consists of a list of messages.
 *
 *  Each message has an identifier and a pattern, and may have additional attributes.
 */
class Message {
  protected:
    std::optional<std::string> comment;
    std::string id;
    std::vector<PatternElement> pattern;
    std::unordered_map<std::string, Attribute> attributes;

  public:
    inline void setComment(std::string &&comment) { this->comment = comment; }

#ifdef TEST
    virtual std::string getPropertyTreeType() const { return "Message"; }
    boost::property_tree::ptree getPropertyTree() const;
#endif
    inline const std::optional<Attribute>
    getAttribute(const std::string &identifier) const {
        auto iter = this->attributes.find(identifier);
        if (iter != this->attributes.end()) {
            return std::optional(iter->second);
        }
        return std::optional<Attribute>();
    }

    inline const std::string &getId() const { return this->id; }
    Message(std::string &&id, std::vector<Attribute> &&attributes);

    Message(std::string &&id, std::vector<PatternElement> &&pattern,
            std::vector<Attribute> &&attributes = std::vector<Attribute>(),
            std::optional<std::string> &&comment = std::optional<std::string>());

    const std::string format(
        const std::map<std::string, Variable> &args,
        const std::function<std::optional<Message>(const std::string &)> messageLookup,
        const std::function<std::optional<Term>(const std::string &)> termLookup) const;

    friend std::ostream &operator<<(std::ostream &out,
                                    const fluent::ast::Message &message);
};

/**
 *  \class Term
 *  \brief A Message for internal use within fluent resources
 *
 *  Terms when defined prefix their identifiers with ``-`` and can only be referenced
 *  within other terms and messages. They cannot be accessed through the
 *  fluent::FluentLoader
 */
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
