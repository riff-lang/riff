load conf.bash

@test "Ad hoc tests (tables)" {
    $RUNFILE test/revkey.rf
    [ "$output" = "002233778899FFBC1234567890449955" ]

    $RUNFILE test/aoc2020151.rf
    [ "$output" = "1373" ]
}

@test "Ad hoc tests (tailcalls)" {
    $RUNFILE test/eea.rf
    [ "$output" = "71" ]
}
