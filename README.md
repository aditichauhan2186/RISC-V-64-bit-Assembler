# RISC-V-64-bit-Assembler
CS207: Fundamentals of Computer Systems Laboratory
This project involves the development of a two-pass assembler for the RISC-V 64-bit Instruction Set Architecture (ISA) using C++. The assembler translates RISC-V assembly programs into their corresponding 32-bit machine code, accurately handling instruction encoding, immediate values, and label resolution.

The implementation follows a two-pass architecture: the first pass constructs a symbol table to record label addresses, and the second pass generates the final binary code using this mapping. The assembler supports 37 core RISC-V instructions across all major formats (R, I, S, SB, U, and UJ) and standard data directives such as .text, .data, .word, and .asciz. It also manages memory segments precisely, with designated text and data bases.

This project demonstrates proficiency in low-level system programming, instruction encoding, and ISA-level hardware understanding, forming a key component of processor design and systems-level software development.
