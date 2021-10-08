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

#include "fluent/ast.hpp"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unicode/errorcode.h>
#include <unicode/numberformatter.h>

namespace fluent {
namespace ast {
template <class> inline constexpr bool always_false_v = false;

std::ostream &operator<<(std::ostream &out, const VariableReference &var) {
    return out << "{ $" << var.identifier << " }";
}

std::ostream &operator<<(std::ostream &out, const MessageReference &var) {
    return out << "{ " << var.identifier << " }";
}

std::ostream &operator<<(std::ostream &out, const TermReference &var) {
    return out << "{ -" << var.identifier << " }";
}

std::ostream &operator<<(std::ostream &out, const StringLiteral &literal) {
    return out << "{ \"" << literal.value << "\" }";
}

std::ostream &operator<<(std::ostream &out, const PatternElement &elem) {
    std::visit([&out](auto &&arg) { out << arg; }, elem);
    return out;
}

std::ostream &operator<<(std::ostream &out, const Message &message) {
    out << message.id << " = ";
    for (fluent::ast::PatternElement value : message.pattern)
        out << value;

    return out;
}

std::ostream &operator<<(std::ostream &out, const Variable &var) {
    std::visit([&out](auto &&arg) { out << arg; }, var);
    return out;
}

std::string Comment::getValue() const {
    std::stringstream result;
    for (int i = 0; i < this->value.size(); i++) {
        result << this->value[i];
        if (i != this->value.size() - 1)
            result << std::endl;
    }
    return result.str();
}

// The first text element should be stripped to remove leading whitespace
std::string stripStart(const std::string &str, const std::string &ws = " \r\n") {
    const auto begin = str.find_first_not_of(ws);
    if (begin == std::string::npos)
        return "";

    return str.substr(begin, std::string::npos);
}

std::string stripEnd(const std::string &str, const std::string &ws = " \r\n") {
    const auto end = str.find_last_not_of(ws);
    return str.substr(0, end + 1);
}

std::string replaceNewlines(std::string &str) {
    size_t pos;
    // DOS newlines should be replaced with Unix newlines
    while ((pos = str.find("\r\n")) != std::string::npos) {
        str.replace(pos, 2, "\n");
    }
    return str;
}

/**
 * Finds minimum indent (on lines with something other than whitespace)
 */
size_t getMinIndent(const std::string &value) {
    std::stringstream ss(value);
    std::string line;
    size_t minIndent = std::numeric_limits<size_t>::max();
    while (std::getline(ss, line, '\n')) {
        size_t startpos = line.find_first_not_of("\r\n ");
        // Ignore lines which only contain whitespace
        if (startpos != std::string::npos && startpos < minIndent)
            minIndent = startpos;
    }
    return minIndent;
}

/**
 * Removes minimum indent from all lines
 */
std::string stripIndent(const std::string value, const size_t indent) {
    std::stringstream ss(value);
    std::stringstream result;
    std::string line;
    while (std::getline(ss, line, '\n')) {
        size_t startpos = line.find_first_not_of("\r\n ");
        // Clear lines which only contain whitespace
        if (startpos == std::string::npos) {
            line = "";
        } else if (startpos > 0) {
            line.replace(0, std::min(indent, startpos), "");
        }
        result << line << '\n';
    }

    return result.str();
}

void addPattern(std::vector<PatternElement> &&newPattern,
                std::vector<PatternElement> &existingPattern) {
    bool lastString = false;
    std::string last;
    size_t minIndent = std::numeric_limits<size_t>::max();
    for (ast::PatternElement elem : newPattern) {
        std::visit(
            [&](const auto &arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    size_t indent = getMinIndent(arg);
                    if (indent < minIndent)
                        minIndent = indent;
                }
            },
            elem);
    }
    // Only strip leading whitespace from the first text element
    bool first = true;
    for (ast::PatternElement elem : newPattern) {
        std::visit(
            [&](const auto &arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    // If the previous element was a string, merge with it
                    last += arg;
                    if (!lastString) {
                        lastString = true;
                        last = arg;
                    }
                } else {
                    if (lastString) {
                        if (first) {
                            existingPattern.push_back(PatternElement(stripStart(
                                stripIndent(replaceNewlines(last), minIndent))));
                            first = false;
                        } else {
                            existingPattern.push_back(PatternElement(
                                stripIndent(replaceNewlines(last), minIndent)));
                        }
                        last = std::string();
                        lastString = false;
                    }
                    existingPattern.push_back(elem);
                }
            },
            elem);
    }
    // Only strip trailing whitespace from the last text element
    if (lastString) {
        if (first)
            existingPattern.push_back(PatternElement(
                stripStart(stripEnd(stripIndent(replaceNewlines(last), minIndent)))));
        else
            existingPattern.push_back(PatternElement(
                stripEnd(stripIndent(replaceNewlines(last), minIndent))));
    }
}

Attribute::Attribute(std::string &&id, std::vector<PatternElement> &&pattern)
    : id(std::move(id)) {
    addPattern(std::move(pattern), this->pattern);
}

Message::Message(std::string &&id, std::vector<Attribute> &&attributes)
    : Message(std::move(id), std::vector<PatternElement>(), std::move(attributes)) {}

Message::Message(std::string &&id, std::vector<PatternElement> &&pattern,
                 std::vector<Attribute> &&attributes, std::optional<Comment> &&comment)
    : comment(std::move(comment)), id(std::move(id)) {
    addPattern(std::move(pattern), this->pattern);
    for (ast::Attribute &attribute : attributes) {
        this->attributes.insert(std::make_pair(attribute.getId(), attribute));
    }
}

// FIXME: this might not always be the desired formatter.
// Do we want the number format to match the localisation, or to always use the
// locale-specific format?
static const icu::number::LocalizedNumberFormatter NUMBER_FORMATTER =
    icu::number::NumberFormatter::withLocale(icu::Locale());

const std::string NumberLiteral::format() const {
    icu::ErrorCode status;
    std::string buffer;

    size_t decimalPos = this->value.find_first_of(".");
    std::string result;
    if (decimalPos == std::string::npos) {
        result = NUMBER_FORMATTER.formatInt(stol(this->value), status)
                     .toString(status)
                     .toUTF8String(buffer);
    } else {
        size_t significantDigits = this->value.size() - decimalPos - 1;
        auto formatter = NUMBER_FORMATTER.precision(
            icu::number::Precision::minFraction(significantDigits));

        return formatter.formatDouble(stod(this->value), status)
            .toString(status)
            .toUTF8String(buffer);
    }

    if (status.isSuccess())
        return result;
    else {
        std::cerr << "Formatting number literal \"" << this->value
                  << "\"failed: " << status.errorName() << std::endl;
        // Fall back to the original literal value if formatting fails
        return this->value;
    }
}

const std::string formatVariable(const Variable &variable) {
    std::string buffer;
    return std::visit(
        [&](const auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_same_v<T, long>) {
                icu::ErrorCode status;
                std::string result = NUMBER_FORMATTER.formatInt(arg, status)
                                         .toString(status)
                                         .toUTF8String(buffer);
                if (status.isSuccess()) {
                    return result;
                } else {
                    std::cerr << "Formatting number \"" << arg
                              << "\"failed: " << status.errorName() << std::endl;
                    return std::to_string(arg);
                }
            } else if constexpr (std::is_same_v<T, double>) {
                icu::ErrorCode status;
                std::string result = NUMBER_FORMATTER.formatDouble(arg, status)
                                         .toString(status)
                                         .toUTF8String(buffer);
                if (status.isSuccess()) {
                    return result;
                } else {
                    std::cerr << "Formatting number \"" << arg
                              << "\"failed: " << status.errorName() << std::endl;
                    return std::to_string(arg);
                }
            }
        },
        variable);
}

const std::string formatPattern(
    const std::vector<ast::PatternElement> &pattern,
    const std::map<std::string, Variable> &args,
    const std::function<std::optional<Message>(const std::string &)> messageLookup,
    const std::function<std::optional<Term>(const std::string &)> termLookup) {
    std::stringstream values;

    for (const PatternElement &elem : pattern) {
        std::visit(
            [&](const auto &arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>)
                    values << arg;
                else if constexpr (std::is_same_v<T, StringLiteral>)
                    values << arg.value;
                else if constexpr (std::is_same_v<T, NumberLiteral>) {
                    values << arg.format();
                } else if constexpr (std::is_same_v<T, MessageReference>) {
                    auto message = messageLookup(arg.identifier);
                    if (message) {
                        if (arg.attribute) {
                            std::optional<Attribute> attribute =
                                message->getAttribute(*arg.attribute);
                            values
                                << attribute->format(args, messageLookup, termLookup);
                        } else {
                            values << message->format(args, messageLookup, termLookup);
                        }
                    } else {
                        // FIXME: This could probably be handled better
                        values << "unknown message " << arg;
                    }
                } else if constexpr (std::is_same_v<T, TermReference>) {
                    // FIXME: TermReferences can also have arguments
                    auto term = termLookup(arg.identifier);
                    if (term) {
                        if (arg.attribute) {
                            std::optional<Attribute> attribute =
                                term->getAttribute(*arg.attribute);
                            values << attribute->format({}, messageLookup, termLookup);
                        } else {
                            values << term->format({}, messageLookup, termLookup);
                        }
                    } else {
                        // FIXME: This could probably be handled better
                        values << "unknown message " << arg;
                    }
                } else if constexpr (std::is_same_v<T, VariableReference>)
                    values << formatVariable(args.at(arg.identifier));
                else
                    static_assert(always_false_v<T>, "non-exhaustive visitor!");
            },
            elem);
    }
    return values.str();
}

const std::string Attribute::format(
    const std::map<std::string, Variable> &args,
    const std::function<std::optional<Message>(const std::string &)> messageLookup,
    const std::function<std::optional<Term>(const std::string &)> termLookup) const {
    return formatPattern(this->pattern, args, messageLookup, termLookup);
}

const std::string Message::format(
    const std::map<std::string, Variable> &args,
    const std::function<std::optional<Message>(const std::string &)> messageLookup,
    const std::function<std::optional<Term>(const std::string &)> termLookup) const {
    return formatPattern(this->pattern, args, messageLookup, termLookup);
}

#ifdef TEST
namespace pt = boost::property_tree;

boost::property_tree::ptree MessageReference::getPropertyTree() const {
    pt::ptree root, expression, id, attribute;
    root.put("type", "Placeable");
    expression.put("type", this->getPropertyTreeType());
    id.put("type", "Identifier");
    id.put("name", this->identifier);
    expression.add_child("id", id);
    if (this->attribute) {
        attribute.put("type", "Identifier");
        attribute.put("name", *this->attribute);
        expression.add_child("attribute", attribute);
    } else {
        expression.put("attribute", "null");
    }
    root.add_child("expression", expression);
    return root;
}

boost::property_tree::ptree TermReference::getPropertyTree() const {
    pt::ptree root = MessageReference::getPropertyTree();
    root.put("arguments", "null");
    return root;
}

pt::ptree getPatternPropertyTree(const std::vector<PatternElement> &pattern) {
    pt::ptree value, elements;
    value.put("type", "Pattern");
    for (const PatternElement &elem : pattern) {
        std::visit(
            [&elements](const auto &arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    pt::ptree textElem;
                    textElem.put("type", "TextElement");
                    textElem.put("value", arg);
                    elements.push_back(std::make_pair("", textElem));
                } else if constexpr (std::is_same_v<T, StringLiteral>) {
                    pt::ptree stringElem, expression;
                    stringElem.put("type", "Placeable");
                    expression.put("value", arg.value);
                    expression.put("type", "StringLiteral");
                    stringElem.add_child("expression", expression);
                    elements.push_back(std::make_pair("", stringElem));
                } else if constexpr (std::is_same_v<T, NumberLiteral>) {
                    pt::ptree stringElem, expression;
                    stringElem.put("type", "Placeable");
                    expression.put("value", arg.value);
                    expression.put("type", "NumberLiteral");
                    stringElem.add_child("expression", expression);
                    elements.push_back(std::make_pair("", stringElem));

                } else if constexpr (std::is_same_v<T, VariableReference>) {
                    pt::ptree varElem, expression, id;
                    varElem.put("type", "Placeable");
                    expression.put("type", "VariableReference");
                    id.put("type", "Identifier");
                    id.put("name", arg.identifier);
                    expression.add_child("id", id);
                    varElem.add_child("expression", expression);
                    elements.push_back(std::make_pair("", varElem));
                } else if constexpr (std::is_same_v<T, MessageReference>) {
                    elements.push_back(std::make_pair("", arg.getPropertyTree()));
                } else if constexpr (std::is_same_v<T, TermReference>) {
                    elements.push_back(std::make_pair("", arg.getPropertyTree()));
                } else
                    static_assert(always_false_v<T>, "non-exhaustive visitor!");
            },
            elem);
    }
    value.add_child("elements", elements);
    return value;
}

boost::property_tree::ptree Attribute::getPropertyTree() const {
    pt::ptree root, identifier, comment, value, elements;
    root.put("type", "Attribute");
    identifier.put("type", "Identifier");
    identifier.put("name", this->id);
    root.add_child("id", identifier);
    if (this->pattern.empty()) {
        root.put("value", "null");
    } else {
        root.add_child("value", getPatternPropertyTree(this->pattern));
    }
    return root;
}

boost::property_tree::ptree Message::getPropertyTree() const {
    pt::ptree message, identifier, comment, value, elements;
    message.put("type", this->getPropertyTreeType());
    identifier.put("type", "Identifier");
    identifier.put("name", this->id);
    message.add_child("id", identifier);
    if (this->pattern.empty()) {
        message.put("value", "null");
    } else {
        message.add_child("value", getPatternPropertyTree(this->pattern));
    }
    if (this->attributes.empty()) {
        message.put("attributes", "");
    } else {
        pt::ptree attributes;
        for (auto attribute : this->attributes) {
            attributes.push_back(
                std::make_pair("", attribute.second.getPropertyTree()));
        }
        message.add_child("attributes", attributes);
    }
    if (this->comment) {
        comment.put("type", "Comment");
        comment.put("content", this->comment->getValue());
        message.add_child("comment", comment);
    } else {
        message.put("comment", "null");
    }
    return message;
}

void processEntry(pt::ptree &parent, fluent::ast::Entry &entry) {
    std::visit(
        [&parent](const auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, fluent::ast::Message>) {
                parent.push_back(std::make_pair("", arg.getPropertyTree()));
            } else if constexpr (std::is_same_v<T, fluent::ast::Term>) {
                parent.push_back(std::make_pair("", arg.getPropertyTree()));
            } else if constexpr (std::is_same_v<T, fluent::ast::AnyComment>) {
                pt::ptree comment;
                std::visit(
                    [&comment](const auto &arg) {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, fluent::ast::Comment>) {
                            comment.put("type", "Comment");
                            comment.put("content", arg.getValue());
                        } else if constexpr (std::is_same_v<
                                                 T, fluent::ast::GroupComment>) {
                            comment.put("type", "GroupComment");
                            comment.put("content", arg.getValue());
                        } else if constexpr (std::is_same_v<
                                                 T, fluent::ast::ResourceComment>) {
                            comment.put("type", "ResourceComment");
                            comment.put("content", arg.getValue());
                        } else
                            static_assert(always_false_v<T>, "non-exhaustive visitor!");
                    },
                    arg);
                parent.push_back(std::make_pair("", comment));
            } else if constexpr (std::is_same_v<T, fluent::ast::Junk>) {
                pt::ptree comment;
                comment.put("type", "Junk");
                comment.put("annotations", "");
                comment.put("content", arg.value);
                parent.push_back(std::make_pair("", comment));
            } else
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
        },
        entry);
}
#endif

} // namespace ast
} // namespace fluent
