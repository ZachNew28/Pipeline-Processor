#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

//Every LC2K file will contain less than 1000 lines of assembly.
#define MAXLINELENGTH 1000
#define MAXSYMBOLS 1000
#define MAXRELOCATIONS 1000
#define MAXLABELS 1000

/**
 * Requires: readAndParse is non-static and unmodified from project 1a. 
 *   inFilePtr and outFilePtr must be opened. 
 *   inFilePtr must be rewound before calling this function.
 * Modifies: outFilePtr
 * Effects: Prints the correct machine code for the input file. After 
 *   reading and parsing through inFilePtr, the pointer is rewound.
 *   Most project 1a error checks are done. No undefined labels of any
 *   type are checked, and these are instead resolved to 0.
*/
extern void print_inst_machine_code(FILE *inFilePtr, FILE *outFilePtr);

int readAndParse(FILE *, char *, char *, char *, char *, char *);
void checkForBlankLinesInCode(FILE *inFilePtr);
int isNumber(char *);
int encodeInstructionWithLabelResolution(char* opcode, char* arg0, char* arg1, char* arg2, int currentAddress);
int findLabelAddressSymbols(char *label);
int findLabelAddressRelocate(char* label);
bool isGlobalLabel(char* label);
int addLabelToSymbolTable(char* label, int offset, char type, bool isGlobal);
int isValidRegister(char* reg);
void addEntryToRelocationTable(const char* label, int currentAddress, const char* opcode);

typedef struct {
    char label[MAXLINELENGTH]; // Label name
    char type; // 'T' for text, 'D' for data, 'U' for undefined
    int offset; // Offset from the start of the text or data section
    bool isGlobal; // True if global, false if local
    bool isDefined;
} SymbolTableEntry;

typedef struct {
    int offset; // Offset from the start of the text or data section
    char opcode[MAXLINELENGTH]; // Opcode that uses the symbol
    char label[MAXLINELENGTH]; // Label name
} RelocationTableEntry;

typedef struct {
    char label[MAXLINELENGTH];
} LabelTableEntry;

SymbolTableEntry symbolTable[MAXSYMBOLS];
int symbolTableSize = 0;
int symbolTablePrintSize = 0;
RelocationTableEntry relocationTable[MAXRELOCATIONS];
int relocationTableSize = 0;

LabelTableEntry labelTable[MAXLABELS];
int labelTableSize = 0;

int textSection[MAXLINELENGTH];
int textSectionSize = 0;
int dataSection[MAXLINELENGTH];
int dataSectionSize = 0;

int
main(int argc, char **argv)
{
    char *inFileString, *outFileString;
    FILE *inFilePtr, *outFilePtr;
    char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH],
            arg1[MAXLINELENGTH], arg2[MAXLINELENGTH];

    if (argc != 3) {
        printf("error: usage: %s <assembly-code-file> <machine-code-file>\n",
            argv[0]);
        exit(1);
    }

    inFileString = argv[1];
    outFileString = argv[2];

    inFilePtr = fopen(inFileString, "r");
    if (inFilePtr == NULL) {
        printf("error in opening %s\n", inFileString);
        exit(1);
    }
    // Check for blank lines in the middle of the code.
    checkForBlankLinesInCode(inFilePtr);
    
    outFilePtr = fopen(outFileString, "w");
    if (outFilePtr == NULL) {
        printf("error in opening %s\n", outFileString);
        exit(1);
    }

    /* here is an example for how to use readAndParse to read a line from
        inFilePtr */
    if (! readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2) ) {
        /* reached end of file */
    }

    /* after doing a readAndParse, you may want to do the following to test the
        opcode */
    if (!strcmp(opcode, "add")) {
        /* do whatever you need to do for opcode "add" */
    }

    /* this is how to rewind the file ptr so that you start reading from the
        beginning of the file */
    rewind(inFilePtr);

    // First Pass: Collect labels and their addresses
    int dataAddressNum = -1;
    int textAddressNum = 0;
    while (readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)) {
        if (strcmp(opcode, ".fill") == 0) {
            dataAddressNum++;
        }
        //Will only enter this condition if the line has a label in it at the start or a label at the end
        if (strlen(label) > 0 || strlen(arg2) > 0) {
            int result = 10;
            int undefinedResult = 10;
            bool foundLabel = false;
            bool addReloc = false;

            //Checks for labels at the end of the line (arg2)
            if (strlen(arg2) > 0 && !isNumber(arg2)) {
                for (int i = 0; i < symbolTableSize; ++i) {
                    if (strcmp(symbolTable[i].label, arg2) == 0) {
                        foundLabel = true;
                        //If the symbol is undefined and still doing things then keep adding those lines to relocation table
                        if (symbolTable[i].type == 'U') {
                            addReloc = true;
                        }
                    }
                }
                //Add to symbol table as undefined and to relocation table if the label wasn't found
                if (!foundLabel) {
                    undefinedResult = addLabelToSymbolTable(arg2, 0, 'U', isGlobalLabel(arg2));
                    if (!isNumber(arg2)) {
                        addEntryToRelocationTable(arg2, textAddressNum, opcode);
                    }
                }
                else if(foundLabel && addReloc) {
                    addEntryToRelocationTable(arg2, textAddressNum, opcode);
                }
                else {
                    foundLabel = false;
                }
            }

            //Checks for labels in arg0
            if (strlen(arg0) > 0 && !isNumber(arg0)) {
                for (int i = 0; i < symbolTableSize; ++i) {
                    if (strcmp(symbolTable[i].label, arg0) == 0) {
                        foundLabel = true;
                        //If the symbol is undefined and still doing things then keep adding those lines to relocation table
                        if (symbolTable[i].type == 'U') {
                            addReloc = true;
                        }
                    }
                }
                //Add to symbol table as undefined and to relocation table if the label wasn't found
                if (!foundLabel) {
                    undefinedResult = addLabelToSymbolTable(arg0, 0, 'U', isGlobalLabel(arg0));
                    if (!isNumber(arg0)) {
                        addEntryToRelocationTable(arg0, textAddressNum, opcode);
                    }
                }
                else if(foundLabel && addReloc) {
                    addEntryToRelocationTable(arg0, textAddressNum, opcode);
                }
                else {
                    foundLabel = false;
                }
            }

            //Checks for the labels in arg1 slot
            if (strlen(arg1) > 0 && !isNumber(arg1)) {
                for (int i = 0; i < symbolTableSize; ++i) {
                    if (strcmp(symbolTable[i].label, arg1) == 0) {
                        foundLabel = true;
                        //If the symbol is undefined and still doing things then keep adding those lines to relocation table
                        if (symbolTable[i].type == 'U') {
                            addReloc = true;
                        }
                    }
                }
                //Add to symbol table as undefined and to relocation table if the label wasn't found
                if (!foundLabel) {
                    undefinedResult = addLabelToSymbolTable(arg1, 0, 'U', isGlobalLabel(arg1));
                    if (!isNumber(arg1)) {
                        addEntryToRelocationTable(arg1, textAddressNum, opcode);
                    }
                }
                else if(foundLabel && addReloc) {
                    addEntryToRelocationTable(arg1, textAddressNum, opcode);
                }
                else {
                    foundLabel = false;
                }
            }
            
            //Checks the labels at the start of a line
            if (strlen(label) > 0) {
                for (int i = 0; i < symbolTableSize; ++i) {
                    if (strcmp(symbolTable[i].label, label) == 0) {
                        foundLabel = true;
                        //If its got .fill then its a directive
                        if (strcmp(opcode, ".fill") == 0) {
                            symbolTable[i].type = 'D';
                            symbolTable[i].offset = dataSectionSize;
                            dataSectionSize++;
                        }
                        else {
                            symbolTable[i].type = 'T';
                            symbolTable[i].offset = textSectionSize;
                            textSectionSize++;
                        }
                    }
                }
                //Label is not already in the symbol table
                if (!foundLabel) {
                    if (strcmp(opcode, ".fill") == 0) {
                        result = addLabelToSymbolTable(label, dataAddressNum, 'D', isGlobalLabel(label));
                        dataSectionSize++;
                    }
                    else {
                        result = addLabelToSymbolTable(label, textAddressNum, 'T', isGlobalLabel(label));
                        textSectionSize++;
                    }
                }
                else {
                    foundLabel = false;
                }
            }

            if (undefinedResult < 0) {
                // Error handling based on the returned error code
                if (undefinedResult == -1) {
                    fprintf(stderr, "Assembly halted: Symbol table capacity exceeded.\n");
                } else if (undefinedResult == -2) {
                    fprintf(stderr, "Assembly halted: Duplicate label '%s'.\n", label);
                }
                // Close files and clean up resources before exiting
                fclose(inFilePtr);
                fclose(outFilePtr);
                exit(1);
            }
            if (result < 0) {
                // Error handling based on the returned error code
                if (result == -1) {
                    fprintf(stderr, "Assembly halted: Symbol table capacity exceeded.\n");
                } else if (result == -2) {
                    fprintf(stderr, "Assembly halted: Duplicate label '%s'.\n", label);
                }
                // Close files and clean up resources before exiting
                fclose(inFilePtr);
                fclose(outFilePtr);
                exit(1);
            }
        }
        textAddressNum++;
    }

    // Prepare for second pass
    rewind(inFilePtr);
    dataSectionSize = 0;
    textSectionSize = 0;
    textAddressNum = 0;
    dataAddressNum = -1;

    // Second Pass: Encode instructions and resolve labels
    while (readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)) {
        if (strcmp(opcode, ".fill") == 0) {
            dataAddressNum++;
        }
        //Find the label in the table and see if it is undefined and local
        for (int i = 0; i < symbolTableSize; ++i) {
            if (strcmp(symbolTable[i].label, arg2) == 0 && symbolTable[i].type == 'U' && !symbolTable[i].isGlobal) {
                printf("Error: Undefined label %s\n", arg2);
                exit(1); // Exit if the label is undefined
            }
        }
        //Make sure beq doesn't use undefined labels
        if (strcmp(opcode, "beq") == 0) {
            for (int i = 0; i < symbolTableSize; ++i) {
                if ((strcmp(symbolTable[i].label, arg0) == 0 || strcmp(symbolTable[i].label, arg1) == 0 
                    || strcmp(symbolTable[i].label, arg2) == 0) && symbolTable[i].type == 'U') {
                    printf("Error: beq using undefined symbolic address \n");
                    exit(1); // Exit if beq is using a symbolic address
                }
            }
        }

        // Handle '.fill' directive specifically
        if (strcmp(opcode, ".fill") == 0) {
            int value;
            if (isNumber(arg0)) {
                value = atoi(arg0); // Directly use the numeric value if arg0 is a number
            } else {
                // If arg0 is not a number, assume it's a symbolic address and resolve it
                int labelAddress = findLabelAddressSymbols(arg0);
                if (labelAddress == -1) {
                    printf("Error: Undefined label %s\n", arg0);
                    exit(1); // Exit if the label is undefined
                }
                value = labelAddress; // Use the resolved label address
                if (!isNumber(arg0)) {
                    addEntryToRelocationTable(arg0, dataAddressNum, opcode);
                }
            }
            dataSection[dataSectionSize++] = value; // Store the resolved value in the data section
        } else {
            // For other instructions, encode them normally
            int encodedInstruction = encodeInstructionWithLabelResolution(opcode, arg0, arg1, arg2, textAddressNum);
            textSection[textSectionSize++] = encodedInstruction;
        }
        textAddressNum++;
    }

    rewind(inFilePtr);

    // Now output the object file with the proper format
    fprintf(outFilePtr, "%d %d %d %d\n", textSectionSize, dataSectionSize, symbolTablePrintSize, relocationTableSize); // Header
    /* this will print the correct machine code for the file */
    print_inst_machine_code(inFilePtr, outFilePtr);
    for (int i = 0; i < symbolTableSize; i++) {
        if (symbolTable[i].isGlobal) {
            fprintf(outFilePtr, "%s %c %d\n", symbolTable[i].label, symbolTable[i].type, symbolTable[i].offset); // Symbol Table
        }
    }
    for (int i = 0; i < relocationTableSize; i++) {
        fprintf(outFilePtr, "%d %s %s\n", relocationTable[i].offset, relocationTable[i].opcode, relocationTable[i].label); // Relocation Table
    }

    fclose(inFilePtr);
    fclose(outFilePtr);
    return 0;
}

/*
* NOTE: The code defined below is not to be modifed as it is implemented correctly.
*/

// Returns non-zero if the line contains only whitespace.
int lineIsBlank(char *line) {
    char whitespace[4] = {'\t', '\n', '\r', ' '};
    int nonempty_line = 0;
    for(int line_idx=0; line_idx < strlen(line); ++line_idx) {
        int line_char_is_whitespace = 0;
        for(int whitespace_idx = 0; whitespace_idx < 4; ++ whitespace_idx) {
            if(line[line_idx] == whitespace[whitespace_idx]) {
                line_char_is_whitespace = 1;
                break;
            }
        }
        if(!line_char_is_whitespace) {
            nonempty_line = 1;
            break;
        }
    }
    return !nonempty_line;
}

// Exits 2 if file contains an empty line anywhere other than at the end of the file.
// Note calling this function rewinds inFilePtr.
void checkForBlankLinesInCode(FILE *inFilePtr) {
    char line[MAXLINELENGTH];
    int blank_line_encountered = 0;
    int address_of_blank_line = 0;
    rewind(inFilePtr);

    for(int address = 0; fgets(line, MAXLINELENGTH, inFilePtr) != NULL; ++address) {
        // Check for line too long
        if (strlen(line) >= MAXLINELENGTH-1) {
            printf("error: line too long\n");
            exit(1);
        }

        // Check for blank line.
        if(lineIsBlank(line)) {
            if(!blank_line_encountered) {
                blank_line_encountered = 1;
                address_of_blank_line = address;
            }
        } else {
            if(blank_line_encountered) {
                printf("Invalid Assembly: Empty line at address %d\n", address_of_blank_line);
                exit(2);
            }
        }
    }
    rewind(inFilePtr);
}

/*
 * Read and parse a line of the assembly-language file.  Fields are returned
 * in label, opcode, arg0, arg1, arg2 (these strings must have memory already
 * allocated to them).
 *
 * Return values:
 *     0 if reached end of file
 *     1 if all went well
 */
int readAndParse(FILE *inFilePtr, char *label,
	char *opcode, char *arg0, char *arg1, char *arg2) {

    char line[MAXLINELENGTH];
    char *ptr = line;

    // delete prior values
    label[0] = opcode[0] = arg0[0] = arg1[0] = arg2[0] = '\0';

    // read the line from the assembly-language file
    if (fgets(line, MAXLINELENGTH, inFilePtr) == NULL) {
		// reached end of file
        return(0);
    }

    // check for line too long
    if (strlen(line) >= MAXLINELENGTH-1) {
		printf("error: line too long\n");
		exit(1);
    }

    // Ignore blank lines at the end of the file.
    if(lineIsBlank(line)) {
        return 0;
    }

    // is there a label?
    ptr = line;
    if (sscanf(ptr, "%[^\t\n ]", label)) {
		// successfully read label; advance pointer over the label */
        ptr += strlen(label);
    }

    // Parse the rest of the line.  Would be nice to have real regular expressions, but scanf will suffice.
    sscanf(ptr, "%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]",
        opcode, arg0, arg1, arg2);
    return(1);
}

int
isNumber(char *string)
{
    int num;
    char c;
    return((sscanf(string, "%d%c",&num, &c)) == 1);
}

// Function to encode an instruction into machine code
int encodeInstructionWithLabelResolution(char* opcode, char* arg0, char* arg1, char* arg2, int currentAddress) {
    int machineCode = 0;
    int op, regA, regB, destReg, offset;
    
    // Opcode mapping
    if (strcmp(opcode, "add") == 0) op = 0;
    else if (strcmp(opcode, "nor") == 0) op = 1;
    else if (strcmp(opcode, "lw") == 0) op = 2;
    else if (strcmp(opcode, "sw") == 0) op = 3;
    else if (strcmp(opcode, "beq") == 0) op = 4;
    else if (strcmp(opcode, "jalr") == 0) op = 5;
    else if (strcmp(opcode, "halt") == 0) op = 6;
    else if (strcmp(opcode, "noop") == 0) op = 7;
    else return -1; // Invalid opcode

    if (!(strcmp(opcode, "halt") == 0 || strcmp(opcode, "noop") == 0)) {
        // Validate registers for instructions that require them
        if (!isValidRegister(arg0) || (op != 5 && !isValidRegister(arg1)) ||
            (op <= 1 && !isValidRegister(arg2))) { // For R-type instructions and others needing registers except jalr which uses only arg0 and arg1
            printf("Error: Invalid register number in instruction '%s %s %s %s'.\n", opcode, arg0, arg1, arg2);
            return -1; // Indicate an error in encoding due to invalid register number
        }
    }

    regA = atoi(arg0);
    regB = atoi(arg1);
    if (op <= 1) { // R-type: add, nor
        destReg = atoi(arg2);
        machineCode = (op << 22) | (regA << 19) | (regB << 16) | destReg;
    } else if (op >= 2 && op <= 4) { // I-type: lw, sw, beq
        if (isNumber(arg2)) {
            offset = atoi(arg2);
        } else {
            offset = findLabelAddressSymbols(arg2) - currentAddress;
        }
        machineCode = (op << 22) | (regA << 19) | (regB << 16) | (offset & 0xFFFF);
        if (offset < -32768 || offset > 32767) {
            printf("Error: Offset %d out of range for 16-bit signed integer.\n", offset);
            exit(1);
        }
    } else if (op == 5) { // J-type: jalr
        machineCode = (op << 22) | (regA << 19) | (regB << 16);
    } else { // O-type: halt, noop
        machineCode = (op << 22);
    }


    return machineCode;
}

int findLabelAddressSymbols(char *label) {
    for (int i = 0; i < symbolTableSize; i++) {
        if (strcmp(symbolTable[i].label, label) == 0) {
            return symbolTable[i].offset;
        }
    }
    return -1; // Indicate unresolved label
}
int findLabelAddressRelocate(char *label) {
    for (int i = 0; i < relocationTableSize; i++) {
        if (strcmp(relocationTable[i].label, label) == 0) {
            return relocationTable[i].offset;
        }
    }
    return -1; // Indicate unresolved label
}

// Function to determine if a label is global
bool isGlobalLabel(char* label) {
    return label[0] >= 'A' && label[0] <= 'Z';
}

// When adding a label to the symbol table:
int addLabelToSymbolTable(char* label, int offset, char type, bool isGlobal) {
    // Check for duplicate labels
    for (int i = 0; i < symbolTableSize; i++) {
        //!isNumber(label) is just making sure that it didn't fuck up somehow
        if (strcmp(symbolTable[i].label, label) == 0 && !isNumber(label)) {
            printf("Error: Duplicate label '%s' found.\n", label);
            return -2; // Indicate error due to duplicate label
        }
    }

    // Add label to symbol table
    strcpy(symbolTable[symbolTableSize].label, label);
    symbolTable[symbolTableSize].offset = offset;
    symbolTable[symbolTableSize].type = type;
    symbolTable[symbolTableSize].isGlobal = isGlobal;
    symbolTableSize++;
    if (isGlobal) {
        symbolTablePrintSize++;
    }

    return 0; // Indicate success
}

// Returns 1 (true) if the register number is valid, 0 (false) otherwise
int isValidRegister(char* reg) {
    if (!isNumber(reg)) return 0; // Ensure it's a number
    int regNum = atoi(reg);
    return regNum >= 0 && regNum <= 7;
}

// Function to add an entry to the relocation table
void addEntryToRelocationTable(const char* label, int currentAddress, const char* opcode) {
    if (strcmp(opcode, "beq") == 0) {
        return;
    }
    relocationTable[relocationTableSize].offset = currentAddress;
    strcpy(relocationTable[relocationTableSize].opcode, opcode);
    strcpy(relocationTable[relocationTableSize].label, label);
    relocationTableSize++;
}