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

#include "fluent/parser.hpp"
#include "fluent/ast.hpp"
#include <fstream>
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#ifdef DEBUG_PARSER
#include <lexy/action/parse_as_tree.hpp>
#include <lexy/visualize.hpp>
#endif

namespace fluent {
namespace grammar {
namespace dsl = lexy::dsl;

/*
 * This grammar is derived from the project fluent EBNF specification
 * https://github.com/projectfluent/fluent/blob/master/spec/fluent.ebnf
 *
 * Parts of the specification have been included verbatim for reference and clarity
 *  (some formatting changes have been made).
 * Project Fluent is licensed under the Apache 2.0 license.
 * A copy of this license can be found in the fluent directory in the root of this
 * project
 */

static constexpr auto text_char =
    dsl::code_point - dsl::lit_c<'{'> - dsl::lit_c<'}'> - dsl::newline;
static constexpr auto blank_inline = dsl::while_one(dsl::lit_c<' '>);
// blank_block         ::= (blank_inline? line_end)+
static constexpr auto blank_block =
    dsl::while_one(dsl::peek(dsl::while_(dsl::lit_c<' '>) + dsl::newline) >>
                   dsl::while_(dsl::lit_c<' '>) + dsl::newline);
// blank               ::= (blank_inline | line_end)+
static constexpr auto opt_blank = dsl::while_(blank_inline | dsl::newline);
static constexpr auto indented_char =
    text_char - dsl::lit_c<'['> - dsl::lit_c<'*'> - dsl::lit_c<'.'>;

// CommentLine         ::= ("###" | "##" | "#") ("\u0020" comment_char*)? line_end
struct CommentLine : lexy::token_production {
    static constexpr auto rule =
        (dsl::lit_c<'#'> / LEXY_LIT("##") / LEXY_LIT("###")) +
        dsl::opt(
            dsl::lit_c<' '> >>
            dsl::capture(dsl::while_(dsl::code_point - dsl::lit_c<'\r'> - dsl::eol))) +
        dsl::newline;
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> |
                                  lexy::construct<ast::Comment>;
};

struct MessageComment : lexy::token_production {
    static constexpr auto rule =
        LEXY_LIT("# ") >>
            dsl::capture(dsl::while_(dsl::code_point - dsl::lit_c<'\r'> - dsl::eol)) +
                dsl::newline
        | dsl::lit_c<'#'> >> (dsl::else_ >> dsl::nullopt) >> dsl::newline;
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
};

// Junk                ::= junk_line (junk_line - "#" - "-" - [a-zA-Z])*
// junk_line           ::= /[^\n]*/ ("\u000A" | EOF)
auto junk_line = dsl::until(dsl::eol);
auto junk = junk_line + dsl::while_(junk_line - dsl::lit_c<'#'> - dsl::lit_c<'-'> -
                                    dsl::ascii::alpha);
struct Junk : lexy::token_production {
    static constexpr auto rule = dsl::capture(
        junk_line + dsl::while_(dsl::peek_not(dsl::lit_c<'#'> / dsl::lit_c<'-'> /
                                              dsl::ascii::alpha / dsl::eof) >>
                                junk_line));
    static constexpr auto value =
        lexy::as_string<std::string, lexy::utf8_encoding> | lexy::construct<ast::Junk>;
};

struct Identifier : lexy::token_production {
    static constexpr auto rule = [] {
        auto head = dsl::ascii::alpha_underscore;
        auto tail = dsl::ascii::alpha_digit_underscore / dsl::lit_c<'-'>;
        return dsl::identifier(head, tail);
    }();
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
};

// inline_text         ::= text_char+
struct inline_text : lexy::token_production {
    static constexpr auto rule = dsl::identifier(text_char);
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> |
                                  lexy::construct<ast::PatternElement>;
};

struct opt_inline_text : lexy::token_production {
    static constexpr auto rule = dsl::capture(dsl::while_(text_char));
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
};

// block_text          ::= blank_block blank_inline indented_char inline_text?
struct block_text : lexy::token_production {
    static constexpr auto rule = [] {
        // Note: the indent needs to be captured as a whole as we will remove
        // matching indents from the pattern later
        auto block_prefix = dsl::capture(blank_block + blank_inline);
        auto value =
            block_prefix + dsl::capture(indented_char) + dsl::p<opt_inline_text>;
        return dsl::peek(block_prefix + indented_char) >> value;
    }();
    static constexpr auto value = lexy::callback<ast::PatternElement>(
        [](auto blank_block, auto indentedChar, auto text) {
            return ast::PatternElement(
                std::string(blank_block.begin(), blank_block.end()) +
                std::string(indentedChar.begin(), indentedChar.end()) +
                std::string(text.begin(), text.end()));
        });
};

struct VariableReference : lexy::token_production {
    static constexpr auto rule = dsl::lit_c<'$'> >> dsl::p<Identifier>;
    static constexpr auto value = lexy::construct<ast::VariableReference>;
};

// AttributeAccessor   ::= "." Identifier
struct AttributeAccessor {
    static constexpr auto rule = dsl::lit_c<'.'> >> dsl::p<Identifier>;
    static constexpr auto value = lexy::forward<std::string>;
};

// MessageReference    ::= Identifier AttributeAccessor?
struct MessageReference : lexy::token_production {
    static constexpr auto rule = dsl::p<Identifier> >>
                                 dsl::opt(dsl::p<AttributeAccessor>);
    static constexpr auto value = lexy::construct<ast::MessageReference>;
};

// TermReference       ::= "-" Identifier AttributeAccessor? CallArguments?
struct TermReference : lexy::token_production {
    // FIXME: Add arguments
    static constexpr auto rule = dsl::lit_c<'-'> >> dsl::p<Identifier> >>
                                 dsl::opt(dsl::p<AttributeAccessor>);
    static constexpr auto value = lexy::construct<ast::TermReference>;
};

// NumberLiteral       ::= "-"? digits ("." digits)?
struct NumberLiteral : lexy::token_production {
    static constexpr auto rule = [] {
        auto digits = dsl::digits<>;
        auto decimal = dsl::if_(dsl::lit_c<'.'> >> digits);
        return dsl::capture(dsl::lit_c<'-'> >> digits >> decimal | digits >> decimal);
    }();
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> |
                                  lexy::construct<ast::NumberLiteral>;
};

struct StringLiteral : lexy::token_production {
    static constexpr auto escaped_symbols = lexy::symbol_table<char> //
                                                .map<'"'>('"')
                                                .map<'\\'>('\\');
    static constexpr auto escape = dsl::backslash_escape //
                                       .symbol<escaped_symbols>()
                                       .rule(dsl::lit_c<'u'> >> dsl::code_point_id<4>);
    static constexpr auto rule = dsl::quoted(dsl::code_point - dsl::newline, escape);
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> >>
                                  lexy::construct<ast::StringLiteral>;
};

// InlineExpression    ::= StringLiteral | NumberLiteral | FunctionReference |
// MessageReference | TermReference | VariableReference | inline_placeable
struct InlineExpression {
    struct expected_expression {
        static LEXY_CONSTEVAL auto name() { return "expected expression"; }
    };
    // Note: order is necessary for parsing
    static constexpr auto rule = [] {
        // Note: since recurse does not produce a branch rule, we must inline
        // inline_placeable here
        auto inline_placeable = dsl::lit_c<'{'> >> opt_blank +
                                                       dsl::recurse<InlineExpression> +
                                                       opt_blank + dsl::lit_c<'}'>;
        return dsl::p<StringLiteral> | dsl::p<NumberLiteral> |
               dsl::p<MessageReference> | dsl::p<TermReference> |
               dsl::p<VariableReference> | inline_placeable |
               dsl::error<expected_expression>;
    }();
    static constexpr auto value = lexy::construct<ast::PatternElement>;
};

// inline_placeable    ::= "{" blank? (SelectExpression | InlineExpression) blank? "}"
struct inline_placeable : lexy::token_production {
    // FIXME: Add Select Expression
    static constexpr auto rule = dsl::lit_c<'{'> >> opt_blank +
                                                        dsl::p<InlineExpression> +
                                                        opt_blank + dsl::lit_c<'}'>;
    static constexpr auto value = lexy::forward<ast::PatternElement>;
};

// block_placeable     ::= blank_block blank_inline? inline_placeable
struct block_placeable : lexy::token_production {
    static constexpr auto rule = [] {
        auto block_prefix = blank_block + dsl::if_(blank_inline);
        auto value = block_prefix + dsl::p<inline_placeable>;
        return dsl::peek(block_prefix + dsl::lit_c<'{'>) >> value;
    }();
    static constexpr auto value = lexy::forward<ast::PatternElement>;
};

// PatternElement      ::= inline_text | block_text | inline_placeable | block_placeable
struct PatternElement : lexy::token_production {
    // FIXME: Add block_placeable
    static constexpr auto rule = dsl::p<block_text> | dsl::p<block_placeable> |
                                 dsl::p<inline_placeable> | dsl::p<inline_text>;
    static constexpr auto value = lexy::forward<ast::PatternElement>;
};

/* Patterns are values of Messages, Terms, Attributes and Variants. */
// Pattern             ::= PatternElement+
struct Pattern : lexy::token_production {
    static constexpr auto rule = dsl::list(dsl::p<PatternElement>);
    static constexpr auto value = lexy::as_list<std::vector<ast::PatternElement>>;
};

// Attribute ::= line_end blank? "." Identifier blank_inline? "=" blank_inline? Pattern
struct Attribute : lexy::token_production {
    static constexpr auto
        rule = dsl::token(dsl::newline + opt_blank + dsl::lit_c<'.'>) >>
               dsl::p<Identifier> + dsl::if_(blank_inline) + dsl::lit_c<'='> +
                   dsl::if_(blank_inline) + dsl::p<Pattern>;
    static constexpr auto value = lexy::construct<ast::Attribute>;
};

struct AttributesPlus : lexy::token_production {
    static constexpr auto rule = dsl::list(dsl::p<Attribute>);
    static constexpr auto value = lexy::as_list<std::vector<ast::Attribute>>;
};

struct Attributes : lexy::token_production {
    static constexpr auto rule = dsl::opt(dsl::list(dsl::p<Attribute>));
    static constexpr auto value = lexy::as_list<std::vector<ast::Attribute>>;
};

// Term ::= "-" Identifier blank_inline? "=" blank_inline? Pattern Attribute*
struct Term {
    static constexpr auto whitespace = dsl::lit_c<' '>;
    static constexpr auto rule = dsl::opt(dsl::p<MessageComment>) + dsl::lit_c<'-'> +
                                 dsl::p<Identifier> + dsl::lit_c<'='> +
                                 dsl::p<Pattern> + dsl::p<Attributes> + dsl::newline;
    static constexpr auto value = lexy::construct<ast::Term>;
};

// Message ::= Identifier blank_inline? "="
//   blank_inline? ((Pattern Attribute*) | (Attribute+))
struct Message {
    struct missing_pattern_or_attribute {
        static constexpr auto name =
            "Message must contain at least one pattern or atribute";
    };
    static constexpr auto whitespace = dsl::lit_c<' '>;
    static constexpr auto rule = [] {
        auto comment = dsl::opt(dsl::p<MessageComment>);
        auto value = dsl::p<Pattern> >> dsl::p<Attributes> | dsl::p<AttributesPlus> |
                     dsl::error<missing_pattern_or_attribute>;

        return comment + dsl::p<Identifier> + dsl::lit_c<'='> + value + dsl::newline;
    }();
    static constexpr auto value = lexy::construct<ast::Message>;
};

struct Entry : lexy::token_production {
    static constexpr auto rule = dsl::peek(dsl::p<Term>) >> dsl::p<Term> |
                                 dsl::peek(dsl::p<Message>) >> dsl::p<Message> |
                                 dsl::peek(dsl::p<CommentLine>) >> dsl::p<CommentLine> |
                                 dsl::else_ >> dsl::p<Junk>;
    static constexpr auto value = lexy::construct<ast::Entry>;
};

struct Resource {
    static constexpr auto ws = dsl::whitespace(blank_block);
    static constexpr auto rule =
        dsl::terminator(dsl::eof).opt_list(ws + dsl::p<Entry> + ws);
    static constexpr auto value = lexy::as_list<std::vector<ast::Entry>>;
};

} // namespace grammar

std::vector<ast::Entry> parseFile(const std::filesystem::path &filename) {
    std::ifstream ifs(filename);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));
    auto stringInput = lexy::string_input<lexy::utf8_encoding>(content);
#ifdef DEBUG_PARSER
    lexy::trace<grammar::Resource>(stdout, stringInput);

    lexy::parse_tree_for<decltype(stringInput)> tree;
    auto result = lexy::parse_as_tree<grammar::Resource>(tree, stringInput,
                                                         lexy_ext::report_error);
    lexy::visualize(stdout, tree, {lexy::visualize_fancy});
#endif

    auto parse_result =
        lexy::parse<grammar::Resource>(stringInput, lexy_ext::report_error);
    if (parse_result)
        return parse_result.value();
    else
        // FIXME: This should be a more specific error
        throw std::runtime_error("Failed to parse");
}

std::vector<ast::Entry> parse(std::string &&input) {
    auto parse_result = lexy::parse<grammar::Resource>(
        lexy::string_input<lexy::utf8_encoding>(input), lexy_ext::report_error);
    if (parse_result)
        return parse_result.value();
    else
        // FIXME: This should be a more specific error
        throw std::runtime_error("Failed to parse");
}

std::vector<ast::PatternElement> parsePattern(const std::string &input) {
    auto parse_result = lexy::parse<grammar::Pattern>(
        lexy::string_input<lexy::utf8_encoding>(input), lexy_ext::report_error);
    if (parse_result)
        return parse_result.value();
    else
        throw std::runtime_error("Failed to parse");
}

ast::MessageReference parseMessageReference(const std::string &input) {
    auto parse_result = lexy::parse<grammar::MessageReference>(
        lexy::string_input<lexy::utf8_encoding>(input), lexy_ext::report_error);
    if (parse_result)
        return parse_result.value();
    else
        throw std::runtime_error("Failed to parse");
}

} // namespace fluent
