// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/04/Fill.asm

// Runs an infinite loop that listens to the keyboard input.
// When a key is pressed (any key), the program blackens the screen
// by writing 'black' in every pixel;
// the screen should remain fully black as long as the key is pressed. 
// When no key is pressed, the program clears the screen by writing
// 'white' in every pixel;
// the screen should remain fully clear as long as no key is pressed.

// i = 0
@i
M=0

// n = address of KBD (end of SCREEN)
@KBD
D=A
@SCREEN
D=D-A
@n
M=D

// Row pointer
@SCREEN
D=A
@row
M=D

// Tracks whether screen is ready to be drawn
@ready
M=1

(LISTEN)
        @KBD
        D=M

        // If ready to draw and keyboard is presesd draw, otherwise if not ready to draw (ready = 0) and no keys pressed, clear
        @ready
        D=D&M
        
        @DRAW
        D;JGT        
        
        @CLEAR
        D;JEQ
        
        @LISTEN
        0;JMP

(DRAW)
        // If i > n, update state
        @i
        D=M
        @n
        D=D-M
        @UPDATE
        D;JEQ // If i = n goto UPDATE

        // Draw, set RAM[row + i] = -1
        @i
        D=M
        @row
        A=M+D
        M=-1

        // i = i + 1
        @i
        M=M+1
        
        @DRAW
        0;JMP

(CLEAR)
        // If i > n, update state
        @i
        D=M
        @n
        D=D-M
        @CLEAN        
        D;JEQ

        // Draw, set RAM[row + i] = -1
        @i
        D=M
        @row
        A=M+D
        M=0

        // i = i + 1
        @i
        M=M+1

        @CLEAR
        0;JMP

(UPDATE)
        @ready
        M=0

        // If key is held, wait
        @KBD
        D=M
        @UPDATE
        D;JGT
        
        @i
        M=0

        @LISTEN
        0;JMP
        
(CLEAN)
        @ready
        M=1
        
        @i
        M=0
        
        @LISTEN
        0;JMP
