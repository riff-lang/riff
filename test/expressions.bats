load conf.bash

@test "Basic arithmetic" {
    $RUNCODE 'print(1+2)'
    [ "$output" -eq 3 ]

    $RUNCODE 'print(4-3)'
    [ "$output" -eq 1 ]
}

@test "Operator precedence" {
    $RUNCODE 'print(-2**4)'
    [ "$output" = "-16" ]
}

@test "Divide by zero returns inf" {
    $RUNCODE 'print(1/0)'
    [ "$output" = "inf" ]
}

@test "Modulus by zero returns nan" {
    $RUNCODE 'print(1%0)'
    [ "$output" = "nan" ]
}

@test "Bitwise operations" {
    $RUNCODE 'print(87&1)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(86&1)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(2|1)'
    [ "$output" -eq 3 ]
}

@test "Logical operations" {
}

@test "null != zero" {
    $RUNCODE 'print(null==0)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null==0.0)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null=="0")'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null=="0.0")'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null!=0)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(null!=0.0)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(null!="0")'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(null!="0.0")'
    [ "$output" -eq 1 ]
}

@test "null != the empty string" {
    $RUNCODE 'print(null=="")'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null!="")'
    [ "$output" -eq 1 ]
}

@test "Empty string != zero" {
    $RUNCODE 'print(""==0)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(""==0.0)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(""=="0")'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(""=="0.0")'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(""!=0)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(""!=0.0)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(""!="0")'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(""!="0.0")'
    [ "$output" -eq 1 ]
}

@test "String equality" {
    $RUNCODE 'print("hello"=="world")'
    [ "$output" -eq 0 ]

    $RUNCODE 'print("hello"=="hello")'
    [ "$output" -eq 1 ]

    $RUNCODE 'print("hello"!="world")'
    [ "$output" -eq 1 ]

    $RUNCODE 'print("hello"!="hello")'
    [ "$output" -eq 0 ]
}

@test "Relational ops w/ mismatched types" {
    $RUNCODE 'print("1.0">1)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print("1.0">=1)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(null>0)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null>=0)'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(null>="")'
    [ "$output" -eq 1 ]

    $RUNCODE 'print(null>"")'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null>1)'
    [ "$output" -eq 0 ]

    $RUNCODE 'print(null<1)'
    [ "$output" -eq 1 ]
}

@test "Substring expressions" {
    $RUNCODE 'print("abc"[0])'
    [ "$output" = "a" ]

    $RUNCODE 'print("abc"[2..])'
    [ "$output" = "c" ]

    $RUNCODE 'print("abc"[-1])'
    [ "$output" = "c" ]

    $RUNCODE 'print(#"abc"[2..])'
    [ "$output" -eq 1 ]

    $RUNCODE 'print("abc"[2..0])'
    [ "$output" = "cba" ]
}

@test "Multi-dimensional subscript expressions" {
    $RUNCODE 't=1 print(t[0])'
    [ "$output" = "1" ]

    $RUNCODE 'a[1,2]=3 a[1,2]++ print(a[1,2])'
    [ "$output" = "4" ]

    $RUNCODE 'local a[1,2]=3 a[1,2]++ print(a[1,2])'
    [ "$output" = "4" ]

    $RUNCODE 'a[1]=4 a[1,2]=3 a[1,2]++ print(a[1,2])'
    [ "$status" -eq 1 ]
}

@test "Misc. edge case expressions" {
    $RUNCODE 'print(a=b=7)'
    [ "$output" -eq 7 ]

    $RUNCODE 'b[1]=4;print(a=b[c=1])'
    [ "$output" -eq 4 ]
    
    $RUNCODE 'c[1]=4;print(a=b+c[d=1])'
    [ "$output" -eq 4 ]
    
    $RUNCODE 'print(a=0+int(c=6,d=7)+1)'
    [ "$output" -eq 7 ]

    $RUNCODE 'a=7+c=1'
    [ "$status" -eq 1 ]

    $RUNCODE 'print(a=7+(c=1))'
    [ "$output" -eq 8 ]
}
