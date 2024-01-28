// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/04/Mult.asm

// Multiplies R0 and R1 and stores the result in R2.
// Assumes that R0 >= 0, R1 >= 0, and R0 * R1 < 32768.
// (R0, R1, R2 refer to RAM[0], RAM[1], and RAM[2], respectively.)

@R2
M=0 // RAM[2]=0

@R1
D=M
@END
D;JEQ // END if RAM[1]=0

@n
M=D // n=RAM[1]

(LOOP)
        @R0
        D=M
        @R2
        M=M+D // RAM[2]=RAM[2]+RAM[1]
                
        @n
        MD=M-1 // n=n-1
        @END
        D;JEQ // if n = 0 goto END
        
        @LOOP
        0;JMP
(END)
        @END
        0;JMP
