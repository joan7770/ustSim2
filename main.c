//  main.c
//  Joan Marin-Romero
//	Nancy Yang
//
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000


int field0(int instruction){
	return( (instruction>>19) & 0x7);
}

int field1(int instruction){
	return( (instruction>>16) & 0x7);
}

int field2(int instruction){
	return(instruction & 0xFFFF);
}

int opcode(int instruction){
	return(instruction>>22);
}

int signExtend(int num){
	// convert a 16-bit number into a 32-bit integer
	if (num & (1<<15) ) {
		num -= (1<<16);
	}
	return num;
}

typedef struct IFIDstruct{
	int instr;
	int pcPlus1;
} IFIDType;

typedef struct IDEXstruct{
	int instr;
	int pcPlus1;
	int readRegA;
	int readRegB;
	int offset;
} IDEXType;

typedef struct EXMEMstruct{
	int instr;
	int branchTarget;
	int aluResult;
	int readReg;
} EXMEMType;

typedef struct MEMWBstruct{
	int instr;
	int writeData;
} MEMWBType;

typedef struct WBENDstruct{
	int instr;
	int writeData;
} WBENDType;

typedef struct statestruct{
	int pc;
	int instrMem[NUMMEMORY];
	int dataMem[NUMMEMORY];
	int reg[NUMREGS];
	int numMemory;
	int cycles;
	int fetched;
	int retired;
	int branches; /* Total number of branches executed */
	int mispreds; /* Number of branch mispredictions*/
	IFIDType IFID;
	IDEXType IDEX;
	EXMEMType EXMEM;
	MEMWBType MEMWB;
	WBENDType WBEND;
} stateType;


void printInstruction(int instr) {
	char opcodeString[10];
	if (opcode(instr) == ADD) {
		strcpy(opcodeString, "add");
	}
	else if (opcode(instr) == NAND) {
		strcpy(opcodeString, "nand");
		
	}
	else if (opcode(instr) == LW) {
		strcpy(opcodeString, "lw");
	}
	else if (opcode(instr) == SW) { strcpy(opcodeString, "sw");}
	else if (opcode(instr) == BEQ) { strcpy(opcodeString, "beq");}
	else if (opcode(instr) == JALR) { strcpy(opcodeString, "jalr");}
	else if (opcode(instr) == HALT) { strcpy(opcodeString, "halt");}
	else if (opcode(instr) == NOOP) { strcpy(opcodeString, "noop");}
	else {
		strcpy(opcodeString, "data");
	}
	if(opcode(instr) == ADD || opcode(instr) == NAND){
		printf("%s %d %d %d\n", opcodeString, field2(instr), field0(instr), field1(instr));
	}
	else if(0 == strcmp(opcodeString, "data")){
		printf("%s %d\n", opcodeString, signExtend(field2(instr)));
	}
	else{
		printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
			   signExtend(field2(instr)));
	}
}

void printState(stateType *statePtr){ int i;
	printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
	printf("\tpc %d\n", statePtr->pc);
	printf("\tdata memory:\n");
	
	for (i=0; i<statePtr->numMemory; i++) {
		printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
	}
	
	printf("\tregisters:\n");
	
	for (i=0; i<NUMREGS; i++) {
		printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
		
	}
	
	printf("\tIFID:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IFID.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
	printf("\tIDEX:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IDEX.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
	printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
	printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
	printf("\t\toffset %d\n", statePtr->IDEX.offset);
	printf("\tEXMEM:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->EXMEM.instr);
	printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
	printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
	printf("\t\treadRegB %d\n", statePtr->EXMEM.readReg);
	printf("\tMEMWB:\n");
	printf("\t\tinstruction "); printInstruction(statePtr->MEMWB.instr);
	printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
	printf("\tWBEND:\n");
	printf("\t\tinstruction "); printInstruction(statePtr->WBEND.instr);
	printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
}

/*------------------ IF stage ----------------- */
void ifStage(stateType* state){
	state->IFID = (IFIDType){.instr = state->instrMem[state->pc], .pcPlus1 = state->pc + 1};
}

/*------------------ ID stage ----------------- */
void idStage(stateType* state){
	int instr = state->IFID.instr;
	int regA = field0(state->IFID.instr);
	int regB = field1(state->IFID.instr);
	int offset = 0;
	
	//ADD or NAND
	if(opcode(instr) == ADD || opcode(instr) == NAND){
		offset = field2(state->IFID.instr);
	}
	// LW or SW or BEQ
	else if(opcode(instr) == LW || opcode(instr) == SW || opcode(instr) == BEQ){
		offset = signExtend(field2(state->IFID.instr));
	}
	else if (opcode(instr) == JALR){
		
	}
	else if (opcode(instr) == HALT){
		
	}
	else if (opcode(instr) == NOOP){
		instr = NOOPINSTRUCTION;
	}
	state->IDEX.instr = instr;
	state->IDEX.offset = offset;
	state->IDEX.readRegA = regA;
	state->IDEX.readRegB = regB;
}

/*------------------ EX stage ----------------- */
void exStage(stateType* state){
	
}

/*------------------ MEM stage ----------------- */
void memStage(stateType* state){
	
}

/*------------------ WB stage ----------------- */
void wbStage(stateType* state){
	
}

void run(stateType* state){
	
	stateType* newState = (stateType*)malloc(sizeof(stateType));
	
	// Reused variables;
	int branchTarget = 0;
	int aluResult = 0;
	int total_instrs = 0;
	
	// Primary loop
	while(1){
		printState(state);
		/* check for halt */
		if(HALT == opcode(state->MEMWB.instr)) {
			printf("machine halted\n");
			printf("total of %d cycles executed\n", state->cycles);
			printf("total of %d instructions fetched\n", state->fetched);
			printf("total of %d instructions retured\n", state->retired);
			printf("total of %d branches executed\n", state->branches);
			printf("total of %d branch mispredictions\n", state->mispreds);
			exit(0);
		}
		newState = state;
		newState->cycles++;
		
		/*------------------ IF stage ----------------- */
		ifStage(newState);
		/*------------------ ID stage ----------------- */
		idStage(newState);
		/*------------------ EX stage ----------------- */
		exStage(newState);
		/*------------------ MEM stage ----------------- */
		memStage(newState);
		/*------------------ WB stage ----------------- */
		wbStage(newState);
		
		state = newState; /* this is the last statement before the end of the loop. It marks the end of the cycle and updates the current state with the values calculated in this cycle – AKA “Clock Tick”. */
	}
}

int main(int argc, char** argv){
	/** Get command line arguments **/
	char* fname;

	if(argc == 1){
		fname = (char*)malloc(sizeof(char)*100);
		printf("Enter the name of the machine code file to simulate: ");
		fgets(fname, 100, stdin);
		fname[strlen(fname)-1] = '\0'; // gobble up the \n with a \0
	}
	else if (argc == 2){

		int strsize = (int)strlen(argv[1]);

		fname = (char*)malloc(strsize);
		fname[0] = '\0';

		strcat(fname, argv[1]);
	}else{
		printf("Please run this program correctly\n");
		exit(-1);
	}

	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		printf("Cannot open file '%s' : %s\n", fname, strerror(errno));
		return -1;
	}

	/* count the number of lines by counting newline characters */
	int line_count = 0;
	int c;
	while (EOF != (c=getc(fp))) {
		if ( c == '\n' ){
			line_count++;
		}
	}
	// reset fp to the beginning of the file
	rewind(fp);

	stateType* state = (stateType*)malloc(sizeof(stateType));

	state->pc = 0;
	memset(state->instrMem, 0, NUMMEMORY*sizeof(int));
	memset(state->reg, 0, NUMREGS*sizeof(int));

	state->numMemory = line_count;
	state->cycles = 0;
	state->fetched = 0;
	state->retired = 0;
	state->branches = 0;
	state->mispreds = 0;
	state->IFID = (IFIDType){.instr = NOOPINSTRUCTION};
	state->IDEX = (IDEXType){.instr = NOOPINSTRUCTION};
	state->EXMEM = (EXMEMType){.instr = NOOPINSTRUCTION};
	state->MEMWB = (MEMWBType){.instr = NOOPINSTRUCTION};
	state->WBEND = (WBENDType){.instr = NOOPINSTRUCTION};
	
	char line[256];

	int i = 0;
	while (fgets(line, sizeof(line), fp)) {
		/* note that fgets doesn't strip the terminating \n, checking its
		   presence would allow to handle lines longer that sizeof(line) */
		state->instrMem[i] = atoi(line);
		i++;
	}
	fclose(fp);

	/** Run the simulation **/
	run(state);

	free(state);
	free(fname);
}
