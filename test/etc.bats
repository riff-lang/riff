@test "Ad hoc tests (tables)" {
    run bin/riff test/revkey.rf
    [ "$output" = "002233778899FFBC1234567890449955" ]

    run bin/riff test/aoc2020151.rf
    [ "$output" = "1373" ]
}

@test "Ad hoc tests (tailcalls)" {
    run bin/riff test/eea.rf
    [ "$output" = "71" ]
}
