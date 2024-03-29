#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32

#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);

  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);

  cpy[strlen(cpy)-1] = '\0';

  return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#endif

/* Parser Declariations */

mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Lispy;

/* Forward Declarations */

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create Enumeration of Possible lval Types */
enum {
  LVAL_ERR,
  LVAL_FUN,
  LVAL_NUM,
  LVAL_QEXPR,
  LVAL_SEXPR,
  LVAL_STR,
  LVAL_SYM
};

char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_STR: return "String";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}


typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare new lval Struct */
struct lval {
  int type;

  /* Basic*/
  long num;
  char* err;
  char* sym;
  char* str;

  /* Functions */
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;

  /* Expressions */
  int count;
  lval** cell;
};

struct lenv {
  lenv* parent;

  int count;
  char** syms;
  lval** vals;
};

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->parent = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;

  return e;
}

void lval_del(lval* e);
lval* lval_err(char* fmt, ...);
lval* lval_copy(lval* e);

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

  /* Iterate over all the items in environment */
  for (int i = 0; i < e->count; i++) {
    /* Check if the stored string matches the symbol string */
    /* If it does, return a copy of the value */
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  /* If no symbol found first check in parent */
  if (e->parent) {
    return lenv_get(e->parent, k);
  } else {
    /* Otherwise return error */
    return lval_err("Unbound Symbol: %s", k->sym);
  }
}

lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));
  n->parent = e->parent;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);

  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }

  return n;
}

char* lenv_get_fname_from_builtin(lenv* e, lval *v) {
  /* Iterate over all the items in environment */
  for (int i = 0; i < e->count; i++) {
    /* Check if the function matches the lval's function */
    /* If it does, return a copy of the value */
    if (e->vals[i]->builtin == v->builtin) {
      return e->syms[i];
    }
  }

  /* If no function found in env return error */
  return "Could not find function in env";
}

void lenv_put(lenv* e, lval* k, lval* v) {
  /* Iterate over all the items in environment */
  /* This is to see if variable already exists */
  for (int i = 0; i < e->count; i++) {
    /* If the variable is found delete item at that position */
    /* And replace with variable supplied by user */
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* If no existing entry found allocate space for new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /* Copy contents of lval and symbol string into new location */
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
  /* Iterate until e has no parent */
  while (e->parent) {
    e = e->parent;
  }

  /* Put value in e */
  lenv_put(e, k ,v);
}

/* Construct a pointer to a new Number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;

  return v;
}

/* Construct a pointer to a new Error type lval */
lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  /* Create a va list and initialize it */
  va_list va;
  va_start(va, fmt);

  /* Allocate 512 bytes of space */
  v->err = malloc(512);

  /* printf the error string with a maximum of 511 characters */
  vsnprintf(v->err, 511, fmt, va);

  /* Reallocate to number of bytes actually used */
  v->err = realloc(v->err, strlen(v->err)+1);

  /* Cleanup our va list */
  va_end(va);

  return v;
}

/* Construct a pointer to a new Symbol type lval */
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);

  return v;
}

/* Construct a pointer to a new String type lval */
lval* lval_str(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_STR;
  v->str = malloc(strlen(s) + 1);
  strcpy(v->str, s);

  return v;
}

/* Construct a pointer to a new empty Sexpr type lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;

  return v;
}

/* Construct a pointer to a new empty Qexpr lval */
lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* Construct a pointer to a new function type lval */
lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;

  return v;
}

/* Construct a pointer to a new lambda type lval */
lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  /* Set builtin to Null */
  v->builtin = NULL;

  /* Build new environment */
  v->env = lenv_new();

  /* Set formals and body */
  v->formals = formals;
  v->body = body;

  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
    /* Do nothing specifal for number type */
    case LVAL_NUM: break;

    case LVAL_FUN:;
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->formals);
        lval_del(v->body);
      }
    break;

    /* For Err or Sym free the string data */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_STR: free(v->str); break;

    /* If Qexpr or Sexpr then delete all elements inside it */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      /* Also free the memory allocated to contain the pointers */
      free(v->cell);
    break;

  }

  /* Free the memory allocated for the lval struct itself */
  free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);

  if (errno != ERANGE) {
    return lval_num(x);
  } else {
    return lval_err("invalid number");
  }
}

lval* lval_read_str(mpc_ast_t* t) {
  /* Cut off the final quote character */
  t->contents[strlen(t->contents)-1] = '\0';

  /* Copy the string missing out the first quote character */
  char* unescaped = malloc(strlen(t->contents+1)+1);
  strcpy(unescaped, t->contents+1);

  /* Pass through the unescape function */
  unescaped = mpcf_unescape(unescaped);

  /* Construct a new lval using the string */
  lval* str = lval_str(unescaped);

  /* Free the string and return */
  free(unescaped);

  return str;
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_read(mpc_ast_t* t) {
  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
  if (strstr(t->tag, "string")) { return lval_read_str(t); }

  /* If root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    if (strstr(t->children[i]->tag, "comment")) { continue; }

    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/* Forward declare this function before we define it */
void lval_print(lenv* e, lval* v);

void lval_expr_print(lenv* e, lval* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++) {
    /* Print value contained within */
    lval_print(e, v->cell[i]);

    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }

  putchar(close);
}

void lval_print_str(lval* v) {
  /* make a copy of the string */
  char* escaped = malloc(strlen(v->str) + 1);
  strcpy(escaped, v->str);

  /* Pass it through the escape function */
  escaped = mpcf_escape(escaped);

  /* Print it between " characters */
  printf("\"%s\"", escaped);

  /* free the copied string */
  free(escaped);
}

void lval_print(lenv* e, lval* v) {
  switch (v->type) {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM: printf("%li", v->num); break;

    case LVAL_FUN:;
      if (v->builtin) {
        char* fname = lenv_get_fname_from_builtin(e, v);
        printf("Function: %s", fname);
      } else {
        printf("(\\ ");
        lval_print(e, v->formals);
        putchar(' ');
        lval_print(e, v->body);
        putchar(')');
      }
      break;

    /* In the case the type is an error */
    case LVAL_ERR: printf("Error: %s", v->err); break;

    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_STR: lval_print_str(v); break;
    case LVAL_SEXPR: lval_expr_print(e, v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(e, v, '{', '}'); break;
  }
}

/* Print an "lval" followed by a newline */
void lval_println(lenv* e, lval* v) { lval_print(e, v); putchar('\n'); }

lval* lval_take(lval* v, int i);
lval* lval_pop(lval* v, int i);
lval* lval_eval(lenv* e, lval* v);
lval* lval_call(lenv* e, lval* f, lval* a);

lval* lval_eval_sexpr(lenv* e, lval* v) {

  /* Evaluate the children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  /* Error checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* Empty expression */
  if (v->count == 0) { return v; }

  /* Single expression */
  if (v->count == 1) { return lval_take(v, 0); }

  /* Ensure first element is a function after evaluation */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval* err = lval_err(
        "S-Expression starts with incorrect type. "
        "Got %s, expected %s.",
        ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(v);
    lval_del(f);
    return err;
  }

  lval* result = lval_call(e, f, v);

  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  /* Evaluate S-expressions */
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }

  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }

  /* All other lval types remain the same */
  return v;
}

lval* lval_pop(lval* v, int i) {
  /* Find the item at "i" */
  lval* x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i+1],
      sizeof(lval*) * (v->count - i - 1));

  /* Decrease the count of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);

  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }

#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
    func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", func, index);

lval* builtin_op(lenv* e, lval* a, char* op) {
  /* Ensure all the arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE(op, a, i, LVAL_NUM);
  }

  /* Pop the first element */
  lval* x = lval_pop(a, 0);

  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -(x->num);
  }

  /* While there are still elements remaining */
  while (a->count > 0) {
    /* Pop the next element */
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero!");
        break;
      } else {
        x->num /= y->num;
      }
    }

    lval_del(y);
  }

  lval_del(a);

  return x;
}

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

lval* builtin_not(lenv* e, lval* a) {
  LASSERT_NUM("!", a, 1);
  LASSERT_TYPE("!", a, 0, LVAL_NUM);

  int r = !(a->cell[0]->num);
  lval_del(a);
  return lval_num(r);
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
  LASSERT_NUM(op, a, 2);
  LASSERT_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_TYPE(op, a, 1, LVAL_NUM);

  int r;

  if (strcmp(op, ">") == 0) {
    r = (a->cell[0]->num > a->cell[1]->num);
  }

  if (strcmp(op, "<") == 0) {
    r = (a->cell[0]->num < a->cell[1]->num);
  }

  if (strcmp(op, ">=") == 0) {
    r = (a->cell[0]->num >= a->cell[1]->num);
  }

  if (strcmp(op, "<=") == 0) {
    r = (a->cell[0]->num <= a->cell[1]->num);
  }

  if (strcmp(op, "&&") == 0) {
    r = (a->cell[0]->num && a->cell[1]->num);
  }

  if (strcmp(op, "||") == 0) {
    r = (a->cell[0]->num || a->cell[1]->num);
  }

  lval_del(a);

  return lval_num(r);
}

lval* builtin_gt(lenv* e, lval* a) {
  return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
  return builtin_ord(e, a, "<");
}

lval* builtin_ge(lenv* e, lval* a) {
  return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
  return builtin_ord(e, a, "<=");
}

lval* builtin_and(lenv* e, lval* a) {
  return builtin_ord(e, a, "&&");
}

lval* builtin_or(lenv* e, lval* a) {
  return builtin_ord(e, a, "||");
}

lval* builtin_head(lenv* e, lval* a) {
  LASSERT_NUM("head", a, 1);
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("head", a, 0);

  /* Otherwise, take first argument */
  lval* v = lval_take(a, 0);

  /* Delete all elements that are not head and return */
  while (v->count > 1) { lval_del(lval_pop(v,1)); }

  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT_NUM("tail", a, 1);
  LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("tail", a, 0);

  /* Otherwise, take first argument */
  lval* v = lval_take(a, 0);

  /* Delete first element and return */
  lval_del(lval_pop(v,0));

  return v;
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_lambda(lenv* e, lval* a) {
  /* Check two argumetns, each of which should be Q-Expressions */
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  /* Check first Q-Expr contains only symbols */
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
      "Cannot define non-symbol. Got %s, Expected %s.",
      ltype_name(a->cell[0]->cell[i]->type),ltype_name(LVAL_SYM));
  }

  /* Pop first two arguments and pass them to lval_lambda */
  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

lval* lval_join(lval* x, lval* y) {
  /* For each cell in 'y' add it to 'x' */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }
  /* Delete the empty 'y' and return 'x' */
  lval_del(y);
  return x;
}

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT_NUM("eval", a, 1);
  LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join_qexpr(lenv* e, lval* a) {
  for(int i = 0; i < a->count; i++) {
    LASSERT_TYPE("join", a, i, LVAL_QEXPR);
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_join_str(lenv* e, lval* a) {
  for(int i = 0; i < a->count; i++) {
    LASSERT_TYPE("join", a, i, LVAL_STR);
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    lval* y = lval_pop(a, 0);
    char* concatted = malloc(strlen(x->str) + strlen(y->str) + 1);
    concatted = strcat(x->str, y->str);
    x = lval_str(concatted);

    free(concatted);
    free(y);
  }

  lval_del(a);
  return x;
}

lval* builtin_join(lenv* e, lval* a) {
  lval* x = malloc(sizeof(lval));

  /* Switches based on the first cell's type */
  switch(a->cell[0]->type) {
    case LVAL_QEXPR: x = builtin_join_qexpr(e, a); break;
    case LVAL_STR: x = builtin_join_str(e, a); break;
  }

  return x;
}

lval* builtin_var(lenv* e, lval* a, char* func) {
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  /* First argument is symbol list */
  lval* syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
        "Function '%s' cannot define non-symbol. "
        "Got %s, expected %s.",
        func,
        ltype_name(syms->cell[i]->type),
        ltype_name(LVAL_SYM));
  }

  /* Check correct number of symbols and values */
  LASSERT(a, (syms->count == a->count-1),
      "Function '%s' passed too many arguments for symbols. "
      "Got %i, Expected %i.",
      func,
      syms->count,
      a->count-1);

  /* Assign copies of values to symbols */
  for (int i = 0; i < syms->count; i++) {
    /* if 'def' define in globally. if 'put' define in locally */
    if (strcmp(func, "def") == 0) {
      lenv_def(e, syms->cell[i], a->cell[i+1]);
    } else if (strcmp(func, "=")) {
      lenv_put(e, syms->cell[i], a->cell[i+1]);
    }
  }

  lval_del(a);
  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

int lval_eq(lval* x, lval* y) {

  /* Different types are always unequal */
  if (x->type != y->type) { return 0; }

  /* Compare based upon type */
  switch(x->type) {
    /* Compare number values */
    case LVAL_NUM: return (x->num == y->num);

    /* Compare string values */
    case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
    case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
    case LVAL_STR: return (strcmp(x->str, y->str) == 0);

    /* If builtin fn compare, otherwise compare formals and body */
    case LVAL_FUN:;
      if (x->builtin || y->builtin) {
        return x->builtin == y->builtin;
      } else {
        return lval_eq(x->formals, y->formals) &&
          lval_eq(x->body, y->body);
      }

    /* If list compare every individual element */
    case LVAL_QEXPR:
    case LVAL_SEXPR:;
      if (x->count != y->count) { return 0; }
      for (int i = 0; i < x->count; i++) {
        /* If any element not equal then the whole list not equal */
        if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
      }

      /* Otherwise lists must be equal */
      return 1;
    break;
  }
  return 0;
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
  LASSERT_NUM(op, a, 2);

  int r;
  if (strcmp(op, "==") == 0) {
    r = lval_eq(a->cell[0], a->cell[1]);
  }
  if (strcmp(op, "!=") == 0) {
    r = !lval_eq(a->cell[0], a->cell[1]);
  }

  lval_del(a);
  return lval_num(r);
}

lval* builtin_eq(lenv* e, lval* a) {
  return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
  return builtin_cmp(e, a, "!=");
}

lval* builtin_if(lenv* e, lval* a) {
  LASSERT_NUM("if", a, 3);
  LASSERT_TYPE("if", a, 0, LVAL_NUM);
  LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
  LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

  /* Mark both expressions as evaluate-able */
  lval* x;
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;

  if (a->cell[0]->num) {
    /* If condition is true evaluate first expression */
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    /* Otherwise evaluate second expression */
    x = lval_eval(e, lval_pop(a, 2));
  }

  /* Delete argument list and return */
  lval_del(a);
  return x;
}

lval* builtin_load(lenv* e, lval* a) {
  LASSERT_NUM("load", a, 1);
  LASSERT_TYPE("load", a, 0, LVAL_STR);

  /* Parse file given by string name */
  mpc_result_t r;

  if (mpc_parse_contents(a->cell[0]->str, Lispy, &r)) {

    /* Read contents */
    lval* expr = lval_read(r.output);
    mpc_ast_delete(r.output);

    /* evaluate each expression */
    while (expr->count) {
      lval* x = lval_eval(e, lval_pop(expr, 0));

      /* If evaluate leads to error print it */
      if (x->type == LVAL_ERR) { lval_println(e, x); }
      lval_del(x);
    }

    /* Delete expression and arguments */
    lval_del(expr);
    lval_del(a);

    /* Return empty list */
    return lval_sexpr();

  } else {
    /* Get parse error as string */
    char* err_msg = mpc_err_string(r.error);
    mpc_err_delete(r.error);

    /* Create new error message using it */
    lval* err = lval_err("Could not load Library %s", err_msg);
    free(err_msg);
    lval_del(a);

    return err;
  }
}


lval* builtin_print(lenv* e, lval* a) {
  /* Print each argument followed by a space */
  for (int i = 0; i < a->count; i++) {
    lval_print(e, a->cell[i]);
    putchar(' ');
  }

  /* Print a newline and delete arguments */
  putchar('\n');
  lval_del(a);

  return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
  LASSERT_NUM("error", a, 1);
  LASSERT_TYPE("error", a, 0, LVAL_STR);

  /* Construct error from first argument */
  lval* err = lval_err(a->cell[0]->str);

  lval_del(a);

  return err;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  /* List functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);

  /* Lambda */
  lenv_add_builtin(e, "\\", builtin_lambda);

  /* Mathematical functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

  /* Arithmetic comparison functions */
  lenv_add_builtin(e, ">", builtin_gt);
  lenv_add_builtin(e, "<", builtin_lt);
  lenv_add_builtin(e, ">=", builtin_ge);
  lenv_add_builtin(e, "<=", builtin_le);
  lenv_add_builtin(e, "&&", builtin_and);
  lenv_add_builtin(e, "||", builtin_or);
  lenv_add_builtin(e, "!", builtin_not);

  /* Equality*/
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_ne);

  lenv_add_builtin(e, "if", builtin_if);

  /* Variable functions */
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);

  /* String functions */
  lenv_add_builtin(e, "load", builtin_load);
  lenv_add_builtin(e, "error", builtin_error);
  lenv_add_builtin(e, "print", builtin_print);
}

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
    /* Copy functions numbers directly */
    case LVAL_NUM: x->num = v->num; break;

    case LVAL_FUN:;
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
      break;

    /* Copy strings using malloc and strcpy */
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err);
      break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
      break;

    case LVAL_STR:
      x->str = malloc(strlen(v->str) + 1);
      strcpy(x->str, v->str);
      break;

    /* Copy lists by copying each sub-expression */
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

lval* lval_call(lenv* e, lval* f, lval* a) {

  /* If builtin then simply call that */
  if (f->builtin) { return f->builtin(e, a); }

  /* Record argument counts */
  int given = a->count;
  int total = f->formals->count;

  /* while arguments still remain to be processed */
  while (a->count) {

    /* If we've ran out of formal arguments to bind */
    if (f->formals->count == 0) {
      lval_del(a); return lval_err(
          "Function passed too many arguments. "
          "Got %i, expected %i.", given, total);
    }

    /* Pop the first symbol from the formals */
    lval* sym = lval_pop(f->formals, 0);

    /* special case to deal with '&'  */
    if (strcmp(sym->sym, "&") == 0) {

      /* Ensure '&' is followed by another symbol */
      if (f->formals->count != 1) {
        lval_del(a);
        return lval_err("Function format invalid. "
            "Symbol '&' not followed by single symbol.");
      }

      /* Next formal should be bound to remaining arguments*/
      lval* nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym);
      lval_del(nsym);
      break;
    }

    /* Pop the next argument from the list */
    lval* val = lval_pop(a, 0);

    /* Bind a copy into the function's environment */
    lenv_put(f->env, sym, val);

    /* Delete symbol and value */
    lval_del(sym);
    lval_del(val);
  }

  /* Argument list is now bound so can be cleaned up */
  lval_del(a);

  /* If '&' remains in formal list bind to empty list */
  if (f->formals->count > 0 &&
      strcmp(f->formals->cell[0]->sym, "&") == 0) {

    /* Check to ensure that & is not passed invalidly */
    if (f->formals->count != 2) {
      return lval_err("Function format invalid. "
          "Symbol '&' not follow by single symbol.");
    }

    /* Pop and delete '&' symbol */
    lval_del(lval_pop(f->formals, 0));

    /* Pop next symbol and create empty list */
    lval* sym = lval_pop(f->formals, 0);
    lval* val = lval_qexpr();

    /* Bind to environment and delete */
    lenv_put(f->env, sym, val);
    lval_del(sym);
    lval_del(val);
  }

  /* If all formals have been bound evaluate */
  if (f->formals->count == 0) {
    /* Set the parent environment */
    f->env->parent = e;

    /*Evaluate the body */
    return builtin_eval(f->env,
      lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    /* Otherwise return partially evaluated function */
    return lval_copy(f);
  }
}

int main(int argc, char** argv) {
  Comment = mpc_new("comment");
  Expr = mpc_new("expr");
  Lispy = mpc_new("lispy");
  Number = mpc_new("number");
  Qexpr  = mpc_new("qexpr");
  Sexpr  = mpc_new("sexpr");
  String  = mpc_new("string");
  Symbol = mpc_new("symbol");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                 \
      number  : /-?[0-9]+/ ;                          \
      symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\\\|=<>!&]+/ ; \
      string  : /\"(\\\\.|[^\"])*\"/ ;                \
      comment : /;[^\\r\\n]*/ ;                       \
      sexpr   : '(' <expr>* ')' ;                     \
      qexpr   : '{' <expr>* '}' ;                     \
      expr    : <number>  | <symbol> | <string>       \
              | <comment> | <sexpr>  | <qexpr>;       \
      lispy   : /^/ <expr>* /$/ ;                     \
    ",
    Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
  /* Print version and exit information */
  puts("Lispy Version 0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  /* Supplied with a list of files */
  if (argc >= 2) {

    /* loop over each supplied filename (starting from 1) */
    for (int i = 1; i < argc; i++) {

      /* Argument list with a single argument, the filename */
      lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));

      /* Pass to builtin load and get the result */
      lval* x = builtin_load(e, args);

      /* If the result is an error be sure to print it */
      if (x->type == LVAL_ERR) { lval_println(e, x); }
      lval_del(x);
    }
  }

  /* in a never ending loop */
  while(1) {
    /* Output our prompt */
    char* input = readline("lispy> ");

    /* Add input to history */
    add_history(input);

    /* Attempt to Parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval* x = lval_eval(e, lval_read(r.output));
      lval_println(e, x);
      lval_del(x);

      mpc_ast_delete(r.output);
    } else {
      /* Otherwise print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* Free retrieved input */
    free(input);
  }

  lenv_del(e);

  /* Undefine and Delete our Parsers */
  mpc_cleanup(4, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
