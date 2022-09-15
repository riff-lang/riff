load conf.bash

@test "Numeric literals" {
    $RUNCODE 'print(1)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(0.5)'
    [ "$output" = "0.5" ]

    $RUNCODE 'print(.2)'
    [ "$output" = "0.2" ]

    $RUNCODE 'print(.342)'
    [ "$output" = "0.342" ]

    $RUNCODE 'print(0xabba7e)'
    [ "$output" = "11254398" ]

    $RUNCODE 'print(0x.f)'
    [ "$output" = "0.9375" ]

    $RUNCODE 'print(0xff.9)'
    [ "$output" = "255.5625" ]

    $RUNCODE 'print(0b110100101110101)'
    [ "$output" = "26997" ]
}

@test "Decimal INT64_MAX" {
    $RUNCODE 'print(9223372036854775807)'
    [ "$output" = "9223372036854775807" ]
}

@test "Decimal INT64_MIN" {
    skip "As of v0.2, decimal INT64_MIN will be a float"
    $RUNCODE 'print(-9223372036854775808)'
    [ "$output" = "-9223372036854775808" ]
}

@test "Decimal INT64_MAX+1" {
    skip "Test should check for numeric equivalence, not string"
    $RUNCODE 'print(9223372036854775808)'
    [ "$output" = "9.22337e+18" ]
}

@test "Decimal INT64_MIN-1" {
    skip "Test should check for numeric equivalence, not string"
    $RUNCODE 'print(-9223372036854775809)'
    [ "$output" = "-9.22337e+18" ]
}

@test "Hexadecimal INT64_MAX" {
    $RUNCODE 'print(0x7fffffffffffffff)'
    [ "$output" = "9223372036854775807" ]
}

@test "Hexadecimal INT64_MIN" {
    $RUNCODE 'print(0x8000000000000000)'
    [ "$output" = "-9223372036854775808" ]
}

@test "Character literals" {
    $RUNCODE "print('A')"
    [ "$output" -eq 65 ]

    $RUNCODE "print('abcd')"
    [ "$output" = "1633837924" ]

    $RUNCODE "print('abcdefgh')"
    [ "$output" = "7017280452245743464" ]
}

@test "String literals" {
    $RUNCODE 'print("hello")'
    [ "$output" = "hello" ]

    $RUNCODE 'print("multi\
line")'
    [ "$output" = "multiline" ]
}
