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

TEST(TestStatic, ChecksStaticLoader) {
    std::optional<std::string> result =
        fluent::formatStaticMessage({icu::Locale("en")}, "cli-help", {});
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, "Print help message");
}
