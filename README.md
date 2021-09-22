# Fluent

fluent-cpp is a C++ library which aims to implement [Project Fluent](https://projectfluent.org/).

It is currently under development, highly unstable and only currently supports a subset of Project Fluent's ftl language.

## Usage

Most interaction with fluent-cpp should be done through the high-level `fluent/loader.hpp`.

E.g.

```c++
#include <fluent/loader.hpp>
#include <unicode/locid.hpp>

icu::Locale DEFAULT_LOCALE("en");

fluent::FluentLoader loader;
loader.addMessage(DEFAULT_LOCALE, "test-message", "Pattern with an argument with value { $value }");

std::optional<std::string> result = loader.formatMessage({icu::Locale(), DEFAULT_LOCALE}, "test-message", {{"value", "Example Value"}});
assert(*result == "Pattern with an argument with value Example Value");
```

You can add entire directories of fluent files at once with `fluent::FluentLoader::addDirectory`.

You can also embed fluent resources into the executable using the builtin `ftlembed` tool. This produces a `cpp` file which, if compiled into your executable, will load at runtime the embedded data into the static loader, accessible through the `fluent::formatStaticMessage` function.

A helper CMake function called embed_ftl has been provided to embed directories of fluent files. See [tests/CMakeLists.txt](https://gitlab.com/bmwinger/fluent-cpp/-/blob/master/tests/CMakeLists.txt) for an example of its usage.
