#include <stdio.h>
#include "mpc.h"

/* Declare a buffer for user input of size 2048 */

static char input[2048];

long eval(mpc_ast_t* t);
long eval_op(long x, char* op, long y);

int main(int argc, char** arv) {
	/* Create some parses */ 
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Galisp = mpc_new("galisp");
	
	/* Define them with the following language */
	mpca_lang(MPCA_LANG_DEFAULT, 
		"																								\
			number		: /-?[0-9]+(\\.[0-9])?/													;				\
			operator	: '+' | '-' | '*' | '/' | '%' | \"add\" | \"sub\" | \"mul\" | \"div\"	;				\
			expr		: <number> | '(' <operator> <expr> + ')'						; 				\
			galisp 		: /^/ <operator> <expr>+ /$/									;				\
		",
		Number, Operator, Expr, Galisp);
	
	
	

	 /* Print Version and Exit Information */
	 puts("GALISP version 0.0001");
	 puts("Press Ctrl+c to Exit \n");
	 
	 /* In a never ending loop */
	 while(1) {
		/* Output our prompt */
		fputs("GALISP> ", stdout);
		
		/* Read a line of user input of maximum size 2048*/
		fgets(input, 2048, stdin);
		
		/* Attempt to parse input */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Galisp, &r)) {
			long result = eval(r.output);
			printf("%li\n", result);
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);

	 }
	mpc_cleanup(4, Number, Operator, Expr, Galisp);
	return 0;
}
	 
	 long eval(mpc_ast_t* t) {
		 
		 if(strstr(t->tag, "number")) {
				return atoi(t->contents);
		 }
		 
		 char* op = t->children[1]->contents;
		 
		 long expr = eval(t->children[2]);
		 
		 int i=3;
		 while (strstr(t->children[i]->tag, "expr")) {
			 expr = eval_op(expr, op, eval(t->children[i]));
			 i++;
		 }
		 
		 return expr;
	 } 
	 
	 long eval_op(long x, char* op, long y) {
		 if (strcmp(op, "+") == 0) { return x + y; } 
		 if (strcmp(op, "-") == 0) { return x - y; }
		 if (strcmp(op, "*") == 0) { return x * y; }
		 if (strcmp(op, "/") == 0) { return x / y; }
		 return 0;
	 }
	
