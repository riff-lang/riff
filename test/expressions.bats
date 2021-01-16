@test "Basic arithmetic" {
    run bin/riff '1+2'
    [ "$output" -eq 3 ]

    run bin/riff '4-3'
    [ "$output" -eq 1 ]
}

@test "Divide by zero returns inf" {
    run bin/riff '1/0'
    [ "$output" = "inf" ]
}

@test "Modulus by zero returns nan" {
    run bin/riff '1%0'
    [ "$output" = "nan" ]
}

@test "Bitwise operations" {
    run bin/riff '87&1'
    [ "$output" -eq 1 ]

    run bin/riff '86&1'
    [ "$output" -eq 0 ]

    run bin/riff '2|1'
    [ "$output" -eq 3 ]
}

@test "Logical operations" {
}

@test "null != zero" {
    run bin/riff 'null==0'
    [ "$output" -eq 0 ]

    run bin/riff 'null==0.0'
    [ "$output" -eq 0 ]

    run bin/riff 'null=="0"'
    [ "$output" -eq 0 ]

    run bin/riff 'null=="0.0"'
    [ "$output" -eq 0 ]

    run bin/riff 'null!=0'
    [ "$output" -eq 1 ]

    run bin/riff 'null!=0.0'
    [ "$output" -eq 1 ]

    run bin/riff 'null!="0"'
    [ "$output" -eq 1 ]

    run bin/riff 'null!="0.0"'
    [ "$output" -eq 1 ]
}

@test "null != the empty string" {
    run bin/riff 'null==""'
    [ "$output" -eq 0 ]

    run bin/riff 'null!=""'
    [ "$output" -eq 1 ]
}

@test "Empty string != zero" {
    run bin/riff '""==0'
    [ "$output" -eq 0 ]

    run bin/riff '""==0.0'
    [ "$output" -eq 0 ]

    run bin/riff '""=="0"'
    [ "$output" -eq 0 ]

    run bin/riff '""=="0.0"'
    [ "$output" -eq 0 ]

    run bin/riff '""!=0'
    [ "$output" -eq 1 ]

    run bin/riff '""!=0.0'
    [ "$output" -eq 1 ]

    run bin/riff '""!="0"'
    [ "$output" -eq 1 ]

    run bin/riff '""!="0.0"'
    [ "$output" -eq 1 ]
}

@test "String equality" {
    run bin/riff '"hello"=="world"'
    [ "$output" -eq 0 ]

    run bin/riff '"hello"=="hello"'
    [ "$output" -eq 1 ]

    run bin/riff '"hello"!="world"'
    [ "$output" -eq 1 ]

    run bin/riff '"hello"!="hello"'
    [ "$output" -eq 0 ]
}

@test "Relational ops w/ mismatched types" {
    run bin/riff '"1.0">1'
    [ "$output" -eq 0 ]

    run bin/riff '"1.0">=1'
    [ "$output" -eq 1 ]

    run bin/riff 'null>0'
    [ "$output" -eq 0 ]

    run bin/riff 'null>=0'
    [ "$output" -eq 1 ]

    run bin/riff 'null>=""'
    [ "$output" -eq 1 ]

    run bin/riff 'null>""'
    [ "$output" -eq 0 ]

    run bin/riff 'null>1'
    [ "$output" -eq 0 ]

    run bin/riff 'null<1'
    [ "$output" -eq 1 ]
}
