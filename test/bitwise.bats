@test "Bitwise AND" {
    run dist/riff "87 & 1"
    [ "$output" -eq "1" ]

    run dist/riff "86 & 1"
    [ "$output" -eq "0" ]
}

@test "Bitwise OR" {
    run dist/riff "2 | 1"
    [ "$output" -eq "3" ]
}
