@test "fmt() conversions" {
    run bin/riff 'print fmt("%b", 0)'
    [ "$output" = "0" ]

    run bin/riff 'print fmt("%.b", 0)'
    [ "$output" = "" ]

    run bin/riff 'print fmt("%b", -1)'
    [ "$output" = "1111111111111111111111111111111111111111111111111111111111111111" ]

    run bin/riff 'print fmt("%b", -2)'
    [ "$output" = "1111111111111111111111111111111111111111111111111111111111111110" ]

    run bin/riff 'print fmt("%b", 1<<63)'
    [ "$output" = "1000000000000000000000000000000000000000000000000000000000000000" ]
}
