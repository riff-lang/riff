@test "Numeric literals" {
    run dist/riff '1'
    [ "$output" -eq 1 ]

    run dist/riff '0.5'
    [ "$output" = "0.5" ]

    run dist/riff '.2'
    [ "$output" = "0.2" ]

    run dist/riff '.342'
    [ "$output" = "0.342" ]

    run dist/riff '0xabba7e'
    [ "$output" = "11254398" ]

    run dist/riff '0x.f'
    [ "$output" = "0.9375" ]

    run dist/riff '0xff.9'
    [ "$output" = "255.562" ]

    run dist/riff '0b110100101110101'
    [ "$output" = "26997" ]
}

@test "Character literals" {
    run dist/riff "'A'"
    [ "$output" -eq 65 ]
}

@test "String literals" {
    run dist/riff '"hello"'
    [ "$output" = "hello" ]

    run dist/riff '"multi\
line"'
    [ "$output" = "multiline" ]
}
