/*
 * EECS 370, University of Michigan, Fall 2023
 * Project 3: LC-2K Pipeline Simulator
 * Instructions are found in the project spec: https://eecs370.github.io/project_3_spec/
 * Make sure NOT to modify printState or any of the associated functions
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Machine Definitions
#define NUMMEMORY 65536 // maximum number of data words in memory
#define NUMREGS 8 // number of machine registers

#define ADD 0
#define NOR 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 // will not implemented for Project 3
#define HALT 6
#define NOOP 7

const char* opcode_to_str_map[] = {
    "add",
    "nor",
    "lw",
    "sw",
    "beq",
    "jalr",
    "halt",
    "noop"
};

#define NOOPINSTR (NOOP << 22)

typedef struct IFIDStruct {
	int pcPlus1;
	int instr;
} IFIDType;

typedef struct IDEXStruct {
	int pcPlus1;
	int valA;
	int valB;
	int offset;
	int instr;
} IDEXType;

typedef struct EXMEMStruct {
	int branchTarget;
    int eq;
	int aluResult;
	int valB;
	int instr;
} EXMEMType;

typedef struct MEMWBStruct {
	int writeData;
    int instr;
} MEMWBType;

typedef struct WBENDStruct {
	int writeData;
	int instr;
} WBENDType;

typedef struct stateStruct {
	int pc;
	int instrMem[NUMMEMORY];
	int dataMem[NUMMEMORY];
	int reg[NUMREGS];
	unsigned int numMemory;
	IFIDType IFID;
	IDEXType IDEX;
	EXMEMType EXMEM;
	MEMWBType MEMWB;
	WBENDType WBEND;
	unsigned int cycles; // number of cycles run so far
} stateType;

static inline int opcode(int instruction) {
    return instruction>>22;
}

static inline int field0(int instruction) {
    return (instruction>>19) & 0x7;
}

static inline int field1(int instruction) {
    return (instruction>>16) & 0x7;
}

static inline int field2(int instruction) {
    return instruction & 0xFFFF;
}

// convert a 16-bit number into a 32-bit Linux integer
static inline int convertNum(int num) {
    return num - ( (num & (1<<15)) ? 1<<16 : 0 );
}

void printState(stateType*);
void printInstruction(int);
void readMachineCode(stateType*, char*);


int main(int argc, char *argv[]) {

    /* Declare state and newState.
       Note these have static lifetime so that instrMem and
       dataMem are not allocated on the stack. */

    stateType state, newState;

    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    // Initialize state here
    state.pc = 0;
    state.cycles = 0;
    for (int i = 0; i < NUMREGS; ++i){
        state.reg[i] = 0;
    }
    state.IFID.instr = NOOPINSTR;
    state.IDEX.instr = NOOPINSTR;
    state.EXMEM.instr = NOOPINSTR;
    state.MEMWB.instr = NOOPINSTR;
    state.WBEND.instr = NOOPINSTR;
    state.IFID.pcPlus1 = 0;
    state.WBEND.writeData = 0;

    //Destinations for detect and forward
    int EXMEM_dest = 0;
    int MEMWB_dest = 0;
    int WBEND_dest = 0;

    newState = state;

    while (opcode(state.MEMWB.instr) != HALT) {
        printState(&state);

        newState.cycles += 1;

        /* ---------------------- IF stage --------------------- */
        //Fetch instruction, increment PC, and store info into pipeline register
        newState.IFID.instr = state.instrMem[state.pc];
        newState.IFID.pcPlus1 = state.pc + 1;
        newState.pc++;

        /* ---------------------- ID stage --------------------- */
        //Store instruction bits and pcPlus1
        newState.IDEX.instr = state.IFID.instr;
        newState.IDEX.pcPlus1 = state.IFID.pcPlus1;

        //Check for lw followed by dependent instr
        if (opcode(state.IDEX.instr) == LW && (field0(newState.IDEX.instr) == field1(state.IDEX.instr) || field1(newState.IDEX.instr) == field1(state.IDEX.instr))) {
            newState.IDEX.instr = NOOPINSTR;
            newState.IFID = state.IFID;
            newState.pc = state.pc;
        }
        //There isn't a data hazard
        else {
            // Store regA and regB data into the pipeline register, also offset
            newState.IDEX.valA = state.reg[field0(state.IFID.instr)];
            newState.IDEX.valB = state.reg[field1(state.IFID.instr)];
            newState.IDEX.offset = convertNum(field2(state.IFID.instr));
        }

        /* ---------------------- EX stage --------------------- */
        //Get instruction
        newState.EXMEM.instr = state.IDEX.instr;

        //Get WBEND_dest
        //If instruction is an lw then WBEND_dest is field1
        if (opcode(state.WBEND.instr) == LW) {
            WBEND_dest = field1(state.WBEND.instr);
        }
        //Otherwise its field2
        else {
            WBEND_dest = field2(state.WBEND.instr);
        }

        //Check for data hazard and forward regAValue and/or regBValue if there is one
        if (opcode(state.WBEND.instr) == ADD || opcode(state.WBEND.instr) == NOR || opcode(state.WBEND.instr) == LW) {
            if (field0(newState.EXMEM.instr) == WBEND_dest) {
                state.IDEX.valA = state.WBEND.writeData;
            }
            if (field1(newState.EXMEM.instr) == WBEND_dest) {
                state.IDEX.valB = state.WBEND.writeData;
            }
        }

        //If instruction is an lw then set MEMWB_dest to regB
        if (opcode(state.MEMWB.instr) == LW) {
            MEMWB_dest = field1(state.MEMWB.instr);
        }
        //Otherwise set MEMWB_dest to other value
        else {
            MEMWB_dest = field2(state.MEMWB.instr);
        }

        //Check for data hazard and forward regAValue and/or regBvalue if there is one
        if (opcode(state.MEMWB.instr) == ADD || opcode(state.MEMWB.instr) == NOR || opcode(state.MEMWB.instr) == LW) {
            if (field0(newState.EXMEM.instr) == MEMWB_dest) {
                state.IDEX.valA = state.MEMWB.writeData;
            }
            if (field1(newState.EXMEM.instr) == MEMWB_dest) {
                state.IDEX.valB = state.MEMWB.writeData;
            }
        }

        //Same thought process as previous operations
        if (opcode(state.EXMEM.instr) == LW) {
            EXMEM_dest = field1(state.EXMEM.instr);
        }
        else {
            EXMEM_dest = field2(state.EXMEM.instr);
        }
        if (opcode(state.EXMEM.instr) == ADD || opcode(state.EXMEM.instr) == NOR || opcode(state.EXMEM.instr) == LW) {
            if (field0(newState.EXMEM.instr) == EXMEM_dest) {
                state.IDEX.valA = state.EXMEM.aluResult;
            }
            else if (field1(newState.EXMEM.instr) == EXMEM_dest) {
                state.IDEX.valB = state.EXMEM.aluResult;
            }
        }
        
        //Figure out what the instruciton is and store the ALU result
        if (opcode(newState.EXMEM.instr) == ADD) {
            newState.EXMEM.aluResult = state.IDEX.valA + state.IDEX.valB;
        }
        else if (opcode(newState.EXMEM.instr) == NOR) {
            newState.EXMEM.aluResult = ~(state.IDEX.valA | state.IDEX.valB);
        }
        else if (opcode(newState.EXMEM.instr) == LW) {
            newState.EXMEM.aluResult = state.IDEX.valA + state.IDEX.offset;
        }
        else if (opcode(newState.EXMEM.instr) == SW) {
            newState.EXMEM.aluResult = state.IDEX.valA + state.IDEX.offset;
        }
        else if (opcode(newState.EXMEM.instr) == BEQ) {
            newState.EXMEM.aluResult = state.IDEX.valA - state.IDEX.valB;
        }
        
        // If an instruction is actually being performed, pass on the contents of regB
        if (opcode(newState.EXMEM.instr) != NOOP){
            newState.EXMEM.valB = state.IDEX.valB;
        }

        // Get PC + 1 + offset and pass it on along with the instruction
        newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + state.IDEX.offset;

        // Set 'eq'
        if (state.IDEX.valA == state.IDEX.valB) {
            newState.EXMEM.eq = 1;
        }
        else {
            newState.EXMEM.eq = 0;
        }

        /* --------------------- MEM stage --------------------- */
        // Pass on instuction
        newState.MEMWB.instr = state.EXMEM.instr;
        
        // Pass on the stuff that deals with data memory
        if (opcode(newState.MEMWB.instr) == LW) {
            newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
        }
        else if (opcode(newState.MEMWB.instr) == SW) {
            newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.valB;
        }
        else if (opcode(newState.MEMWB.instr) == BEQ) {
            //If the branch was taken then reset pc and squash
            if (state.EXMEM.eq == 1) {
                newState.pc = state.EXMEM.branchTarget;
                newState.IFID.instr = NOOPINSTR;
                newState.IDEX.instr = NOOPINSTR;
                newState.EXMEM.instr = NOOPINSTR;
            }
        }
        else if (opcode(newState.MEMWB.instr) != NOOP && opcode(newState.MEMWB.instr) != HALT) {
            newState.MEMWB.writeData = state.EXMEM.aluResult;
        }

        /* ---------------------- WB stage --------------------- */
        // Pass things on to the final pipeline register
        newState.WBEND.instr = state.MEMWB.instr;
        newState.WBEND.writeData = state.MEMWB.writeData;

        // Write the data into the register file
        if (opcode(state.MEMWB.instr) == ADD ||  opcode(state.MEMWB.instr) == NOR) {
            newState.reg[field2(newState.WBEND.instr)] = state.MEMWB.writeData;
        }
        else if (opcode(state.MEMWB.instr) == LW) {
            newState.reg[field1(newState.WBEND.instr)] = state.MEMWB.writeData;
        }

        /* ------------------------ END ------------------------ */
        state = newState; /* this is the last statement before end of the loop. It marks the end
        of the cycle and updates the current state with the values calculated in this cycle */
    }
    printf("Machine halted\n");
    printf("Total of %d cycles executed\n", state.cycles);
    printf("Final state of machine:\n");
    printState(&state);
}

/*
* DO NOT MODIFY ANY OF THE CODE BELOW.
*/

void printInstruction(int instr) {
    const char* instr_opcode_str;
    int instr_opcode = opcode(instr);
    if(ADD <= instr_opcode && instr_opcode <= NOOP) {
        instr_opcode_str = opcode_to_str_map[instr_opcode];
    }

    switch (instr_opcode) {
        case ADD:
        case NOR:
        case LW:
        case SW:
        case BEQ:
            printf("%s %d %d %d", instr_opcode_str, field0(instr), field1(instr), convertNum(field2(instr)));
            break;
        case JALR:
            printf("%s %d %d", instr_opcode_str, field0(instr), field1(instr));
            break;
        case HALT:
        case NOOP:
            printf("%s", instr_opcode_str);
            break;
        default:
            printf(".fill %d", instr);
            return;
    }
}

void printState(stateType *statePtr) {
    printf("\n@@@\n");
    printf("state before cycle %d starts:\n", statePtr->cycles);
    printf("\tpc = %d\n", statePtr->pc);

    printf("\tdata memory:\n");
    for (int i=0; i<statePtr->numMemory; ++i) {
        printf("\t\tdataMem[ %d ] = %d\n", i, statePtr->dataMem[i]);
    }
    printf("\tregisters:\n");
    for (int i=0; i<NUMREGS; ++i) {
        printf("\t\treg[ %d ] = %d\n", i, statePtr->reg[i]);
    }

    // IF/ID
    printf("\tIF/ID pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IFID.instr);
    printInstruction(statePtr->IFID.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IFID.pcPlus1);
    if(opcode(statePtr->IFID.instr) == NOOP){
        printf(" (Don't Care)");
    }
    printf("\n");

    // ID/EX
    int idexOp = opcode(statePtr->IDEX.instr);
    printf("\tID/EX pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IDEX.instr);
    printInstruction(statePtr->IDEX.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IDEX.pcPlus1);
    if(idexOp == NOOP){
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegA = %d", statePtr->IDEX.valA);
    if (idexOp >= HALT || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->IDEX.valB);
    if(idexOp == LW || idexOp > BEQ || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\toffset = %d", statePtr->IDEX.offset);
    if (idexOp != LW && idexOp != SW && idexOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // EX/MEM
    int exmemOp = opcode(statePtr->EXMEM.instr);
    printf("\tEX/MEM pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->EXMEM.instr);
    printInstruction(statePtr->EXMEM.instr);
    printf(" )\n");
    printf("\t\tbranchTarget %d", statePtr->EXMEM.branchTarget);
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\teq ? %s", (statePtr->EXMEM.eq ? "True" : "False"));
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\taluResult = %d", statePtr->EXMEM.aluResult);
    if (exmemOp > SW || exmemOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->EXMEM.valB);
    if (exmemOp != SW) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // MEM/WB
	int memwbOp = opcode(statePtr->MEMWB.instr);
    printf("\tMEM/WB pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->MEMWB.instr);
    printInstruction(statePtr->MEMWB.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->MEMWB.writeData);
    if (memwbOp >= SW || memwbOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // WB/END
	int wbendOp = opcode(statePtr->WBEND.instr);
    printf("\tWB/END pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->WBEND.instr);
    printInstruction(statePtr->WBEND.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->WBEND.writeData);
    if (wbendOp >= SW || wbendOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    printf("end state\n");
    fflush(stdout);
}

// File
#define MAXLINELENGTH 1000 // MAXLINELENGTH is the max number of characters we read

void readMachineCode(stateType *state, char* filename) {
    char line[MAXLINELENGTH];
    FILE *filePtr = fopen(filename, "r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", filename);
        exit(1);
    }

    printf("instruction memory:\n");
    for (state->numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; ++state->numMemory) {
        if (sscanf(line, "%d", state->instrMem+state->numMemory) != 1) {
            printf("error in reading address %d\n", state->numMemory);
            exit(1);
        }
        printf("\tinstrMem[ %d ]\t= 0x%08x\t= %d\t= ", state->numMemory, 
            state->instrMem[state->numMemory], state->instrMem[state->numMemory]);
        printInstruction(state->dataMem[state->numMemory] = state->instrMem[state->numMemory]);
        printf("\n");
    }
}
