@test "Numeric literals" {
    run bin/riff '1'
    [ "$output" -eq 1 ]

    run bin/riff '0.5'
    [ "$output" = "0.5" ]

    run bin/riff '.2'
    [ "$output" = "0.2" ]

    run bin/riff '.342'
    [ "$output" = "0.342" ]

    run bin/riff '0xabba7e'
    [ "$output" = "11254398" ]

    run bin/riff '0x.f'
    [ "$output" = "0.9375" ]

    run bin/riff '0xff.9'
    [ "$output" = "255.5625" ]

    run bin/riff '0b110100101110101'
    [ "$output" = "26997" ]
}

@test "Decimal INT64_MAX" {
    run bin/riff '9223372036854775807'
    [ "$output" = "9223372036854775807" ]
}

@test "Decimal INT64_MIN" {
    skip "As of v0.2, decimal INT64_MIN will be a float"
    run bin/riff '-9223372036854775808'
    [ "$output" = "-9223372036854775808" ]
}

@test "Decimal INT64_MAX+1" {
    skip "Test should check for numeric equivalence, not string"
    run bin/riff '9223372036854775808'
    [ "$output" = "9.22337e+18" ]
}

@test "Decimal INT64_MIN-1" {
    skip "Test should check for numeric equivalence, not string"
    run bin/riff '-9223372036854775809'
    [ "$output" = "-9.22337e+18" ]
}

@test "Hexadecimal INT64_MAX" {
    run bin/riff '0x7fffffffffffffff'
    [ "$output" = "9223372036854775807" ]
}

@test "Hexadecimal INT64_MIN" {
    run bin/riff '0x8000000000000000'
    [ "$output" = "-9223372036854775808" ]
}

@test "Character literals" {
    run bin/riff "'A'"
    [ "$output" -eq 65 ]
}

@test "String literals" {
    run bin/riff '"hello"'
    [ "$output" = "hello" ]

    run bin/riff '"multi\
line"'
    [ "$output" = "multiline" ]
}
