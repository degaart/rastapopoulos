/*
	Generate standard stack frame for stdcall
	function
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char* siz2nam(int size) {
	static char buffer[100];
	
	switch(size) {
	case 1:
		return("byte");
	case 2:
		return("word");
	case 4:
		return("dword");
	case 8:
		return("qword");
	default:
		sprintf(buffer, "%d bytes", size);
		return(buffer);
	}		
}

int main()
{
	int params[100];			/* Parameter sizes */
	int locals[100];			/* Local variables sizes */
	char str[100];

	/* Read param sizes from user */	
	int param_space = 0;
	for(int i=0; i<100; i++) {
		printf("param%d size [0]: ", i);
		fgets(str, sizeof(str), stdin);
		if(!(*str))
			strcpy(str, "0");
		params[i] = atoi(str);
		if(!params[i])
			break;
		param_space += params[i];
	}

	int local_space = 0;
	for(int i=0; i<100; i++) {
		printf("local%d size [0]: ", i);
		fgets(str, sizeof(str), stdin);
		if(!(*str))
			strcpy(str, "0");
		locals[i] = atoi(str);
		if(!locals[i])
			break;
		local_space += locals[i];
	}
	
	printf("Function name: ");
	fgets(str, sizeof(str), stdin);
	if(!*str)
		return(1);
	str[strlen(str)-1] = '\0';

	int acc_param = 4;	
	for(int i=0; params[i]; i++) {
		printf("%%define _param%d bp+%d\t; %s\n", i, acc_param, siz2nam(params[i]));
		acc_param += params[i];
	}
	
	signed acc_local = 0;
	for(int i=0; locals[i]; i++) {
		acc_local += locals[i];
		printf("%%define _local%d bp-%d\t; %s\n", i, acc_local, siz2nam(locals[i]));
	}

	printf("%s:\n", str);
	printf("\t\tpush bp\n");
	printf("\t\tmov bp,sp\n");
	printf("\t\tsub sp, %d\n", local_space);
	printf("\n");
	printf("\t.return:\n");
	printf("\t\tadd sp, %d\n", local_space);
	printf("\t\tpop bp\n");
	printf("\t\tret %d\n", param_space);
	return(0);
}
