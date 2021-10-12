# CLI Help Message Strings

cli-help = Print help message
float-format = { 1.0 }
integer-format = { 0010 }
argument = { $arg }
indentation =
    Foo
        Bar

indentation-with-expression =
    Foo

    {"Bar"}
        Baz

select = { $num ->
    [0.0] Nothing
    [1] One thing
    *[other] Some things
}
