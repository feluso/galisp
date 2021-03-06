#include <stdio.h>
#include "mpc.h"
#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }


/* Declare a buffer for user input of size 2048 */

static char input[2048];
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;


typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
  int type;
  long num;
  // Error and symbol types have some string data 
  char* sym;
  lbuiltin fun;
  
  char* err;
  // Conut and pointer to a list of lval*
  int count;
  struct lval** cell;
};

struct lenv {
  int count;
  char** syms;
  lval** vals;
};




enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

char* ltype_name(int t) {
  switch(t) {
  case LVAL_FUN: return "Function";
  case LVAL_NUM: return "Number";
  case LVAL_ERR: return "Error";
  case LVAL_SYM: return "Symbol";
  case LVAL_SEXPR: return "S-expression";
  case LVAL_QEXPR: return "Q-expression";
  default: return "Unknown";
  }
}


lval* lval_take(lval* v, int i); 
lval* lval_pop(lval* v, int i);
lval* lval_eval(lenv* e, lval* v);
lval* lval_read(mpc_ast_t* t);
lval* lval_add(lval* v, lval* x);
void lval_print(lval *v);
void lval_expr_print(lval *v, char open, char close); 

lval* lval_join(lval* x, lval* y);

lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  // Create a variable list
  va_list va;
  va_start(va, fmt);

  // Allocate 512 bytes of memory
  v->err = malloc(512);

  // print the error string with a max of 511 characters
  vsnprintf(v->err, 511, fmt, va);

  // Reallocate to a number of bytes that are used
  v->err = realloc(v->err, strlen(v->err)+1);

  // Cleanup
  va_end(va);

  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

void lval_del(lval* v) {
  switch(v->type) {
  case LVAL_NUM: break;
  case LVAL_FUN: break;
		
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;
		
  case LVAL_SEXPR: 
  case LVAL_QEXPR: 
    for(int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
			
    free(v->cell);
    break;
  }
	
  free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number '%d'", x);
}

lval* lval_read(mpc_ast_t* t) {
  if(strstr(t->tag, "number")) { return lval_read_num(t); }
  if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	
  lval* x = NULL;
  if(strcmp(t->tag, ">") == 0) { x= lval_sexpr(); }
  if(strstr(t->tag, "sexpr")) { x= lval_sexpr(); }
  if(strstr(t->tag, "qexpr")) { x= lval_qexpr(); }
	
  for (int i=0; i < t->children_num; i++) {
    if(strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if(strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if(strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if(strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if(strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
	
  return x;
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}


void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
		
    if(i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
  case LVAL_NUM: printf("%li", v->num); break;
  case LVAL_ERR: printf("Error: %s", v->err); break;
  case LVAL_SYM: printf("%s", v->sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  case LVAL_FUN: printf("<function>"); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_copy(lval* v) {

  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type){
  case LVAL_FUN: x->fun = v->fun; break;
  case LVAL_NUM: x->num = v->num; break;

  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err); break;
    
  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym); break;
    
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval*) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
}

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval* lenv_get(lenv* e, lval* k) {
  /*iterate over all items in environment */
  for (int i = 0; i < e->count; i++) {
    /*Check if the stored string matches the symbol string */
    /* If it does , return a copy of the value */
    if(strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  /* If no symbol found return error */
  return lval_err("unbound symbol! '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
  /* Checks if variables exist */

  for (int i = 0; i < e->count; i++) {
    /* if found, delete and overwrite */
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
 
  }

  /* If nothing found, create new. */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /* Copy conetnts into a new location */
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
  
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
		 
  for (int i=0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }
		 
  for (int i=0; i < v->count; i++) {
    if(v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }
		
  if (v->count == 0) { return v; }
		
  if (v->count == 1) { return lval_take(v,0); }

  /* ensure first element is a function. */
  lval* f = lval_pop(v, 0);
  if(f->type != LVAL_FUN) {
    lval_del(v); lval_del(f);
    return lval_err("S-expression does not start with symbol!");
  }
		 
  lval* result = f->fun(e, v);
  lval_del(f);
  return result;
} 


lval* lval_eval(lenv* e,lval* v) {
  if(v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if(v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }	
  return v;
}

lval* lval_join(lval* x, lval* y) {

  /* For each cell in 'y' add it to 'x' */
  while(y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* Delete the empty 'y' and return 'x' */
  lval_del(y);
  return x;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];
	 
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
 
  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
 
  return x;
}
	 
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}


lval* builtin_op(lenv* e, lval* a, char* op) {
	 
  for(int i = 0; i < a->count; i++) {
    if(a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }
	 
  lval* x = lval_pop(a, 0);
	 
  if((strcmp(op, "-") == 0) && a->count == 0) {
    x->num =-x->num;
  }
		 
  while(a->count > 0) {
    lval* y = lval_pop(a, 0);
    if (strcmp(op, "+") == 0) { x->num += y->num; } 
    if (strcmp(op, "-") == 0) { x->num -= y->num; } 
    if (strcmp(op, "*") == 0) { x->num *= y->num; } 
    if (strcmp(op, "/") == 0) { 
      if(y->num == 0) {
	lval_del(x); lval_del(y);
	x = lval_err("Division by cero"); break;
      }
      x->num /= y->num;
    }	 
    lval_del(y);
  }
	 
  lval_del(a);
  return x;
}
	 

lval* builtin_head(lenv* e, lval* qexpr) {
  LASSERT(qexpr, qexpr->count == 1 , "Function 'head' passed too many arguments. Got %i, Expected %i", qexpr->count, 1);
  LASSERT(qexpr, qexpr->cell[0]->type == LVAL_QEXPR , "Function 'head' passed incorrect type!, Got %s, Expected %s.", ltype_name(qexpr->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(qexpr, qexpr->cell[0]->count != 0 , "Function 'head' passed {}");

  //Take first argument
  lval* firstQexpr = lval_take(qexpr, 0);
  //Delete everything until theres only the first one
  while(firstQexpr->count > 1) {
    lval_del(lval_pop(firstQexpr, 1));
  }
  return firstQexpr;
}

	 
lval* builtin_tail(lenv* e, lval* qexpr) {
  LASSERT(qexpr, qexpr->count == 1 , "Function 'tail' passed too many arguments Got %i, Expected %i", qexpr->count, 1);
  LASSERT(qexpr, qexpr->cell[0]->type == LVAL_QEXPR , "Function 'tail' passed incorrect type! Got %s, Expected %s.", ltype_name(qexpr->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(qexpr, qexpr->cell[0]->count != 0 , "Function 'tail' passed {}");
  //Take first argument
  lval* firstQexpr = lval_take(qexpr, 0);

  //Delete only the first element
  lval_del(lval_pop(firstQexpr, 0));
  return firstQexpr;
}


lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT(a, a->count == 1, "Function, 'eval' passed too many arguments! Got %i, Expected %i", a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' passed incorrect type! Got %s, Expected %s.", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {

  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed incorrect type Got %s, Expected %s.", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k); lval_del(v);
}

lval* builtin_def(lenv* env, lval* a) {
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'def' passed incorrect type! Got %s, Expected %s.", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  //First argument is the symbol list
  lval* syms = a->cell[0];

  //Ensure all elements of first list are symbols
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM, "Function 'def' cannot define non symbols");
  }


  //Check correct number of symbols and values
  LASSERT(a, syms->count == a->count-1, "Function 'def' contains incorrect number of values");

  for (int i = 0; i < syms->count; i++) {
    lenv_put(env, syms->cell[i], a->cell[i+1]);
  }

  lval_del(a);
  return lval_sexpr();
}

void lenv_add_builtins(lenv* e) {
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
  lenv_add_builtin(e, "def", builtin_def);
}

int main(int argc, char** arv) {
  /* Create some parses */ 
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Galisp = mpc_new("galisp");
	
  /* Define them with the following language */
  mpca_lang(MPCA_LANG_DEFAULT, 
	    "																								\
			number		: /-?[0-9]+/													; \
			symbol		: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/                                                                               ; \
			sexpr		: '(' <expr>* ')'												; \
			qexpr		: '{' <expr>* '}'												; \
			expr		: <number> | <symbol> | <sexpr> | <qexpr> 						; \
			galisp 		: /^/ <expr>* /$/												; \
		",
	    Number, Symbol, Sexpr, Qexpr, Expr, Galisp);
	
	
	

  /* Print Version and Exit Information */
  puts("GALISP version 0.0006");
  puts("Press Ctrl+c to Exit \n");

  lenv* e = lenv_new();
  lenv_add_builtins(e);
  
  /* In a never ending loop */
  while(1) {
    /* Output our prompt */
    fputs("GALISP> ", stdout);
		
    /* Read a line of user input of maximum size 2048*/
    fgets(input, 2048, stdin);
		
		
    /* Attempt to parse input */
    mpc_result_t r;
		

    if (mpc_parse("<stdin>", input, Galisp, &r)) {
      lval* x = lval_eval(e, lval_read(r.output));
      // lval* x = lval_read(r.output);
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {    
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
		
		
  }

  lenv_del(e);
	 

	
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Galisp);
  return 0;
}
