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

        static constexpr auto text_char = dsl::code_point - dsl::lit_c<'{'> - dsl::lit_c<'}'> - dsl::newline;
        static constexpr auto blank_inline = dsl::while_one(dsl::lit_c<' '>);
        // blank_block         ::= (blank_inline? line_end)+
        static constexpr auto blank_block = dsl::while_one(dsl::peek(dsl::while_(dsl::lit_c<' '>) + dsl::newline) >> dsl::while_(dsl::lit_c<' '>) + dsl::newline);
        // blank               ::= (blank_inline | line_end)+
        static constexpr auto opt_blank = dsl::while_(blank_inline | dsl::newline);
        static constexpr auto indented_char = text_char - dsl::lit_c<'['> - dsl::lit_c<'*'> - dsl::lit_c<'.'>;

        // \r is not considered a newline, but may appear by itself in the text
        // So if a \r is read, we must peek and see if it's followed by a \n
        static constexpr auto comment_contents = dsl::while_(dsl::code_point - dsl::newline);

        struct MessageCommentLine : lexy::token_production {
            // The Message comment may be followed by a group comment
            // So we when parsing the MessageComment rule we need to be sure it stops when it
            // encounters ## instead of failing
            static constexpr auto
                rule = dsl::peek(LEXY_LIT("#") + (dsl::lit_c<' '> / dsl::eol)) >>
                    LEXY_LIT("#") >>
                    dsl::opt(dsl::lit_c<' '> >> dsl::capture(comment_contents)) + dsl::eol;
            static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
        };

        struct MessageComment {
            static constexpr auto rule = dsl::list(dsl::p<MessageCommentLine>);
            static constexpr auto value = lexy::as_list<std::vector<std::string>> >> lexy::construct<ast::Comment>;
        };

        struct GroupCommentLine : lexy::token_production {
            // The Group comment may be followed by a resource comment
            static constexpr auto
                rule = dsl::peek(LEXY_LIT("##") + (dsl::lit_c<' '> / dsl::eol)) >>
                    LEXY_LIT("##") >>
                    dsl::opt(dsl::lit_c<' '> >> dsl::capture(comment_contents)) + dsl::eol;
            static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
        };

        struct GroupComment {
            static constexpr auto rule = dsl::list(dsl::p<GroupCommentLine>);
            static constexpr auto value =
                lexy::as_list<std::vector<std::string>> >> lexy::construct<ast::GroupComment>;
        };

        struct ResourceCommentLine : lexy::token_production {
            static constexpr auto
                rule = LEXY_LIT("###") >> dsl::opt(dsl::lit_c<' '> >> dsl::capture(comment_contents)) + dsl::eol;
            static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
        };

        struct ResourceComment {
            static constexpr auto rule = dsl::list(dsl::p<ResourceCommentLine>);
            static constexpr auto value = lexy::as_list<std::vector<std::string>> >> lexy::construct<ast::ResourceComment>;
        };

        // CommentLine         ::= ("###" | "##" | "#") ("\u0020" comment_char*)? line_end
        struct Comment : lexy::token_production {
            static constexpr auto rule = dsl::p<ResourceComment> | dsl::p<GroupComment> | dsl::p<MessageComment>;
            static constexpr auto value = lexy::construct<ast::AnyComment>;
        };

        // Junk                ::= junk_line (junk_line - "#" - "-" - [a-zA-Z])*
        // junk_line           ::= /[^\n]*/ ("\u000A" | EOF)
        static constexpr auto junk_line = dsl::until(dsl::eol);
        static constexpr auto junk = junk_line + dsl::while_(junk_line - dsl::lit_c<'#'> - dsl::lit_c<'-'> - dsl::ascii::alpha);
        struct Junk : lexy::token_production {
            static constexpr auto rule = dsl::capture(junk_line + dsl::while_(dsl::peek_not(dsl::lit_c<'#'> / dsl::lit_c<'-'> / dsl::ascii::alpha / dsl::eof) >> junk_line));
            static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> | lexy::construct<ast::Junk>;
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
            static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> | lexy::construct<ast::PatternElement>;
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
                auto value = block_prefix + dsl::capture(indented_char) + dsl::p<opt_inline_text>;
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
            static constexpr auto rule = dsl::p<Identifier> >> dsl::opt(dsl::p<AttributeAccessor>);
            static constexpr auto value = lexy::construct<ast::MessageReference>;
        };

        // TermReference       ::= "-" Identifier AttributeAccessor? CallArguments?
        struct TermReference : lexy::token_production {
            // FIXME: Add arguments
            static constexpr auto rule = dsl::lit_c<'-'> >> dsl::p<Identifier> >> dsl::opt(dsl::p<AttributeAccessor>);
            static constexpr auto value = lexy::construct<ast::TermReference>;
        };

        // NumberLiteral       ::= "-"? digits ("." digits)?
        struct NumberLiteral : lexy::token_production {
            static constexpr auto rule = [] {
                auto digits = dsl::digits<>;
                auto decimal = dsl::if_(dsl::lit_c<'.'> >> digits);
                // Peek is necessary to ensure that negative NumberLiterals can be distinguised
                // from terms Note that the Term doesn't peek, as it is is processed after we
                // have eliminated the possibility of a NumberLiteral
                return dsl::peek(dsl::lit_c<'-'> >> dsl::ascii::digit | dsl::ascii::digit) >>
                    dsl::capture(dsl::lit_c<'-'> >> digits >> decimal | digits >> decimal);
            }();
            static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> |
                                        lexy::construct<ast::NumberLiteral>;
        };

        struct StringLiteral : lexy::token_production {
            static constexpr auto escaped_symbols = 
                lexy::symbol_table<char> //
                    .map<'"'>('"')
                    .map<'\\'>('\\');
            static constexpr auto escape = 
                dsl::backslash_escape //
                    .symbol<escaped_symbols>()
                    .rule(dsl::lit_c<'u'> >> dsl::code_point_id<4>)
                    .rule(dsl::lit_c<'U'> >> dsl::code_point_id<6>);
            static constexpr auto rule =
                dsl::quoted.limit(dsl::newline)(dsl::code_point - dsl::newline, escape);
            static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding> >> lexy::construct<ast::StringLiteral>;
        };

        struct inline_placeable;

        // InlineExpression    ::= StringLiteral | NumberLiteral | FunctionReference |
        // MessageReference | TermReference | VariableReference | inline_placeable
        struct InlineExpression {
            struct expected_expression {
                static LEXY_CONSTEVAL auto name() { return "expected expression"; }
            };
            // Note: order is necessary for parsing
            static constexpr auto rule = [] {
                return dsl::p<StringLiteral> 
                    |  dsl::p<NumberLiteral>
                    |  dsl::p<MessageReference>
                    |  dsl::p<TermReference>
                    |  dsl::p<VariableReference>
                    |  dsl::recurse_branch<inline_placeable>
                    |  dsl::error<expected_expression>;
            }();
            static constexpr auto value = lexy::construct<ast::PatternElement>;
        };

        // Selectors are like InlineExpressions, but cannot have nested placeables
        struct Selector {
            struct expected_expression {
                static LEXY_CONSTEVAL auto name() { return "expected expression"; }
            };
            // Note: order is necessary for parsing
            static constexpr auto rule = [] {
                return dsl::p<StringLiteral>
                    |  dsl::p<NumberLiteral>
                    |  dsl::p<MessageReference>
                    |  dsl::p<TermReference>
                    |  dsl::p<VariableReference>;
            }();
            static constexpr auto value = lexy::construct<ast::PatternElement>;
        };

        struct Pattern;

        struct VariantKey {
            static constexpr auto rule = 
                dsl::lit_c<'['> >> opt_blank + (dsl::p<NumberLiteral> | dsl::p<Identifier>)+opt_blank + dsl::lit_c<']'>;
            // Not sure if this needs to be anything other than a string, but NumberLiteral
            // doesn't produce one
            static constexpr auto value = lexy::construct<ast::VariantKey>;
        };

        // Variant             ::= line_end blank? VariantKey blank_inline? Pattern
        struct Variant {
            static constexpr auto rule = dsl::peek(dsl::newline + opt_blank + dsl::lit_c<'['>) >> dsl::newline + opt_blank + dsl::p<VariantKey> + dsl::if_(blank_inline) + dsl::recurse<Pattern>;
            static constexpr auto value = lexy::construct<std::pair<ast::VariantKey, std::vector<ast::PatternElement>>>;
        };

        // DefaultVariant      ::= line_end blank? "*" VariantKey blank_inline? Pattern
        struct DefaultVariant {
            static constexpr auto rule = dsl::newline >> opt_blank + dsl::lit_c<'*'> + dsl::p<VariantKey> + dsl::if_(blank_inline) + dsl::recurse<Pattern>;
            static constexpr auto value =
                lexy::construct<std::pair<ast::VariantKey, std::vector<ast::PatternElement>>>;
        };

        struct Variants {
            static constexpr auto rule = dsl::opt(dsl::list(dsl::p<Variant>));
            static constexpr auto value = lexy::as_list<std::vector<std::pair<ast::VariantKey, std::vector<ast::PatternElement>>>>;
        };

        // FIXME: We could probably parse SelectExpression and InlineExpression together, such
        // that if it's followed by a -> it gets turned into a SelectExpression, otherwise it's
        // just an InlineExpression. It would remove the unnecessary peek
        //
        // SelectExpression ::= InlineExpression blank? "->" blank_inline? variant_list
        struct SelectExpression {
            struct inline_placeable_error {
                static constexpr auto name = "Nested placeables are not valid selectors";
            };
            static constexpr auto rule = [] {
                // FIXME: It used to be possible to recursively peek meaning that
                // InlineExpression could be peeked here, and filtered to Selector after the
                // peek, to provide a more helpful error message about using nested placeables
                // as selectors, however lexy no longer allows recursive peeks.
                // FIXME: Can we provide a better error message if someone tries to use a
                // Select Expression as a selector?
                auto prefix = dsl::p<Selector> + opt_blank + LEXY_LIT("->");
                // variant_list        ::= Variant* DefaultVariant Variant* line_end
                auto variant_list = dsl::p<Variants> + dsl::p<DefaultVariant> + dsl::p<Variants> + dsl::eol;
                return dsl::peek(prefix) >> prefix + dsl::if_(blank_inline) + variant_list;
            }();
            static constexpr auto value = lexy::construct<ast::SelectExpression> | lexy::construct<ast::PatternElement>;
        };

        // inline_placeable    ::= "{" blank? (SelectExpression | InlineExpression) blank? "}"
        struct inline_placeable : lexy::token_production {
            static constexpr auto rule = dsl::lit_c<'{'> >> opt_blank + (dsl::p<SelectExpression> | dsl::else_ >> dsl::p<InlineExpression>)+opt_blank + dsl::lit_c<'}'>;
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
            static constexpr auto rule = dsl::p<block_text> | dsl::p<block_placeable> | dsl::p<inline_placeable> | dsl::p<inline_text>;
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
            static constexpr auto rule = dsl::token(dsl::newline + opt_blank + dsl::lit_c<'.'>) >> dsl::p<Identifier> + dsl::if_(blank_inline) + dsl::lit_c<'='> + dsl::if_(blank_inline) + dsl::p<Pattern>;
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
            static constexpr auto rule = dsl::lit_c<'-'> >> dsl::p<Identifier> + dsl::lit_c<'='> + dsl::p<Pattern> + dsl::p<Attributes> + dsl::newline;
            static constexpr auto value = lexy::construct<ast::Term>;
        };

        // Message ::= Identifier blank_inline? "="
        //   blank_inline? ((Pattern Attribute*) | (Attribute+))
        struct Message {
            struct missing_pattern_or_attribute {
                static constexpr auto name = "Message must contain at least one pattern or atribute";
            };
            // FIXME: Use explicit whitespace
            static constexpr auto whitespace = dsl::lit_c<' '>;
            static constexpr auto rule = [] {
                auto value = dsl::p<Pattern> >> dsl::p<Attributes> | dsl::p<AttributesPlus> | dsl::error<missing_pattern_or_attribute>;

                return dsl::p<Identifier> >> dsl::lit_c<'='> + value + dsl::newline;
            }();
            static constexpr auto value = lexy::construct<ast::Message>;
        };

        struct Entry : lexy::token_production {
            static constexpr auto rule = [] {
                auto comment = dsl::p<Comment>;
                auto entry = dsl::p<Term> | dsl::p<Message>;

                auto commented_entry = dsl::peek(LEXY_LIT("# ")) >> dsl::p<MessageComment> + dsl::if_(entry);

                // commented_entry must be before comment
                // since commented_entry is prefixed with part of comment
                return commented_entry | comment | entry;
            }();
            static constexpr auto value =
                lexy::callback<ast::Entry>(
                    [](auto entry) { return entry; }, 
                    [](ast::Comment comment, auto entry) { entry.setComment(std::move(comment)); return entry; }
                );
        };

        struct Resource {
            static constexpr auto ws = dsl::whitespace(blank_block);
            static constexpr auto rule = dsl::terminator(dsl::eof).opt_list( ws + dsl::try_(dsl::p<Entry>, dsl::p<Junk>) + ws);
            static constexpr auto value = lexy::as_list<std::vector<ast::Entry>>;
        };

    } // namespace grammar

    std::vector<ast::Entry> parseFile(const std::filesystem::path &filename, bool strict) {
        std::ifstream ifs(filename);
        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        auto stringInput = lexy::string_input<lexy::utf8_encoding>(content);
    #ifdef DEBUG_PARSER
        lexy::trace<grammar::Resource>(stdout, stringInput);

        lexy::parse_tree_for<decltype(stringInput)> tree;
        auto result = lexy::parse_as_tree<grammar::Resource>(tree, stringInput, lexy_ext::report_error);
        lexy::visualize(stdout, tree, {lexy::visualize_fancy});
    #endif

        auto parse_result = lexy::parse<grammar::Resource>(stringInput, lexy_ext::report_error);

        if (strict && parse_result.is_error() || !strict && parse_result.is_fatal_error()) {
            // FIXME: This should be a more specific error
            throw std::runtime_error("Failed to parse");
        } else {
            return parse_result.value();
        }
    }

    std::vector<ast::Entry> parse(std::string &&input, bool strict) {
        auto parse_result = lexy::parse<grammar::Resource>(lexy::string_input<lexy::utf8_encoding>(input), lexy_ext::report_error);

        if (strict && parse_result.is_error() || !strict && parse_result.is_fatal_error()) {
            // FIXME: This should be a more specific error
            throw std::runtime_error("Failed to parse");
        } else {
            return parse_result.value();
        }
    }

    std::vector<ast::PatternElement> parsePattern(const std::string &input) {
        auto parse_result = lexy::parse<grammar::Pattern>(lexy::string_input<lexy::utf8_encoding>(input), lexy_ext::report_error);
        if (parse_result) {
            return parse_result.value();
        } else {
            throw std::runtime_error("Failed to parse");
        }
    }

    ast::MessageReference parseMessageReference(const std::string &input) {
        auto parse_result = lexy::parse<grammar::MessageReference>(lexy::string_input<lexy::utf8_encoding>(input), lexy_ext::report_error);
        if (parse_result) {
            return parse_result.value();
        } else {
            throw std::runtime_error("Failed to parse");
        }
    }

} // namespace fluent
