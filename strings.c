
#include <stdio.h>
#include "mpc.h"
#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }

#define LASSERT_NUM(parameter, lval, size)				\
  LASSERT(lval, lval->count == size, "Function %s passed too many arguments. Got %i, Expected %i", parameter, lval->count, size); \

#define LASSERT_TYPE(parameter, lval, position, lvalType)			\
  LASSERT(lval, lval->cell[position]->type == lvalType , "Function %s passed incorrect type!, Got %s, Expected %s.", parameter, ltype_name(lval->cell[position]->type), ltype_name(lvalType)); \

/* Declare a buffer for user input of size 2048 */

static char input[2048];
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

 /* Create some parses */ 
mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr ;
mpc_parser_t* Galisp;


typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
  int type;

  // basic
  long num;
  char* sym;
  char* err;
  char* str;
  
  // functions
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;

  // Expression
  int count;
  struct lval** cell;
};

struct lenv {
  lenv* parent;
  int count;
  char** syms;
  lval** vals;
};




enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR, LVAL_STR };

char* ltype_name(int t) {
  switch(t) {
  case LVAL_FUN: return "Function";
  case LVAL_NUM: return "Number";
  case LVAL_ERR: return "Error";
  case LVAL_SYM: return "Symbol";
  case LVAL_STR: return "String";
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
void lval_del(lval* v);
lval* lval_copy(lval* v);

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

lval* lval_str(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_STR;
  v->str = malloc(strlen(s) + 1);
  strcpy(v->str, s);
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
  v->builtin = func;
  return v;
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

void lval_del(lval* v) {
  switch(v->type) {
  case LVAL_NUM: break;
  case LVAL_FUN:
    if(!v->builtin) {
      lval_del(v->formals);
      lval_del(v->body);
      lenv_del(v->env);      
    }
    break;
		
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;
  case LVAL_STR: free(v->str); break;
		
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

lval* lval_read_string(mpc_ast_t* t) {
  char* string = t->contents;
  string[strlen(string)-1] = '\0';

  char* unescaped = malloc(strlen(string+1)+1);
  strcpy(unescaped, string+1);

  unescaped = mpcf_unescape(unescaped);

  lval* lval_string = lval_str(unescaped);
  free(unescaped);
  return lval_string;
}

lval* lval_read(mpc_ast_t* t) {
  if(strstr(t->tag, "number")) { return lval_read_num(t); }
  if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	
  lval* x = NULL;
  if(strcmp(t->tag, ">") == 0) { x= lval_sexpr(); }
  if(strstr(t->tag, "sexpr")) { x= lval_sexpr(); }
  if(strstr(t->tag, "qexpr")) { x= lval_qexpr(); }
  if(strstr(t->tag, "string")) { x= lval_read_string(t); }
	
  for (int i=0; i < t->children_num; i++) {
    if(strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if(strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if(strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if(strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if(strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    if(strstr(t->children[i]->tag, "comment")) { continue; }
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

void lval_print_str(lval* v) {
  char* escaped = malloc(strlen(v->str) + 1);
  strcpy(escaped, v->str);

  escaped = mpcf_escape(escaped);

  printf("\"%s\"", escaped);
  free(escaped);
}

void lval_print(lval* v) {
  switch (v->type) {
  case LVAL_NUM: printf("%li", v->num); break;
  case LVAL_ERR: printf("Error: %s", v->err); break;
  case LVAL_SYM: printf("%s", v->sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  case LVAL_FUN:
    if(v->builtin) {
      printf("<builtin>");
    } else {
      printf("(\\ "); lval_print(v->formals); putchar(' '); lval_print(v->body); putchar(')');
    }
    break;
  case LVAL_STR: lval_print_str(v); break;
    
  }
}



void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lenv* lenv_copy(lenv* env) {
  lenv* copy = malloc((sizeof(lenv)));
  copy->parent = env->parent;
  copy->count = env->count;
  copy->syms = malloc(sizeof(char*) * env->count);
  copy->vals = malloc(sizeof(lval*) * env->count);
  for (int i = 0; i < env->count; i++) {
    copy->syms[i] = malloc(strlen(env->syms[i]) + 1);
    strcpy(copy->syms[i], env->syms[i]);
    copy->vals[i] = lval_copy(env->vals[i]);
  }
  return copy;
  
}

lval* lval_copy(lval* v) {

  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type){
  case LVAL_FUN:
    if (v->builtin) {
      x->builtin = v->builtin;
    } else {
      x->builtin = NULL;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    }
    break;
  case LVAL_NUM: x->num = v->num; break;

  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err); break;
    
  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym); break;

  case LVAL_STR:
    x->str = malloc(strlen(v->str) + 1);
    strcpy(x->str, v->str); break;
    
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
  e->parent = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

lval* lval_lambda(lval* formals, lval* body) {
  lval* funct = malloc(sizeof(lval));
  funct->type = LVAL_FUN;

  funct->builtin = NULL;

  funct->env = lenv_new();
  funct->formals = formals;
  funct->body = body;
  return funct;

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

  if(e->parent) {
    return lenv_get(e->parent, k);
  } else {
    /* If no symbol found return error */
    return lval_err("unbound symbol! '%s'", k->sym);
  }
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

void lenv_def(lenv* env, lval* name, lval* value) {
  //Search a parent until we get to the parent environment
  if (env->parent) {
    lenv_def(env->parent, name, value);

  }
  
  lenv_put(env, name, value);
  
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

lval* lval_call(lenv* env, lval* fun, lval* values) {
  if(fun->builtin) {
    return fun->builtin(env, values);
  }

  int total = fun->formals->count;
  int given = values->count;

   while(values->count) {

    if(fun->formals->count == 0) {
      lval_del(values); return lval_err("function passed too many arguments, expected %i, got %i", given, total);
    }

    lval* sym = lval_pop(fun->formals, 0);

    if(strcmp(sym->sym, "&") == 0) {

      if(fun->formals->count != 1) {
        lval_del(values);
        return lval_err("Function format invalid. Symbol '&' not followed by single symbol.");
      }

      lval* nsym = lval_pop(fun->formals, 0);
      lenv_put(fun->env, nsym, builtin_list(env, values));
      lval_del(sym); lval_del(nsym);
      break;
    }

    lval* value = lval_pop(values, 0);

    lenv_put(fun->env, sym, value);

    lval_del(sym); lval_del(value);

  }

  lval_del(values);

  if(fun->formals->count > 0 && strcmp(fun->formals->cell[0]->sym, "&") == 0) {

    if(fun->formals->count != 2) {
      lval_err("Funtion format invalid '&' not followed by single symbol.");
    }

    lval_del(lval_pop(fun->formals, 0));

    lval* sym = lval_pop(fun->formals, 0);
    lval* val = lval_qexpr();

    lenv_put(fun->env, sym, val);
    lval_del(sym); lval_del(val);

  }

  if(fun->formals->count == 0) {

    fun->env->parent = env;

    return builtin_eval(fun->env, lval_add(lval_sexpr(), lval_copy(fun->body)));
  } else {
    return lval_copy(fun);
  }
  
  
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

  lval* result = lval_call(e, f, v);
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

lval* builtin_var(lenv* env, lval* a, char* func) {
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);
  //First argument is the symbol list
  lval* syms = a->cell[0];

  //Ensure all elements of first list are symbols
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM, "Function 'def' cannot define non symbols");
  }


  //Check correct number of symbols and values
  LASSERT(a, syms->count == a->count-1, "Function 'def' contains incorrect number of values");

  for (int i = 0; i < syms->count; i++) {
    if(strcmp(func, "=") == 0) {
      lenv_put(env, syms->cell[i], a->cell[i+1]);
    } else if (strcmp(func, "def") == 0) {
      lenv_def(env, syms->cell[i], a->cell[i+1]);
    }
  }

  lval_del(a);
  return lval_sexpr();
}

lval* builtin_def(lenv* env, lval* a) {
  return builtin_var(env, a, "def");
}

lval* builtin_put(lenv* env, lval* a) {
  return builtin_var(env, a, "=");
}


lval* builtin_lambda(lenv* env, lval* val) {
  LASSERT_NUM("\\", val, 2);
  LASSERT_TYPE("\\", val, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", val, 1, LVAL_QEXPR);

  for(int i=0; i < val->cell[0]->count; i++) {
    int formalsType =  val->cell[0]->cell[i]->type;
    LASSERT(val, formalsType == LVAL_SYM, "Cannot pass non symbol characters, got %s, expected %s", ltype_name(formalsType), ltype_name(LVAL_SYM));
  }

  lval* formals =  lval_pop(val, 0);
  lval* body =  lval_pop(val, 0);
  lval_del(val);
  return lval_lambda(formals, body);
}

lval* builtin_ord(lenv* env, lval* values, char* operator) {
  LASSERT_NUM(operator, values, 2);
  LASSERT_TYPE(operator, values, 0, LVAL_NUM);
  LASSERT_TYPE(operator, values, 1, LVAL_NUM);

  lval* first = lval_pop(values, 0);
  lval* second = lval_pop(values, 0);
  lval_del(values);

  lval* result;
  if(strcmp("<", operator) == 0) {
      result = lval_num(first->num < second->num);
  } else if(strcmp(">", operator) == 0) {
      result = lval_num(first->num > second->num);
  } else if(strcmp(">=", operator) == 0) {
      result =  lval_num(first->num >= second->num);
  } else if(strcmp("<=", operator) == 0) {
      result = lval_num(first->num <= second->num);
  }

  lval_del(first); lval_del(second);
  return result;
}

lval* builtin_lt(lenv* env, lval* values) {
  return builtin_ord(env, values, "<");

}

lval* builtin_gt(lenv* env, lval* values) {
  return builtin_ord(env, values, ">");

}

lval* builtin_gte(lenv* env, lval* values) {
  return builtin_ord(env, values, ">=");

}

lval* builtin_lte(lenv* env, lval* values) {
  return builtin_ord(env, values, "<=");
}

int lval_eq(lval* first, lval* second) {
  if(first->type != second->type) { return 0; }
  switch(first->type) {
  case LVAL_NUM: return first->num == second->num; break;
  case LVAL_SYM: return strcmp(first->sym, second->sym) == 0; break;
  case LVAL_STR: return strcmp(first->str, second->str) == 0; break;
  case LVAL_ERR: return strcmp(first->err, second->err) == 0; break;
  case LVAL_FUN:
    if(first->builtin || second->builtin) {
      return first->builtin == second->builtin;
    } else {
      return lval_eq(first->formals, second->formals) && lval_eq(first->body, second->body);
    }
  case LVAL_QEXPR:
  case LVAL_SEXPR:
    if(first->count != second->count) { return 0; }
    for(int i = 0; i < first->count; i++) {
      if(lval_eq(first->cell[i], second->cell[i])) { return 0; }
    }
    return 1;
    break;
  }

  return 0;
}


lval* builtin_compare(lenv* env, lval* values, char* operator) {
  LASSERT_NUM(operator, values, 2);

  lval* first = lval_pop(values, 0);
  lval* second = lval_pop(values, 0);
  lval_del(values);

  lval* result;

  if(strcmp("==", operator) == 0) {
    result = lval_num(lval_eq(first, second));
  } else if(strcmp("!=", operator) == 0) {
    result = lval_num(!lval_eq(first, second));
  }

  lval_del(first); lval_del(second);
  return result;
}

lval* builtin_eq(lenv* env, lval* values) {
  return builtin_compare(env, values, "==");
}

lval* builtin_neq(lenv* env, lval* values) {
  return builtin_compare(env, values, "!=");
}

lval* builtin_if(lenv* env, lval* values) {
  LASSERT_NUM("if", values, 3);
  LASSERT_TYPE("if", values, 0, LVAL_NUM);
  LASSERT_TYPE("if", values, 1, LVAL_QEXPR);
  LASSERT_TYPE("if", values, 2, LVAL_QEXPR);

  int condition = values->cell[0]->num;
  lval* ifStatement = values->cell[1];
  lval* elseStatement = values->cell[2];

  ifStatement->type = LVAL_SEXPR;
  elseStatement->type = LVAL_SEXPR;

  lval* result;
  if(condition) {
    result = lval_eval(env, lval_pop(values, 1));
  } else {
    result = lval_eval(env, lval_pop(values, 2));
  }

  lval_del(values);
  return result;
}

lval* builtin_load(lenv* env, lval* filename) {
  LASSERT_NUM("load", filename, 1);
  LASSERT_TYPE("load", filename, 0, LVAL_STR);

  mpc_result_t r;
  if (mpc_parse_contents(filename->cell[0]->str, Galisp, &r)) {
    lval* expr = lval_read(r.output);
    mpc_ast_delete(r.output);

    while(expr->count) {
      lval* eval = lval_eval(env, lval_pop(expr, 0));
      if(eval->type == LVAL_ERR) {
	lval_println(eval);
      }
      lval_del(eval);
    }

    lval_del(expr);
    lval_del(filename);
    return lval_sexpr();
  } else {
    char* err_msg = mpc_err_string(r.error);
    mpc_err_delete(r.error);
    
    lval* err = lval_err("Could not load library %s", err_msg);
    free(err_msg);
    lval_del(filename);
    
    return err;
  }
}

lval* builtin_print(lenv* env, lval* values) {

  for(int i = 0; i < values->count; i++) {
    lval_print(values->cell[i]);
    putchar(' ');
  }

  putchar('\n');
  lval_del(values);

  return lval_sexpr();
}

lval* builtin_error(lenv* env, lval* values) {
  LASSERT_NUM("error", values, 1);
  LASSERT_TYPE("error", values, 0, LVAL_STR);

  lval* err = lval_err(values->cell[0]->str);
  lval_del(values);

  return err;
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
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "\\", builtin_lambda);
  lenv_add_builtin(e, "<", builtin_lt);
  lenv_add_builtin(e, ">", builtin_gt);
  lenv_add_builtin(e, "<=", builtin_lte);
  lenv_add_builtin(e, ">=", builtin_gte);
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_neq);
  lenv_add_builtin(e, "if", builtin_if);
  lenv_add_builtin(e, "load", builtin_load);
  lenv_add_builtin(e, "print", builtin_print);
  lenv_add_builtin(e, "error", builtin_error);
}


int main(int argc, char** argv) {
  /* Create some parses */ 
  Number = mpc_new("number");
  Symbol = mpc_new("symbol");
  String = mpc_new("string");
  Comment = mpc_new("comment");
  Sexpr = mpc_new("sexpr");
  Qexpr = mpc_new("qexpr");
  Expr = mpc_new("expr");
  Galisp = mpc_new("galisp");
	
  /* Define them with the following language */
  mpca_lang(MPCA_LANG_DEFAULT, 
	    "																                	\
			number		: /-?[0-9]+/													;\
			symbol		: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/                                                                              ;\
                        string          : /\"(\\\\.|[^\"])*\"/ ;                                                                                         \
                        comment          : /;[^\\r\\n]*/ ;                                                                                         \
                        sexpr		: '(' <expr>* ')'												; \
			qexpr		: '{' <expr>* '}'												;\
			expr		: <number> | <symbol> | <string> | <comment> | <sexpr> | <qexpr> 						                        ; \
			galisp 		: /^/ <expr>* /$/												;\
		",
	    Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Galisp);

  lenv* e = lenv_new();
  lenv_add_builtins(e);
  
  if(argc >= 2) {
    for(int i = 0; i < argc; i++) {

      lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));
      lval* x = builtin_load(e, args);

      if( x->type == LVAL_ERR) { lval_println(x); }

      lval_del(x);
    }
    return 0;
  }
	

  /* Print Version and Exit Information */
  puts("GALISP version 0.0008");
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
	 

	
  mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Galisp);
  return 0;
}
