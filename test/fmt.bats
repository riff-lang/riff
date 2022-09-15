load conf.bash

@test "fmt() conversions" {
    $RUNCODE 'printf("%b\n", 0)'
    [ "$output" = "0" ]

    $RUNCODE 'printf("%.b\n", 0)'
    [ "$output" = "" ]

    $RUNCODE 'printf("%b\n", -1)'
    [ "$output" = "1111111111111111111111111111111111111111111111111111111111111111" ]

    $RUNCODE 'printf("%b\n", -2)'
    [ "$output" = "1111111111111111111111111111111111111111111111111111111111111110" ]

    $RUNCODE 'printf("%b\n", 1<<63)'
    [ "$output" = "1000000000000000000000000000000000000000000000000000000000000000" ]
}
