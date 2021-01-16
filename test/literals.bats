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
    [ "$output" = "255.562" ]

    run bin/riff '0b110100101110101'
    [ "$output" = "26997" ]
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
