@test "Addition" {
    run dist/riff "1+2"
    [ "$output" -eq "3" ]
 }

 @test "Subtraction" {
    run dist/riff "4-3"
    [ "$output" -eq "1" ]
 }
