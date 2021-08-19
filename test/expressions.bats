@test "Basic arithmetic" {
    run bin/riff 'print 1+2'
    [ "$output" -eq 3 ]

    run bin/riff 'print 4-3'
    [ "$output" -eq 1 ]
}

@test "Operator precedence" {
    run bin/riff 'print -2**4'
    [ "$output" = "-16" ]
}

@test "Divide by zero returns inf" {
    run bin/riff 'print 1/0'
    [ "$output" = "inf" ]
}

@test "Modulus by zero returns nan" {
    run bin/riff 'print 1%0'
    [ "$output" = "nan" ]
}

@test "Bitwise operations" {
    run bin/riff 'print 87&1'
    [ "$output" -eq 1 ]

    run bin/riff 'print 86&1'
    [ "$output" -eq 0 ]

    run bin/riff 'print 2|1'
    [ "$output" -eq 3 ]
}

@test "Logical operations" {
}

@test "null != zero" {
    run bin/riff 'print null==0'
    [ "$output" -eq 0 ]

    run bin/riff 'print null==0.0'
    [ "$output" -eq 0 ]

    run bin/riff 'print null=="0"'
    [ "$output" -eq 0 ]

    run bin/riff 'print null=="0.0"'
    [ "$output" -eq 0 ]

    run bin/riff 'print null!=0'
    [ "$output" -eq 1 ]

    run bin/riff 'print null!=0.0'
    [ "$output" -eq 1 ]

    run bin/riff 'print null!="0"'
    [ "$output" -eq 1 ]

    run bin/riff 'print null!="0.0"'
    [ "$output" -eq 1 ]
}

@test "null != the empty string" {
    run bin/riff 'print null==""'
    [ "$output" -eq 0 ]

    run bin/riff 'print null!=""'
    [ "$output" -eq 1 ]
}

@test "Empty string != zero" {
    run bin/riff 'print ""==0'
    [ "$output" -eq 0 ]

    run bin/riff 'print ""==0.0'
    [ "$output" -eq 0 ]

    run bin/riff 'print ""=="0"'
    [ "$output" -eq 0 ]

    run bin/riff 'print ""=="0.0"'
    [ "$output" -eq 0 ]

    run bin/riff 'print ""!=0'
    [ "$output" -eq 1 ]

    run bin/riff 'print ""!=0.0'
    [ "$output" -eq 1 ]

    run bin/riff 'print ""!="0"'
    [ "$output" -eq 1 ]

    run bin/riff 'print ""!="0.0"'
    [ "$output" -eq 1 ]
}

@test "String equality" {
    run bin/riff 'print "hello"=="world"'
    [ "$output" -eq 0 ]

    run bin/riff 'print "hello"=="hello"'
    [ "$output" -eq 1 ]

    run bin/riff 'print "hello"!="world"'
    [ "$output" -eq 1 ]

    run bin/riff 'print "hello"!="hello"'
    [ "$output" -eq 0 ]
}

@test "Relational ops w/ mismatched types" {
    run bin/riff 'print "1.0">1'
    [ "$output" -eq 0 ]

    run bin/riff 'print "1.0">=1'
    [ "$output" -eq 1 ]

    run bin/riff 'print null>0'
    [ "$output" -eq 0 ]

    run bin/riff 'print null>=0'
    [ "$output" -eq 1 ]

    run bin/riff 'print null>=""'
    [ "$output" -eq 1 ]

    run bin/riff 'print null>""'
    [ "$output" -eq 0 ]

    run bin/riff 'print null>1'
    [ "$output" -eq 0 ]

    run bin/riff 'print null<1'
    [ "$output" -eq 1 ]
}

@test "Misc. edge case expressions" {
    run bin/riff 'print a=b=7'
    [ "$output" -eq 7 ]

    run bin/riff 'b[1]=4;print a=b[c=1]'
    [ "$output" -eq 4 ]
    
    run bin/riff 'c[1]=4;print a=b+c[d=1]'
    [ "$output" -eq 4 ]
    
    run bin/riff 'print a=0+int(c=6,d=7)+1'
    [ "$output" -eq 7 ]

    run bin/riff 'a=7+c=1'
    [ "$status" -eq 1 ]

    run bin/riff 'print a=7+(c=1)'
    [ "$output" -eq 8 ]
}
