@test "Basic arithmetic" {
    run dist/riff '1+2'
    [ "$output" -eq 3 ]

    run dist/riff '4-3'
    [ "$output" -eq 1 ]
}

@test "Divide by zero returns inf" {
    run dist/riff '1/0'
    [ "$output" = "inf" ]
}

@test "Modulus by zero returns nan" {
    run dist/riff '1%0'
    [ "$output" = "nan" ]
}

@test "Bitwise operations" {
    run dist/riff '87&1'
    [ "$output" -eq 1 ]

    run dist/riff '86&1'
    [ "$output" -eq 0 ]

    run dist/riff '2|1'
    [ "$output" -eq 3 ]
}

@test "Logical operations" {
}

@test "Relational operations" {
}
