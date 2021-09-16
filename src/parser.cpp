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

#include "parser.hpp"
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy_ext/report_error.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef DEBUG_PARSER
#include <lexy/action/parse_as_tree.hpp>
#include <lexy/input/string_input.hpp>
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
    struct CommentInner : lexy::token_production {
        static constexpr auto rule =
            (dsl::lit_c<'#'> / dsl::lit<"##"> / dsl::lit<"###">)+dsl::opt(
                dsl::peek_not(dsl::eol) >>
                dsl::lit_c<' '> +
                    dsl::capture(dsl::while_(dsl::peek_not(dsl::eol) >>
                                             dsl::code_point - dsl::eol))) +
            dsl::newline;
        static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
    };
    static constexpr auto rule = dsl::p<CommentInner>;
    static constexpr auto value = lexy::construct<ast::Comment>;
};

// Junk                ::= junk_line (junk_line - "#" - "-" - [a-zA-Z])*
// junk_line           ::= /[^\n]*/ ("\u000A" | EOF)
static constexpr auto junk_char = dsl::code_point - dsl::newline;
struct Junk : lexy::token_production {
    static constexpr auto rule =
        dsl::capture(dsl::while_(junk_char) + dsl::eol +
                     dsl::while_(dsl::peek_not(dsl::lit_c<'#'> / dsl::lit_c<'-'> /
                                               dsl::ascii::alpha / dsl::eof) >>
                                 dsl::while_(junk_char) + dsl::eol));
    static constexpr auto value = lexy::callback<ast::Junk>(
        [](auto text) { return ast::Junk(std::string(text.begin(), text.end())); });
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
    static constexpr auto rule = dsl::capture(dsl::while_one(text_char));
    static constexpr auto value = lexy::callback<ast::PatternElement>([](auto text) {
        return ast::PatternElement(std::string(text.begin(), text.end()));
    });
};

struct opt_inline_text : lexy::token_production {
    static constexpr auto rule = dsl::capture(dsl::while_(text_char));
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
};
// block_text          ::= blank_block blank_inline indented_char inline_text?
struct block_text : lexy::token_production {
    static constexpr auto rule = dsl::capture(blank_block) + blank_inline +
                                 dsl::capture(indented_char) + dsl::p<opt_inline_text>;
    static constexpr auto value = lexy::callback<ast::PatternElement>(
        [](auto blank_block, auto indentedChar, auto text) {
            return ast::PatternElement(
                std::string(blank_block.begin(), blank_block.end()) +
                std::string(indentedChar.begin(), indentedChar.end()) +
                std::string(text.begin(), text.end()));
        });
};

struct VariableReference : lexy::token_production {
    static constexpr auto rule = dsl::lit_c<'$'> + dsl::p<Identifier>;
    static constexpr auto value = lexy::construct<ast::VariableReference>;
};

struct StringLiteral : lexy::token_production {
    struct StringLiteralInner : lexy::transparent_production {
        static constexpr auto escaped_symbols = lexy::symbol_table<char> //
                                                    .map<'"'>('"')
                                                    .map<'\\'>('\\');
        static constexpr auto escape =
            dsl::backslash_escape //
                .symbol<escaped_symbols>()
                .rule(dsl::lit_c<'u'> >> dsl::code_point_id<4>);
        static constexpr auto rule =
            dsl::quoted(dsl::code_point - dsl::newline, escape);
        static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
    };
    static constexpr auto rule = dsl::p<StringLiteralInner>;
    static constexpr auto value = lexy::construct<ast::StringLiteral>;
};

// InlineExpression    ::= StringLiteral | NumberLiteral | FunctionReference |
// MessageReference | TermReference | VariableReference | inline_placeable
struct InlineExpression {
    struct expected_expression {
        static LEXY_CONSTEVAL auto name() { return "expected expression"; }
    };
    static constexpr auto
        rule = dsl::peek(dsl::lit_c<'"'>) >> dsl::p<StringLiteral> |
               dsl::peek(dsl::lit_c<'$'>) >> dsl::p<VariableReference> |
               dsl::error<expected_expression>;
    static constexpr auto value = lexy::construct<ast::PatternElement>;
};

// inline_placeable    ::= "{" blank? (SelectExpression | InlineExpression) blank? "}"
struct inline_placeable : lexy::token_production {
    // FIXME: Add Select Expression
    static constexpr auto rule = dsl::lit_c<'{'> + opt_blank +
                                 (dsl::p<InlineExpression>)+opt_blank + dsl::lit_c<'}'>;
    static constexpr auto value = lexy::forward<ast::PatternElement>;
};

// PatternElement      ::= inline_text | block_text | inline_placeable | block_placeable
struct PatternElement : lexy::token_production {
    // FIXME: Add block_placeable
    static constexpr auto rule =
        dsl::peek(dsl::p<block_text>) >> dsl::p<block_text> |
        dsl::peek(dsl::lit_c<'{'>) >> dsl::p<inline_placeable> | dsl::p<inline_text>;
    static constexpr auto value = lexy::forward<ast::PatternElement>;
};

/* Patterns are values of Messages, Terms, Attributes and Variants. */
// Pattern             ::= PatternElement+
struct Pattern : lexy::token_production {
    static constexpr auto rule = dsl::list(dsl::p<PatternElement>);
    static constexpr auto value = lexy::as_list<std::vector<ast::PatternElement>>;
};

// Message             ::= Identifier blank_inline? "=" blank_inline? ((Pattern
// Attribute*) | (Attribute+))
struct Message {
    static constexpr auto whitespace = dsl::lit_c<' '>;
    // FIXME: Add Attributes
    static constexpr auto rule =
        dsl::p<Identifier> + dsl::lit_c<'='> + dsl::p<Pattern> + dsl::newline;
    static constexpr auto value = lexy::construct<ast::Message>;
};

struct Entry : lexy::token_production {
    static constexpr auto rule =
        dsl::peek(dsl::lit_c<'#'>) >> dsl::p<CommentLine> |
        dsl::peek(dsl::p<Message>) >> dsl::p<Message> | dsl::else_ >> dsl::p<Junk>;
    static constexpr auto value = lexy::construct<ast::Entry>;
};

struct Resource {
    static constexpr auto ws = dsl::whitespace(blank_block);
    // static constexpr auto ws = dsl::whitespace(dsl::newline);
    static constexpr auto rule =
        dsl::opt(dsl::list(dsl::peek_not(dsl::eof) >> (ws + dsl::p<Entry> + ws))) +
        dsl::eof;
    static constexpr auto value = lexy::as_list<std::vector<ast::Entry>>;
};

} // namespace grammar

std::vector<ast::Entry> parse(const char *filename) {
    auto file = lexy::read_file<lexy::utf8_encoding>(filename);
    if (!file) {
        throw std::runtime_error("file '" + std::string(filename) + "' not found");
    }

#ifdef DEBUG_PARSER
    lexy::trace<grammar::Resource>(stdout, file.buffer());

    lexy::parse_tree_for<decltype(file.buffer())> tree;
    auto result = lexy::parse_as_tree<grammar::Resource>(tree, file.buffer(),
                                                         lexy_ext::report_error);
    lexy::visualize(stdout, tree, {lexy::visualize_fancy});
#endif

    auto parse_result =
        lexy::parse<grammar::Resource>(file.buffer(), lexy_ext::report_error);
    if (parse_result)
        return parse_result.value();
    else
        throw std::runtime_error("Failed to parse");
}

} // namespace fluent
