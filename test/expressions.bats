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

@test "null != zero" {
    run dist/riff 'null==0'
    [ "$output" -eq 0 ]

    run dist/riff 'null==0.0'
    [ "$output" -eq 0 ]

    run dist/riff 'null=="0"'
    [ "$output" -eq 0 ]

    run dist/riff 'null=="0.0"'
    [ "$output" -eq 0 ]

    run dist/riff 'null!=0'
    [ "$output" -eq 1 ]

    run dist/riff 'null!=0.0'
    [ "$output" -eq 1 ]

    run dist/riff 'null!="0"'
    [ "$output" -eq 1 ]

    run dist/riff 'null!="0.0"'
    [ "$output" -eq 1 ]
}

@test "null != the empty string" {
    run dist/riff 'null==""'
    [ "$output" -eq 0 ]

    run dist/riff 'null!=""'
    [ "$output" -eq 1 ]
}

@test "Empty string != zero" {
    run dist/riff '""==0'
    [ "$output" -eq 0 ]

    run dist/riff '""==0.0'
    [ "$output" -eq 0 ]

    run dist/riff '""=="0"'
    [ "$output" -eq 0 ]

    run dist/riff '""=="0.0"'
    [ "$output" -eq 0 ]

    run dist/riff '""!=0'
    [ "$output" -eq 1 ]

    run dist/riff '""!=0.0'
    [ "$output" -eq 1 ]

    run dist/riff '""!="0"'
    [ "$output" -eq 1 ]

    run dist/riff '""!="0.0"'
    [ "$output" -eq 1 ]
}

@test "String equality" {
    run dist/riff '"hello"=="world"'
    [ "$output" -eq 0 ]

    run dist/riff '"hello"=="hello"'
    [ "$output" -eq 1 ]

    run dist/riff '"hello"!="world"'
    [ "$output" -eq 1 ]

    run dist/riff '"hello"!="hello"'
    [ "$output" -eq 0 ]
}

@test "Relational ops w/ mismatched types" {
    run dist/riff '"1.0">1'
    [ "$output" -eq 0 ]

    run dist/riff '"1.0">=1'
    [ "$output" -eq 1 ]

    run dist/riff 'null>0'
    [ "$output" -eq 0 ]

    run dist/riff 'null>=0'
    [ "$output" -eq 1 ]

    run dist/riff 'null>=""'
    [ "$output" -eq 1 ]

    run dist/riff 'null>""'
    [ "$output" -eq 0 ]

    run dist/riff 'null>1'
    [ "$output" -eq 0 ]

    run dist/riff 'null<1'
    [ "$output" -eq 1 ]
}
