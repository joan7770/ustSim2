//  main.c
//  Joan Marin-Romero
//	Nancy Yang

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

/*------------------ Given Code ------------------*/
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
/*------------------ end of given code ------------------ */

/*------------------ Function declarions ------------------*/
int signExtend(int num);
void run(stateType* state, stateType* newState);
void ifStage(stateType* state, stateType* newState);
void idStage(stateType* state, stateType* newState);
void exStage(stateType* state, stateType* newState);
void memStage(stateType* state, stateType* newState);
void wbStage(stateType* state, stateType* newState);
void dataForward(stateType* state);

/*------------------ Main and file handeling ------------------*/
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
		exit(EXIT_FAILURE);
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
	
	//state initialization
	stateType* state = (stateType*)malloc(sizeof(stateType));
	memset(state->instrMem, 0, NUMMEMORY*sizeof(int));
	memset(state->reg, 0, NUMREGS * sizeof(int));
	
	state->pc = 0;
	state->cycles = 0;
	state->fetched = 0;
	state->retired = 0;
	state->branches = 0;
	state->mispreds = 0;
	state->numMemory = line_count;
	state->IFID.instr = NOOPINSTRUCTION;
	state->IDEX.instr = NOOPINSTRUCTION;
	state->EXMEM.instr = NOOPINSTRUCTION;
	state->MEMWB.instr = NOOPINSTRUCTION;
	state->WBEND.instr = NOOPINSTRUCTION;
	
	//newState initialization
	stateType* newState = (stateType*)malloc(sizeof(stateType));
	memset(newState->instrMem, 0, NUMMEMORY * sizeof(int));
	memset(newState->reg, 0, NUMREGS * sizeof(int));
	
	newState->pc = 0;
	newState->cycles = 0;
	newState->fetched = 0;
	newState->retired = 0;
	newState->branches = 0;
	newState->mispreds = 0;
	newState->numMemory = line_count;
	newState->IFID.instr = NOOPINSTRUCTION;
	newState->IDEX.instr = NOOPINSTRUCTION;
	newState->EXMEM.instr = NOOPINSTRUCTION;
	newState->MEMWB.instr = NOOPINSTRUCTION;
	newState->WBEND.instr = NOOPINSTRUCTION;

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
	run(state, newState);

	free(state);
	free(newState);
	free(fname);
}

/*------------------ Simulator ------------------*/
void run(stateType* state, stateType* newState){
	
	// Primary loop
	while(true){
		printState(state);
		/* check for halt */
		if(HALT == opcode(state->MEMWB.instr)) {
			printf("machine halted\n");
			printf("total of %d cycles executed\n", state->cycles);
			printf("total of %d instructions fetched\n", state->fetched);
			printf("total of %d instructions retured\n", state->retired);
			printf("total of %d branches executed\n", state->branches);
			printf("total of %d branch mispredictions\n", state->mispreds);
			exit(EXIT_SUCCESS);
		}
		newState = state;
		newState->cycles++;
		
		/*------------------ IF stage ----------------- */
		ifStage(state, newState);
		/*------------------ ID stage ----------------- */
		idStage(state, newState);
		/*------------------ EX stage ----------------- */
		exStage(state, newState);
		/*------------------ MEM stage ----------------- */
		memStage(state, newState);
		/*------------------ WB stage ----------------- */
		wbStage(state, newState);
		
		state = newState; /* this is the last statement before the end of the loop. It marks the end of the cycle and updates the current state with the values calculated in this cycle – AKA “Clock Tick”. */
	}
}

/*------------------ IF stage ----------------- */
void ifStage(stateType* oldState, stateType* newState){
	int pc = oldState->pc;
	int instr = oldState->instrMem[pc];
	
	if(opcode(oldState->IFID.instr) == LW){
		if(opcode(instr) == ADD || opcode(instr) == NAND){
			if(field1(instr) == field0(oldState->IFID.instr)){
				instr = NOOPINSTRUCTION;
			}
			else if(field2(instr) == field0(oldState->IFID.instr)){
				instr = NOOPINSTRUCTION;
			}
			else{
				++pc;
			}
		}
		else if(opcode(instr) == BEQ){
			if(field0(instr) == field0(oldState->IFID.instr)){
				instr = NOOPINSTRUCTION;
			}
			else if(field1(instr) == field0(oldState->IFID.instr)){
				instr = NOOPINSTRUCTION;
			}
			else{
				++pc;
			}
		}
	}
	else{
		++pc;
	}
	
	newState->IFID.instr = instr;
	newState->IFID.pcPlus1 = pc;
	newState->pc = pc;
}

/*------------------ ID stage ----------------- */
void idStage(stateType* oldState, stateType* newState){
	
	int instr = oldState->IFID.instr;
	int pcPlus1 = oldState->IFID.pcPlus1;
	int readRegA = 0;
	int readRegB = 0;
	int offset = 0;
	
	//ADD or NAND
	if(opcode(instr) == ADD || opcode(instr) == NAND){
		readRegA = oldState->reg[field1(instr)];
		readRegB = oldState->reg[field2(instr)];
	}
	// LW or SW or BEQ
	else if(opcode(instr) == LW || opcode(instr) == SW || opcode(instr) == BEQ){
		readRegA = oldState->reg[field0(instr)];
		readRegB = oldState->reg[field1(instr)];
		offset = signExtend(field2(instr));
	}
	
	newState->IDEX.instr = instr;
	newState->IDEX.pcPlus1 = pcPlus1;
	newState->IDEX.offset = offset;
	newState->IDEX.readRegA = readRegA;
	newState->IDEX.readRegB = readRegB;
}

/*------------------ EX stage ----------------- */
void exStage(stateType* oldState, stateType* newState){
	dataForward(oldState); //check for hazards
	
	int instr = oldState->IDEX.instr;
	int pcPlus1 = oldState->IDEX.pcPlus1;
	int readRegA = oldState->IDEX.readRegA;
	int readRegB = oldState->IDEX.readRegB;
	int offset = oldState->IDEX.offset;
	int aluResult = 0;
	int branchTarget = 0;
	int readReg = 0;
	int op = opcode(instr);
	
	if(op == ADD || op == NAND){
		if(op == ADD){
			aluResult = readRegA + readRegB;
		}
		else{
			aluResult = ~(readRegA & readRegB);
		}
	}
	else if(op == LW || op == SW){
		aluResult = readRegB + offset;
		readReg = readRegA;
	}
	else if(op == BEQ){
		aluResult = readRegA - readRegB;
		branchTarget = offset + pcPlus1;
		newState->branches++;
	}
	
	newState->EXMEM.instr = instr;
	newState->EXMEM.aluResult = aluResult;
	newState->EXMEM.branchTarget = branchTarget;
	newState->EXMEM.readReg = readReg;
	
}

/*------------------ MEM stage ----------------- */
void memStage(stateType* oldState, stateType* newState){
	
	int instr = oldState->EXMEM.instr;
	int branchTarget = oldState->EXMEM.branchTarget;
	int aluResult = oldState->EXMEM.aluResult;
	int readReg = oldState->EXMEM.readReg;
	int op = opcode(instr);
	int pc = oldState->pc;
	int writeData = 0;
	
	if(op == BEQ){
		if(aluResult == 0){
			pc = branchTarget;
			newState->IFID.instr = NOOPINSTRUCTION;
			newState->IDEX.instr = NOOPINSTRUCTION;
			newState->EXMEM.instr = NOOPINSTRUCTION;
			newState->mispreds++;
		}
	}
	else if(op == LW){
		writeData = oldState->dataMem[aluResult];
	}
	else if(op == SW){
		oldState->dataMem[aluResult] = readReg;
	}
	else if(op == ADD || op == NAND){
		writeData = aluResult;
	}
	
	newState->MEMWB.instr = instr;
	newState->MEMWB.writeData = writeData;
}

/*------------------ WB stage ----------------- */
void wbStage(stateType* oldState, stateType* newState){
	
	int instr = oldState->MEMWB.instr;
	int writeData = oldState->MEMWB.writeData;
	int writeReg = 1;
	int op = opcode(instr);
	
	if(op == ADD || op == NAND){
		writeReg = field2(instr);
		newState->reg[writeReg] = writeData;
	}
	else if(op == LW){
		writeReg = field0(instr);
		newState->reg[writeReg] = writeData;
	}
	
	newState->WBEND.instr = instr;
	newState->WBEND.writeData = writeData;
	newState->retired++;
}

/*------------------ Hazard handeling ----------------- */
void dataForward(stateType* state){
	int regA = field0(state->IDEX.instr);
	int regB = field1(state->IDEX.instr);
	int op = opcode(state->IDEX.instr);
	
	//WBEND state
	if(op == ADD || op == NAND){
		if(regA == field2(state->WBEND.instr)){
			state->IDEX.readRegA = state->WBEND.writeData;
		}
		else if(regB == field2(state->WBEND.instr)){
			state->IDEX.readRegB = state->WBEND.writeData;
		}
	}
	else if(op == LW ){
		if(regA == field1(state->WBEND.instr) ) {
			state->IDEX.readRegA = state->WBEND.writeData;
		}
		if(regB == field1(state->WBEND.instr) ) {
			state->IDEX.readRegB = state->WBEND.writeData;
		}
	}
	
	//MEMWB stage
	if(op == ADD || op == NAND){
		if(regA == field2(state->MEMWB.instr)){
			state->IDEX.readRegA = state->MEMWB.writeData;
		}
		else if(regB == field2(state->MEMWB.instr)){
			state->IDEX.readRegB = state->MEMWB.writeData;
		}
	}
	else if(op == LW ){
		if(regA == field1(state->MEMWB.instr)) {
			state->IDEX.readRegA = state->MEMWB.writeData;
		}
		if(regB == field1(state->MEMWB.instr)) {
			state->IDEX.readRegB = state->MEMWB.writeData;
		}
	}
	
	//EXMEM stage
	if(op == ADD || op == NAND){
		if(regA == field2(state->EXMEM.instr)){
			state->IDEX.readRegA = state->EXMEM.aluResult;
		}
		else if(regB == field2(state->EXMEM.instr)){
			state->IDEX.readRegB = state->EXMEM.aluResult;
		}
	}
	else if(op == LW ){
		if(regA == field1(state->EXMEM.instr) || regB == field1(state->EXMEM.instr)) {
			printf("Error the simulator should have already stalled");
			exit(EXIT_FAILURE);
		}
	}
}
