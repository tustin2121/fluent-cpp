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

#include <fluent/loader.hpp>
#include <fluent/parser.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <optional>
#include <unicode/locid.h>

void check_result(std::string message,
                  std::map<std::string, fluent::ast::Variable> args,
                  std::string expected) {
    std::optional<std::string> result =
        fluent::formatStaticMessage({icu::Locale("en")}, message, args);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, expected);
}

TEST(TestStatic, BasicString) { check_result("cli-help", {}, "Print help message"); }
TEST(TestStatic, FloatLiteral) { check_result("float-format", {}, "1.0"); }
TEST(TestStatic, IntLiteral) { check_result("integer-format", {}, "10"); }

TEST(TestStatic, StringVariable) { check_result("argument", {{"arg", "Foo"}}, "Foo"); }

// Note: these tests may fail on non-english platforms due to relying on
// user-preferred locale rather than the fallback chain when formatting numbers
TEST(TestStatic, IntVariable) { check_result("argument", {{"arg", 10}}, "10"); }
TEST(TestStatic, FloatVariable) { check_result("argument", {{"arg", 10.1}}, "10.1"); }

TEST(TestStatic, Indentation) { check_result("indentation", {}, "Foo\n    Bar"); }
TEST(TestStatic, IndentationWithExpression) {
    check_result("indentation-with-expression", {}, "Foo\n\nBar\n    Baz");
}
