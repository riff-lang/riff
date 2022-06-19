@test "Basic arithmetic" {
    run bin/riff -e 'print(1+2)'
    [ "$output" -eq 3 ]

    run bin/riff -e 'print(4-3)'
    [ "$output" -eq 1 ]
}

@test "Operator precedence" {
    run bin/riff -e 'print(-2**4)'
    [ "$output" = "-16" ]
}

@test "Divide by zero returns inf" {
    run bin/riff -e 'print(1/0)'
    [ "$output" = "inf" ]
}

@test "Modulus by zero returns nan" {
    run bin/riff -e 'print(1%0)'
    [ "$output" = "nan" ]
}

@test "Bitwise operations" {
    run bin/riff -e 'print(87&1)'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(86&1)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(2|1)'
    [ "$output" -eq 3 ]
}

@test "Logical operations" {
}

@test "null != zero" {
    run bin/riff -e 'print(null==0)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null==0.0)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null=="0")'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null=="0.0")'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null!=0)'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(null!=0.0)'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(null!="0")'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(null!="0.0")'
    [ "$output" -eq 1 ]
}

@test "null != the empty string" {
    run bin/riff -e 'print(null=="")'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null!="")'
    [ "$output" -eq 1 ]
}

@test "Empty string != zero" {
    run bin/riff -e 'print(""==0)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(""==0.0)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(""=="0")'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(""=="0.0")'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(""!=0)'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(""!=0.0)'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(""!="0")'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(""!="0.0")'
    [ "$output" -eq 1 ]
}

@test "String equality" {
    run bin/riff -e 'print("hello"=="world")'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print("hello"=="hello")'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print("hello"!="world")'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print("hello"!="hello")'
    [ "$output" -eq 0 ]
}

@test "Relational ops w/ mismatched types" {
    run bin/riff -e 'print("1.0">1)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print("1.0">=1)'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(null>0)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null>=0)'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(null>="")'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print(null>"")'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null>1)'
    [ "$output" -eq 0 ]

    run bin/riff -e 'print(null<1)'
    [ "$output" -eq 1 ]
}

@test "Substring expressions" {
    run bin/riff -e 'print("abc"[0])'
    [ "$output" = "a" ]

    run bin/riff -e 'print("abc"[2..])'
    [ "$output" = "c" ]

    run bin/riff -e 'print("abc"[-1])'
    [ "$output" = "c" ]

    run bin/riff -e 'print(#"abc"[2..])'
    [ "$output" -eq 1 ]

    run bin/riff -e 'print("abc"[2..0])'
    [ "$output" = "cba" ]
}

@test "Multi-dimensional subscript expressions" {
    run bin/riff -e 't=1 print(t[0])'
    [ "$output" = "1" ]

    run bin/riff -e 'a[1,2]=3 a[1,2]++ print(a[1,2])'
    [ "$output" = "4" ]

    run bin/riff -e 'local a[1,2]=3 a[1,2]++ print(a[1,2])'
    [ "$output" = "4" ]

    run bin/riff -e 'a[1]=4 a[1,2]=3 a[1,2]++ print(a[1,2])'
    [ "$status" -eq 1 ]
}

@test "Misc. edge case expressions" {
    run bin/riff -e 'print(a=b=7)'
    [ "$output" -eq 7 ]

    run bin/riff -e 'b[1]=4;print(a=b[c=1])'
    [ "$output" -eq 4 ]
    
    run bin/riff -e 'c[1]=4;print(a=b+c[d=1])'
    [ "$output" -eq 4 ]
    
    run bin/riff -e 'print(a=0+int(c=6,d=7)+1)'
    [ "$output" -eq 7 ]

    run bin/riff -e 'a=7+c=1'
    [ "$status" -eq 1 ]

    run bin/riff -e 'print(a=7+(c=1))'
    [ "$output" -eq 8 ]
}
