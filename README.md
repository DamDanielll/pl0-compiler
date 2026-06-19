# PL/0 Compiler

A compiler written in C that translates PL/0 source programs into PM/0 assembly. Built for COP 3402 - Systems Software at UCF.

## How it works

Runs in two integrated phases:

1. **Scanner** — Tokenizes PL/0 source code, handles block comments (`/* */`), and flags lexical errors.
2. **Parser / Code Generator** — Recursive-descent parser for the full PL/0 grammar. Generates PM/0 assembly instructions while tracking lexicographical levels for correct `LOD`, `STO`, and `CAL` generation. Supports nested procedures and the `call` statement.

## Compile & Run

```bash
gcc -O2 -Wall -std=c11 -o parsercodegen_comp parsercodegen_comp.c
./parsercodegen_comp <input_file.txt>
```

## Output

- Assembly code table and symbol table printed to stdout
- Numeric instruction output written to `elf.txt`

## Authors

Daniel Henriquez & Brayden Coggin
