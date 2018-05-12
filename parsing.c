#include <stdio.h>
#include "mpc.h"

/* Declare a buffer for user input of size 2048 */

static char input[2048];

long eval(mpc_ast_t* t);

typedef struct {
	int type;
	long num;
	int err;
} lval;

enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval lval_num(long x) {
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
	return v;
}

lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = xl
	return v;
}

void lval_print(lval v) {
	switch(v.type) {
		case LVAL_NUM: printf("%li", v.num); break;
		case LVAL_ERR:
			if (v.err == LERR_DIV_ZERO) {
				printf("Error: Division by zero!");
			}
			if (v.err == LERR_BAD_OP) {
				printf("Error: Invalid Operator!");
			}	
			if (v.err == LERR_BAD_NUM) {
				printf("Error: Invalid Number!");
			}		
		break;
	}
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

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
		lval result = eval(r.output);
		lval_println(result);
		mpc_ast_delete(r.output);
	 }
	 
	 lval eval(mpc_ast_t* t) {
		 
		 if(strstr(t->tag, "number")) {
			 errno = 0;
			 long x = strtol(t->contents, NULL, 10);
			 return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
		 }
		 
		 char* op = t->children[1]->contents;
		 
		 lval expr = eval(t->children[2]);
		 
		 int i=3;
		 while (strstr(t->children[i]->tag, "expr")) {
			 expr = eval_op(expr, op, eval(t->children[i]));
			 i++;
		 }
	 } 
	 
	 lval eval_op(lval x, char* op, lval y) {
		 if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); } 
		 if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
		 if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
		 if (strcmp(op, "/") == 0) { 
			return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num); 
		 }
		 return lval_err(LERR_BAD_OP);
	 }
	
	mpc_cleanup(4, Number, Operator, Expr, Galisp);
	return 0;
}