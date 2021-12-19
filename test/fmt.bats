@test "fmt() conversions" {
    run bin/riff -e 'printf("%b\n", 0)'
    [ "$output" = "0" ]

    run bin/riff -e 'printf("%.b\n", 0)'
    [ "$output" = "" ]

    run bin/riff -e 'printf("%b\n", -1)'
    [ "$output" = "1111111111111111111111111111111111111111111111111111111111111111" ]

    run bin/riff -e 'printf("%b\n", -2)'
    [ "$output" = "1111111111111111111111111111111111111111111111111111111111111110" ]

    run bin/riff -e 'printf("%b\n", 1<<63)'
    [ "$output" = "1000000000000000000000000000000000000000000000000000000000000000" ]
}
