/*
Assignment:
HW4 - Parser/Code Generator Complete Version for PL/0
    (procedures + lexicographical level management)

Author(s): Brayden Coggin, Daniel Henriquez

Language: C (only)

To Compile:
    gcc -O2 -Wall -std=c11 -o parsercodegen_comp parsercodegen_comp.c

To Execute (on Eustis):
    ./parsercodegen_comp <input_file.txt>

where:
    <input_file.txt> is the path to the PL/0 source program

Notes:
    - Single integrated program: scanner + parser + code gen
    - parsercodegen_comp.c accepts ONE command-line argument
    - Scanner runs internally (no intermediate token file)
    - Implements recursive-descent parser for the full PL/0 grammar
    including procedure declarations and the call statement
    - Tracks lexicographical levels (nesting depth) for every symbol
    - Generates PM/0 assembly code with proper L fields for LOD/STO/CAL
    and emits CAL/RTN instructions for procedures
    (see Appendix A for the ISA)
    - All development and testing performed on Eustis

Class: COP 3402 - Systems Software - Spring 2026

Instructor: Dr. Jie Lin

Due Date: See Webcourses for the posted due date and time.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Token types from the hw assignment
typedef enum{
    skipsym = 1,  // Skip / ignore token
    identsym,     // Identifier
    numbersym,    // Number
    beginsym,     // begin
    endsym,       // end
    ifsym,        // if
    fisym,        // fi
    thensym,      // then
    whilesym,     // while
    dosym,        // do
    odsym,        // od
    callsym,      // call
    constsym,     // const
    varsym,       // var
    procsym,      // procedure
    writesym,     // write
    readsym,      // read
    elsesym,      // else
    plussym,      // +
    minussym,     // -
    multsym,      // *
    slashsym,     // /
    eqsym,        // =
    neqsym,       // <>
    lessym,       // <
    leqsym,       // <=
    gtrsym,       // >
    geqsym,       // >=
    lparentsym,   // (
    rparentsym,   // )
    commasym,     // ,
    semicolonsym, // ;
    periodsym,    // .
    becomessym    // :=
} TokenType;

// Struct to store each token with lexeme, type, number value, and error code
typedef struct{
    char lexeme[12];
    int type;
    int value;
    int error;
} Token;

// Struct to store symbol table entries
#define MAX_SYMBOL_TABLE_SIZE 500
typedef struct{
    int kind;      // const = 1, var = 2, proc = 3
    char name[12]; // name up to 11 chars
    int val;       // number (ASCII value)
    int level;     // L level
    int addr;      // M address
    int mark;      // to indicate unavailable or deleted
} symbol;

// Struct to store a single pm/0 instruction
typedef struct{
    int op, l, m;
} Instruction;

// Array to store all tokens
Token tokens[2000];
int tokenCount = 0;
int tokenIdx = 0;

// Symbol table
symbol symbol_table[MAX_SYMBOL_TABLE_SIZE];
int symCount = 0;

// Generated instructions
Instruction code[2000];
int codeIdx = 0;

// Output file handle
FILE *elfFile = NULL;

int currentLevel = 0;

// Prints error to terminal and elf.txt then exits
void reportError(const char *msg){
    printf("%s\n", msg);
    fprintf(elfFile, "%s\n", msg);
    fclose(elfFile);
    exit(1);
}

// Scanner adapted from lex.c with block comment support
void scan(FILE *fp){
    int c;

    // loops through entire file 
    while ((c = fgetc(fp)) != EOF){

        // Skips whitespace
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r'){
            continue;
        }

        // cheks for block comments /**/
        if(c == '/'){
            int next = fgetc(fp);

            if (next == '*'){
                int prev = 0;


                while ((c = fgetc(fp)) != EOF){
                    if (prev == '*' && c == '/'){
                        break;
                    }
                    prev = c;
                }
                continue;
            }

            // Not a comment treat it is division
            if (next != EOF){
                ungetc(next, fp);
            }
            tokens[tokenCount].type = slashsym;
            tokens[tokenCount].lexeme[0] = '/';
            tokens[tokenCount].lexeme[1] = '\0';
            tokens[tokenCount].error = 0;
            tokenCount++;
            continue;
        }

        // Identifiers or reserved words
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')){
            char buffer[50];
            int i = 0;
            int length = 0;

            //adds first character to buffer
            if (i < (int)sizeof(buffer) - 1){
                buffer[i++] = c;
            }
            length++;

            // keeps reading
            while (1){
                c = fgetc(fp);
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')){
                    if (i < (int)sizeof(buffer) - 1){
                        buffer[i++] = c;
                    }
                    length++;
                }
                else{
                    if (c != EOF){
                        ungetc(c, fp);
                    }
                    break;
                }
            }
            buffer[i] = '\0';
            strncpy(tokens[tokenCount].lexeme, buffer, 11);
            tokens[tokenCount].lexeme[11] = '\0';
            tokens[tokenCount].error = 0;

            // Checks length
            if (length > 11){
                tokens[tokenCount].error = 1;
                tokens[tokenCount].type = skipsym;
            }

            // Checks reserved words
            else if (strcmp(buffer, "begin") == 0){
                tokens[tokenCount].type = beginsym;
            }
            else if (strcmp(buffer, "end") == 0){
                tokens[tokenCount].type = endsym;
            }
            else if (strcmp(buffer, "if") == 0){
                tokens[tokenCount].type = ifsym;
            }
            else if (strcmp(buffer, "fi") == 0){
                tokens[tokenCount].type = fisym;
            }
            else if (strcmp(buffer, "then") == 0){
                tokens[tokenCount].type = thensym;
            }
            else if (strcmp(buffer, "while") == 0){
                tokens[tokenCount].type = whilesym;
            }
            else if (strcmp(buffer, "do") == 0){
                tokens[tokenCount].type = dosym;
            }
            else if (strcmp(buffer, "od") == 0){
                tokens[tokenCount].type = odsym;
            }
            else if (strcmp(buffer, "call") == 0){
                tokens[tokenCount].type = callsym;
            }
            else if (strcmp(buffer, "const") == 0){
                tokens[tokenCount].type = constsym;
            }
            else if (strcmp(buffer, "var") == 0){
                tokens[tokenCount].type = varsym;
            }
            else if (strcmp(buffer, "procedure") == 0){
                tokens[tokenCount].type = procsym;
            }
            else if (strcmp(buffer, "write") == 0){
                tokens[tokenCount].type = writesym;
            }
            else if (strcmp(buffer, "read") == 0){
                tokens[tokenCount].type = readsym;
            }
            else if (strcmp(buffer, "else") == 0){
                tokens[tokenCount].type = elsesym;
            }
            else{
                tokens[tokenCount].type = identsym;
                tokens[tokenCount].value = 0;
            }
            tokenCount++;
        }

        // numbers
        else if (c >= '0' && c <= '9'){
            char buffer[50];
            int i = 0;
            int length = 0;

            if (i < (int)sizeof(buffer) - 1){
                buffer[i++] = c;
            }
            length++;

            // read number
            while (1){
                c = fgetc(fp);
                if (c >= '0' && c <= '9'){
                    if (i < (int)sizeof(buffer) - 1){
                        buffer[i++] = c;
                    }
                    length++;
                }
                else{
                    if (c != EOF){
                        ungetc(c, fp);
                    }
                    break;
                }
            }

            buffer[i] = '\0';

            strncpy(tokens[tokenCount].lexeme, buffer, 11);
            tokens[tokenCount].lexeme[11] = '\0';
            tokens[tokenCount].error = 0;

            // Check number length
            if (length > 5){
                tokens[tokenCount].error = 2;
                tokens[tokenCount].type = skipsym;
            }
            else{
                tokens[tokenCount].type = numbersym;
                tokens[tokenCount].value = atoi(buffer);
            }
            tokenCount++;
        }

        // Symbols, checks single and two character symbols
        else{
            tokens[tokenCount].error = 0;
            tokens[tokenCount].lexeme[0] = c;
            tokens[tokenCount].lexeme[1] = '\0';
            if (c == '+'){
                tokens[tokenCount].type = plussym;
            }
            else if (c == '-'){
                tokens[tokenCount].type = minussym;
            }
            else if (c == '*'){
                tokens[tokenCount].type = multsym;
            }
            else if (c == '('){
                tokens[tokenCount].type = lparentsym;
            }
            else if (c == ')'){
                tokens[tokenCount].type = rparentsym;
            }
            else if (c == ','){
                tokens[tokenCount].type = commasym;
            }
            else if (c == ';'){
                tokens[tokenCount].type = semicolonsym;
            }
            else if (c == '.'){
                tokens[tokenCount].type = periodsym;
            }
            else if (c == '='){
                tokens[tokenCount].type = eqsym;
            }

            // Two character symbols
            else if (c == '<'){
                int next = fgetc(fp);
                if (next == '='){
                    tokens[tokenCount].type = leqsym;
                    strcpy(tokens[tokenCount].lexeme, "<=");
                }
                else if (next == '>'){
                    tokens[tokenCount].type = neqsym;
                    strcpy(tokens[tokenCount].lexeme, "<>");
                }
                else{
                    tokens[tokenCount].type = lessym;
                    if (next != EOF){
                        ungetc(next, fp);
                    }
                }
            }
            else if (c == '>'){
                int next = fgetc(fp);
                if (next == '='){
                    tokens[tokenCount].type = geqsym;
                    strcpy(tokens[tokenCount].lexeme, ">=");
                }
                else{
                    tokens[tokenCount].type = gtrsym;
                    if (next != EOF){
                        ungetc(next, fp);
                    }
                }
            }
            else if (c == ':'){
                int next = fgetc(fp);
                if (next == '='){
                    tokens[tokenCount].type = becomessym;
                    strcpy(tokens[tokenCount].lexeme, ":=");
                }
                else{
                    tokens[tokenCount].error = 3;
                    tokens[tokenCount].type = skipsym;
                    if (next != EOF){
                        ungetc(next, fp);
                    }
                }
            }

            // Invalid character
            else{
                tokens[tokenCount].error = 3;
                tokens[tokenCount].type = skipsym;
            }
            tokenCount++;
        }
    }
}

// adds a new instruction to code aray
void emit(int op, int l, int m){
    code[codeIdx].op = op;
    code[codeIdx].l = l;
    code[codeIdx].m = m;
    codeIdx++;
}

// looks for a symbol in the table and returns its index
int symbolTableCheck(const char *name){
    for (int i = symCount - 1; i >= 0; i--){
        if (symbol_table[i].mark == 0 && strcmp(symbol_table[i].name, name) == 0){
            return i;
        }
    }
    return -1;
}

// checks if symbol name already exists
int symbolTableCheckDup(const char *name){
    for (int i = symCount - 1; i >= 0; i--){
        if (symbol_table[i].mark == 0 && symbol_table[i].level == currentLevel && strcmp(symbol_table[i].name, name) == 0){
            return i;
        }
    }
    return -1;
}

// Returns the current token without moving forward
Token currentToken(void){
    if (tokenIdx < tokenCount){
        return tokens[tokenIdx];
    }
    Token eof;
    memset(&eof, 0, sizeof(eof));
    return eof;
}

// moves to next token
void nextToken(void){
    if (tokenIdx < tokenCount){
        tokenIdx++;
    }
}

// Declarations for parser functions
void expression(void);
void statement(void);
void condition(void);
void term(void);
void factor(void);
void block(int level);
void procedureDeclaration(int level);
void markSymbols(int level);

// handles identifiers numbers or expression
void factor(void){
    if (currentToken().type == identsym){
        int idx = symbolTableCheck(currentToken().lexeme);

        // check if varaible exists
        if (idx == -1){
            reportError("Error: undeclared identifier");
        }

        // load constant or variable value
        if (symbol_table[idx].kind == 1){
            emit(1, 0, symbol_table[idx].val); // LIT for constants
        }
        else if (symbol_table[idx].kind == 2){
            int L = currentLevel - symbol_table[idx].level;
            emit(3, L, symbol_table[idx].addr);
        }
        else{
            reportError("Error: arithmetic equations must contain operands, parentheses, numbers, or symbols");
        }
        nextToken();
    }

    else if (currentToken().type == numbersym){
        emit(1, 0, currentToken().value); // LIT for number literals
        nextToken();
    }

    else if (currentToken().type == lparentsym){
        nextToken();
        expression();

        // chcek for closing parenthesis
        if (currentToken().type != rparentsym){
            reportError("Error: right parenthesis must follow left parenthesis");
        }
        nextToken();
    }
    else{
        reportError("Error: arithmetic equations must contain operands, parentheses, numbers, or symbols");
    }
}

// handles multiplication and division
void term(void){
    factor();
    while (currentToken().type == multsym || currentToken().type == slashsym){
        int op = currentToken().type;
        nextToken();
        factor();
        if (op == multsym){
            emit(2, 0, 4);
        }
        else{
            emit(2, 0, 5); // OPR MUL or DIV
        }
    }
}

// handles + - unary minus
void expression(void){
    int negate = (currentToken().type == minussym);
    if (negate){
        nextToken();
    }
    term();

    // handles negate
    if (negate){
        emit(2, 0, 1);
    }

    // handles + -
    while (currentToken().type == plussym || currentToken().type == minussym){
        int op = currentToken().type;
        nextToken();
        term();
        emit(2, 0, (op == plussym) ? 2 : 3); // OPR ADD or SUB
    }
}

// handles relational conditions
void condition(void){
    expression();
    int rel = currentToken().type;
    if (rel == eqsym){
        nextToken();
        expression();
        emit(2, 0, 6); // OPR EQL
    }
    else if (rel == neqsym){
        nextToken();
        expression();
        emit(2, 0, 7); // OPR NEQ
    }
    else if (rel == lessym){
        nextToken();
        expression();
        emit(2, 0, 8); // OPR LSS
    }
    else if (rel == leqsym){
        nextToken();
        expression();
        emit(2, 0, 9); // OPR LEQ
    }
    else if (rel == gtrsym){
        nextToken();
        expression();
        emit(2, 0, 10); // OPR GTR
    }
    else if (rel == geqsym){
        nextToken();
        expression();
        emit(2, 0, 11); // OPR GEQ
    }
    else{
        reportError("Error: condition must contain comparison operator");
    }
}

// handles all statement types
void statement(void){

    // Assignment statement
    if (currentToken().type == identsym){
        int idx = symbolTableCheck(currentToken().lexeme);
        if (idx == -1){
            reportError("Error: undeclared identifier");
        }
        if (symbol_table[idx].kind != 2){
            reportError("Error: only variable values may be altered");
        }
        nextToken();
        if (currentToken().type != becomessym){
            reportError("Error: assignment statements must use :=");
        }
        nextToken();
        expression();

        int L = currentLevel - symbol_table[idx].level;
        emit(4, L, symbol_table[idx].addr);
    }

    // handles procedure call
    else if (currentToken().type == callsym){

        nextToken();

        // must be follwoed by procedure name
        if (currentToken().type != identsym){
            reportError("Error: procedure and call keywords must be followed by identifier");
        }

        // find procedure in symbol table
        int idx = symbolTableCheck(currentToken().lexeme);

        if (idx == -1){
            reportError("Error: undeclared identifier");
        }

        // make sure identifier is actually in procedure
        if (symbol_table[idx].kind != 3){
            reportError("Error: call must be followed by a procedure identifier");
        }

        // compute lexical level difference and M is starting address
        int L = currentLevel - symbol_table[idx].level;
        int M = symbol_table[idx].addr;

        // genreates CAL instruction to jump to procedure
        emit(5, L, M);

        nextToken();
    }

    // begin ... end block
    else if (currentToken().type == beginsym){
        nextToken();
        statement();
        while (currentToken().type == semicolonsym){
            nextToken();
            statement();
        }
        if (currentToken().type != endsym){
            reportError("Error: begin must be followed by end");
        }
        nextToken();
    }

    // If then  [else] fi
    else if (currentToken().type == ifsym){
        nextToken();
        condition();
        int jpcIdx = codeIdx;
        emit(8, 0, 0);

        if (currentToken().type != thensym){
            reportError("Error: if must be followed by then");
        }

        nextToken();
        statement();

        if (currentToken().type == semicolonsym){
            nextToken();
        }

        if (currentToken().type == elsesym){
            int jmpIdx = codeIdx;
            emit(7, 0, 0);
            code[jpcIdx].m = codeIdx * 3;
            nextToken();
            statement();
            code[jmpIdx].m = codeIdx * 3; 
        }
        else{
            code[jpcIdx].m = codeIdx * 3;
        }
        if (currentToken().type != fisym){
            reportError("Error: if-then statement must end with fi");
        }
        nextToken();
    }

    // While do od
    else if (currentToken().type == whilesym){
        nextToken();
        int loopIdx = codeIdx; // Top of loop for back-jump
        condition();
        if (currentToken().type != dosym){
            reportError("Error: while must be followed by do");
        }
        nextToken();
        int jpcIdx = codeIdx;
        emit(8, 0, 0); // JPC placeholder
        statement();
        if (currentToken().type == semicolonsym){
            nextToken();
        }
        if (currentToken().type != odsym){
            reportError("Error: do must be followed by od");
        }
        nextToken();
        emit(7, 0, loopIdx * 3); 
        code[jpcIdx].m = codeIdx * 3; 
    }

    // Read input
    else if (currentToken().type == readsym){
        nextToken();
        if (currentToken().type != identsym){
            reportError("Error: const, var, and read keywords must be followed by identifier");
        }
        int idx = symbolTableCheck(currentToken().lexeme);
        if (idx == -1){
            reportError("Error: undeclared identifier");
        }
        if (symbol_table[idx].kind != 2){
            reportError("Error: only variable values may be altered");
        }
        int L = currentLevel - symbol_table[idx].level;
        nextToken();
        emit(9, 0, 2);                      // SYS READ
        emit(4, L, symbol_table[idx].addr); // STO
    }

    // Write output
    else if (currentToken().type == writesym){
        nextToken();
        expression();
        emit(9, 0, 1); // SYS WRITE
    }
}

// handles constant declarations and stores them in symbol table
void constDeclaration(void){
    if (currentToken().type != constsym){
        return;
    }

    //moves to first identifier
    nextToken();

    // check for valid identifier
    if (currentToken().type != identsym){
        reportError("Error: const, var, and read keywords must be followed by identifier");
    }
    while(1){
        // checks for duplicate
        if (symbolTableCheckDup(currentToken().lexeme) != -1){
            reportError("Error: symbol name has already been declared");
        }

        // saves identifier name
        char savedName[12];
        strncpy(savedName, currentToken().lexeme, 11);
        savedName[11] = '\0';
        nextToken();

        // must have = 
        if (currentToken().type != eqsym){
            reportError("Error: constants must be assigned with =");
        }
        nextToken();
        // must be a number
        if (currentToken().type != numbersym){
            reportError("Error: constants must be assigned an integer value");
        }
        
        // add constant to symbol table
        symbol_table[symCount].kind = 1;
        strncpy(symbol_table[symCount].name, savedName, 11);
        symbol_table[symCount].name[11] = '\0';
        symbol_table[symCount].val = currentToken().value;
        symbol_table[symCount].level = currentLevel;
        symbol_table[symCount].addr = 0;
        symbol_table[symCount].mark = 0;
        symCount++;
        nextToken();

        // stop with no comma
        if (currentToken().type != commasym){
            break;
        }

        // consumes comma
        nextToken();

        // next must be identifier
        if (currentToken().type != identsym){
            reportError("Error: const, var, and read keywords must be followed by identifier");
        }
    } 

    if (currentToken().type != semicolonsym) {
        reportError("Error: constant and variable declarations must be followed by a semicolon");
    }
    nextToken();
}

// Var declaration: [ "var" ident { "," ident } ";" ]
// Returns number of variables declared
int varDeclaration(void){
    int numVars = 0;

    if (currentToken().type != varsym){
        return 0;
    }

    nextToken();

    // check for valid identifier
    if (currentToken().type != identsym){
        reportError("Error: const, var, and read keywords must be followed by identifier");
    }

    while(1){
        // check for duplicate name
        if (symbolTableCheckDup(currentToken().lexeme) != -1){
            reportError("Error: symbol name has already been declared");
        }

        // add variable to symbol table
        symbol_table[symCount].kind = 2;
        strncpy(symbol_table[symCount].name, currentToken().lexeme, 11);
        symbol_table[symCount].name[11] = '\0';
        symbol_table[symCount].val = 0;
        symbol_table[symCount].level = currentLevel;
        symbol_table[symCount].addr = numVars + 3;
        symbol_table[symCount].mark = 0;

        symCount++;
        numVars++;

        nextToken();

        if (currentToken().type != commasym){
            break;
        }

        // consumes comma
        nextToken();

        if (currentToken().type != identsym){
            reportError("Error: const, var, and read keywords must be followed by identifier");
        }
    } 

    if (currentToken().type != semicolonsym){
        reportError("Error: constant and variable declarations must be followed by a semicolon");
    }
    nextToken();
    return numVars;
}

// handles procedure declarations
void procedureDeclaration(int level){

    // allows multiple procedures in a row
    while (currentToken().type == procsym){

        nextToken();

        // procedure must have a name
        if (currentToken().type != identsym){
            reportError("Error: procedure and call keywords must be followed by identifier");
        }

        char name[12];
        strcpy(name, currentToken().lexeme);

        // prevent duplicate names 
        if (symbolTableCheckDup(name) != -1){
            reportError("Error: symbol name has already been declared");
        }

        // add procedure to symbol table
        symbol_table[symCount].kind = 3;
        strcpy(symbol_table[symCount].name, name);
        symbol_table[symCount].val = 0;

        // procedure is declared at current level
        symbol_table[symCount].level = level;

        // store address where procedure code begins
        symbol_table[symCount].addr = codeIdx * 3;

        symbol_table[symCount].mark = 0;
        symCount++;

        nextToken();

        if (currentToken().type != semicolonsym){
            reportError("Error: procedure declarations must be followed by a semicolon");
        }

        nextToken();

        // recursive process
        block(level + 1);

        if (currentToken().type != semicolonsym){
            reportError("Error: procedure declarations must be followed by a semicolon");
        }

        nextToken();
        
        // return from procedure
        emit(2, 0, 0); 
    }
}

// handles a block of code with its own level
void block(int level){

    // same previous level and update current level
    int prevLevel = currentLevel;
    currentLevel = level;

    // jump over procedure declarations
    int jmpIdx = codeIdx;
    emit(7, 0, 0);

    constDeclaration();
    int numVars = varDeclaration();

    // process all procedures
    procedureDeclaration(level);

    code[jmpIdx].m = codeIdx * 3;

    // allocate space for variables
    emit(6, 0, numVars + 3);

    statement();

    markSymbols(level);

    // restore level
    currentLevel = prevLevel;
}

// main program checks for errors runs parser and ends execution
void program(void){
    // Check for scanning errors 
    for (int i = 0; i < tokenCount; i++){
        if (tokens[i].type == skipsym){
            reportError("Error: Scanning error detected by lexer (skipsym present)");
        }
    }
    block(0);
    if (currentToken().type != periodsym){
        reportError("Error: program must end with period");
    }
    nextToken();
    emit(9, 0, 3); // halt program
}

// Returns string name of instruction opcode
const char *opMnemonic(int op){
    if (op == 1){
        return "LIT";
    }
    if (op == 2){
        return "OPR";
    }
    if (op == 3){
        return "LOD";
    }
    if (op == 4){
        return "STO";
    }
    if (op == 5){
        return "CAL";
    }
    if (op == 6){
        return "INC";
    }
    if (op == 7){
        return "JMP";
    }
    if (op == 8){
        return "JPC";
    }
    if (op == 9){
        return "SYS";
    }
    return "???";
}

// marks all symbols at this lebel unavaliable
void markSymbols(int level){
    for (int i = symCount - 1; i >= 0; i--){
        if (symbol_table[i].level == level && symbol_table[i].mark == 0){
            symbol_table[i].mark = 1;
        }
    }
}

// main function handles input runs scanner and parser and prints output
int main(int argc, char *argv[]){

    // If statement for if the argument count is wrong
    if (argc != 2){
        printf("Usage: ./parsercodegen <input_file>\n");
        return 1;
    }

    // Opens the input file
    FILE *fp = fopen(argv[1], "r");

    // Stops the program if the file isnt opened properly
    if (fp == NULL){
        printf("Error: Cannot open file\n");
        return 1;
    }

    // Opens elf.txt for writing
    elfFile = fopen("elf.txt", "w");
    if (elfFile == NULL){
        printf("Error: Cannot create elf.txt\n");
        fclose(fp);
        return 1;
    }
    scan(fp); // Phase 1: tokenize
    fclose(fp);
    program(); // Phase 2: parse and generate code

    // Prints assembly code table
    printf("Assembly Code:\n");
    printf("+------+-----+---+-----+\n");
    printf("| Line | OP  | L | M   |\n");
    printf("+------+-----+---+-----+\n");
    for (int i = 0; i < codeIdx; i++){
        printf("| %-4d | %-3s | %d | %-3d |\n", i, opMnemonic(code[i].op), code[i].l, code[i].m);
    }
    printf("+------+-----+---+-----+\n\n");

    // Prints symbol table
    printf("Symbol Table:\n");
    printf("+------+-------------+-------+-------+---------+------+\n");
    printf("| Kind | Name        | Value | Level | Address | Mark |\n");
    printf("+------+-------------+-------+-------+---------+------+\n");
    for (int i = 0; i < symCount; i++){
        printf("| %-4d | %-11s | %-5d | %-5d | %-7d | %-4d |\n", symbol_table[i].kind, symbol_table[i].name, symbol_table[i].val, symbol_table[i].level, symbol_table[i].addr, symbol_table[i].mark);
    }
    printf("+------+-------------+-------+-------+---------+------+\n");
    
    // Write numeric instructions to elf.txt
    for (int i = 0; i < codeIdx; i++){
        fprintf(elfFile, "%d %d %d\n", code[i].op, code[i].l, code[i].m);
    }
    fclose(elfFile);
    return 0;
}
