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

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#ifdef TEST
#include <boost/property_tree/ptree.hpp>
#endif
#include <unicode/locid.h>

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

/**
 * \class NumberLiteral
 * \brief A numeric decimal literal enclosed in an expression.
 *
 * The number format will be localised during formatting.
 *
 * The number of significant figures will be preserved in NumberLiterals
 *
 * E.g.
 * - ``{ -3.14 }``
 * - ``{ 100 }``
 */
struct NumberLiteral {
    std::string value;

    NumberLiteral(std::string &&value) : value(std::move(value)) {}

    /**
     * \brief Localises number literal
     */
    const std::string format() const;

    bool operator==(const NumberLiteral &other) const {
        return *this == stod(other.value);
    }

    bool operator==(const double &other) const {
        double a = stod(this->value), b = other;
        return std::abs(a - b) <=
               std::abs(std::min(a, b)) * std::numeric_limits<double>::epsilon();
    }

    bool operator==(const long &other) const {
        return *this == static_cast<double>(other);
    }

    std::variant<long, double> getValue() const {
        if (this->value.find(".") != std::string::npos) {
            return std::variant<long, double>(stod(value));
        } else {
            return std::variant<long, double>(stol(value));
        }
    }
};

/**
 *  \typedef Variable
 *  \brief data which may be passed as an argument when formatting messages.
 */
typedef std::variant<std::string, long, double> Variable;
typedef std::variant<std::string, NumberLiteral> VariantKey;

/**
 * \class SelectExpression
 * \brief An expression matching against some input
 *
 * E.g. ``{ $value ->
 *   [0] No things
 *   [1] One thing
 *   *[other] Some things
 * }``
 */
struct SelectExpression {
    typedef std::variant<std::string, StringLiteral, NumberLiteral, VariableReference,
                         MessageReference, TermReference, SelectExpression>
        PatternElement;

    // Note: this only stores one, but is easier to work with than unique_ptr
    // It also allows it to be treated as a Pattern
    std::vector<PatternElement> selector;
    typedef std::pair<VariantKey, std::vector<PatternElement>> VariantType;
    // Note Stored internally as a vector since there are usually only a small number of
    // variants and we would prefer to preserve the original ordering.
    std::vector<VariantType> variants;
    size_t defaultVariant;

    SelectExpression(PatternElement &&selector,
                     std::vector<VariantType> &&firstVariants,
                     VariantType &&defaultVariant,
                     std::vector<VariantType> &&lastVariants)
        : variants(std::move(firstVariants)) {
        this->selector.push_back(std::move(selector));
        this->defaultVariant = variants.size();
        this->variants.push_back(std::move(defaultVariant));
        this->variants.insert(this->variants.end(), lastVariants.begin(),
                              lastVariants.end());
    }

    const std::vector<PatternElement> &find(const icu::Locale &locid,
                                            const std::string key) const;
    const std::vector<PatternElement> &find(const icu::Locale &locid,
                                            const double key) const;
    const std::vector<PatternElement> &find(const icu::Locale &locid,
                                            const long key) const;

#ifdef TEST
    boost::property_tree::ptree getPropertyTree() const;
#endif
};

typedef std::variant<std::string, StringLiteral, NumberLiteral, VariableReference,
                     MessageReference, TermReference, SelectExpression>
    PatternElement;

/**
 * \class Comment
 * \brief Data stored in a comment within a fluent resource.
 *
 * This class only stores isolated comments.
 * Comments attached to messages are embedded in the Message.
 */
struct Comment {
    std::vector<std::string> value;

    std::string getValue() const;

    Comment(std::vector<std::string> &&value) : value(std::move(value)) {}
};

/**
 * \class GroupComment
 * \brief Data stored in a comment heading a group of messages
 *
 * GroupComments are comments which start with a ##
 */
struct GroupComment : public Comment {
    using Comment::Comment;
};

/**
 * \class ResourceComment
 * \brief Data stored in a comment heading a resource file
 *
 * ResourceComments are comments which start with ###
 */
struct ResourceComment : public Comment {
    using Comment::Comment;
};

typedef std::variant<ast::Comment, ast::GroupComment, ast::ResourceComment> AnyComment;

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
        const icu::Locale &locId, const std::map<std::string, Variable> &args,
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
    std::optional<Comment> comment;
    std::string id;
    std::vector<PatternElement> pattern;
    std::unordered_map<std::string, Attribute> attributes;

  public:
    inline void setComment(Comment &&comment) { this->comment = comment; }

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
            std::optional<Comment> &&comment = std::optional<Comment>());

    const std::string format(
        const icu::Locale &locId, const std::map<std::string, Variable> &args,
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

typedef std::variant<AnyComment, Message, Term, Junk> Entry;

#ifdef TEST
void processEntry(boost::property_tree::ptree &parent, fluent::ast::Entry &entry);
#endif

} // namespace ast
} // namespace fluent

#endif // FLUENT_HPP_INCLUDED
