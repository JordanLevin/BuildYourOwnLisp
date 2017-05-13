#include <stdio.h>
#include "mpc.h"
#include <stdlib.h>


//do this stuff if on windows
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

//replace readline
char* readline(char* prompt){
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

//empty add_history function
void add_history(char* unused){}


#else
#include <editline/readline.h>
#include <editline/history.h>
#endif


//make an enum of lval types
enum {LVAL_NUM, LVAL_ERR, LVAL_FUN, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR};

//possible error types
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};


struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lenv {
    int count;
    char** syms;
    lval** vals;
};

struct lval {
    int type;
    long num;
    char* err;
    char* sym;
    lbuiltin fun;
    //count and pointer to a list of lval pointers
    int count;
    struct lval** cell;
};



//forward declare print functions
void lval_print(lval* v);
void lval_println(lval* v);
void lval_del(lval* v);
lval* lval_copy(lval* v);
lval* lval_err(char* a);
lval* lval_eval(lenv* e, lval* v);
lval* lval_eval_sexpr(lenv* e, lval* v);
lval* builtin_op(lenv* e, lval* a, char* op);
lval* builtin(lenv* e, lval* a, char* func);

//code for handling the variables (creation and deletion of lenv)
lenv* lenv_new(void){
    lenv* e = (lenv*) malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e){
    for(int i = 0;i < e->count; i++){
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k){
    //go through every item in the environment
    for(int i = 0;i< e->count; i++){
        //check if the stored string matches the symbol string and return a copy if it does
        if(strcmp(e->syms[i], k->sym) == 0){
            return lval_copy(e->vals[i]);
        }
    }
    //if no symbol found return error
    return lval_err("unbound symbol");
}

void lenv_put(lenv* e, lval* k, lval* v){
    //go through every item in the environment
    //check if the variable already exists
    for(int i = 0;i < e->count; i++){
        //if variable is found replace that item with the item supplied
        if (strcmp(e->syms[i], k->sym) == 0){
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    //no entry found, need to make a new entry
    e->count++;
    e->vals = (lval**) realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = (char**) realloc(e->syms, sizeof(char*) * e->count);

    //copy contents of lval and symbol string into new location
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = (char*) malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count-1], k->sym);
}

//Number lval constructor function
lval* lval_num(long x){
    lval* v = (lval*) malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

//Error lval constructor function
lval* lval_err(char* m){
    lval* v = (lval*) malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = (char*) malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

//Function lval constructor function
lval* lval_fun(lbuiltin func){
    lval* v = (lval*) malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

//Symbol lval constructor function
lval* lval_sym(char* s){
    lval* v = (lval*) malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = (char*) malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

//Sexpr lval constructor function
lval* lval_sexpr(void){
    lval* v = (lval*) malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

//Qexpr lval constructor function
lval* lval_qexpr(void){
    lval* v = (lval*) malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}


lval* lval_read_num(mpc_ast_t* t){
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    if(errno != ERANGE){
        return lval_num(x);
    }
    return lval_err("invalid number");
}

lval* lval_add(lval* v, lval* x){
    v->count++;
    v->cell = (lval**) realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval* lval_read(mpc_ast_t* t){
    //if its a symbol or a number return a conversion to that type
    if(strstr(t->tag, "number")){
        return lval_read_num(t);
    }
    if(strstr(t->tag, "symbol")){
        return lval_sym(t->contents);
    }

    //if root (>) or sexpy then make an empty list
    lval* x = NULL;
    if(strcmp(t->tag, ">") == 0){
        x = lval_sexpr();
    }
    if(strstr(t->tag, "sexpr")){
        x = lval_sexpr();
    }
    if(strstr(t->tag, "qexpr")){
        x = lval_qexpr();
    }

    //fill in the list with any valid expressions
    for(int i = 0;i < t->children_num;i++){
        if(strcmp(t->children[i]->contents, "(") == 0)
            continue;
        if(strcmp(t->children[i]->contents, ")") == 0)
            continue;
        if(strcmp(t->children[i]->contents, "}") == 0)
            continue;
        if(strcmp(t->children[i]->contents, "{") == 0)
            continue;
        if(strcmp(t->children[i]->tag, "regex") == 0)
            continue;
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}


//Destructor
void lval_del(lval* v){
    switch(v->type){
        case LVAL_NUM: 
            break;
        case LVAL_ERR: 
            free(v->err);
            break;
        case LVAL_FUN:
            break;
        case LVAL_SYM:
            free(v->sym);
            break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for(int i = 0;i<v->count;i++){
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}

void lval_expr_print(lval* v, char open, char close){
    putchar(open);
    for(int i = 0;i< v->count; i++){
        lval_print(v->cell[i]);

        if(i != (v->count-1)){
            putchar(' ');
        }
    }
    putchar(close);
}

//print an lval
void lval_print(lval* v){
    switch (v->type){
        case LVAL_NUM: 
            printf("%li", v->num);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_FUN:
            printf("<function>");
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }

}

//do the print and a newline after
void lval_println(lval* v){
    lval_print(v);
    putchar('\n');
}


int number_of_nodes(mpc_ast_t* t){
    if(t->children_num == 0)
        return 1;
    if(t->children_num >= 1){
        int total = 1;
        for (int i = 0; i < t->children_num; ++i) {
           total+=number_of_nodes(t->children[i]); 
        }
        return total;
    }
    return 0;
}


lval* lval_eval(lenv* e, lval* v){
    if(v->type == LVAL_SYM){
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    //evaluate Sexpressions
    if (v->type == LVAL_SEXPR)
        return lval_eval_sexpr(e, v);
    return v;
}


lval* lval_pop(lval* v, int i){
    lval* x = v->cell[i];

    //shift memory after the item at i over
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    v->count--;

    v->cell = (lval**) realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i){
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_join(lval* x, lval* y){

    //for each cell in y add it to x
    while(y->count){
        x = lval_add(x, lval_pop(y, 0));
    }

    //delete the empty y and return x
    lval_del(y);
    return x;
}


lval* lval_eval_sexpr(lenv* e, lval* v){
    //eval children
    for(int i = 0; i<v->count; i++){
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    //error checking
    for(int i = 0; i<v->count; i++){
        if (v->cell[i]->type == LVAL_ERR)
            return lval_take(v, i);
    }

    //check for empty expression
    if (v->count == 0)
        return v;

    //check for single expression
    if (v->count == 1)
        return lval_take(v, 0);

    //make sure first elment is a symbol
    lval* f = lval_pop(v, 0);
    if(f->type != LVAL_FUN){
        lval_del(v);
        lval_del(f);
        return lval_err("first element is not a function");
    }

    //call builtin with operator
    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}

lval* lval_copy(lval* v){
    lval* x = (lval*) malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type){
        //functions and numbers get directly copied
        case LVAL_FUN:
            x->fun = v->fun;
            break;
        case LVAL_NUM:
            x->num = v->num;
            break;

        //use malloc and strcpy for strings
        case LVAL_ERR:
            x->err = (char*) malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = (char*) malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        //for lists each sub expression needs to be copied
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = (lval**) malloc(sizeof(lval*) * x->count);
            for(int i = 0;i<x->count;i++){
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

lval* builtin_op(lenv* e, lval* a, char* op){

    for(int i=0;i<a->count; i++){
        if(a->cell[i]->type != LVAL_NUM){
            lval_del(a);
            return lval_err("Cannot operate on non-number");
        }
    }

    lval* x = lval_pop(a, 0);
 
    //negate if there are no arguments and a subtraction
    if((strcmp(op, "-") == 0) && a->count == 0){
        x->num = -x->num;
    }

    while(a->count > 0){
        lval* y = lval_pop(a, 0);

        if(strcmp(op, "+") == 0)
            x->num += y->num;
        if(strcmp(op, "-") == 0)
            x->num -= y->num;
        if(strcmp(op, "*") == 0)
            x->num *= y->num;
        if(strcmp(op, "/") == 0){
            if (y->num == 0){
                lval_del(x);
                lval_del(y);
                x = lval_err("Division By Zero");
                break;
            }
            x->num /= y->num;
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}


#define LASSERT(args, cond, err) \
    if (!(cond)) {lval_del(args); return lval_err(err); }


lval* builtin_head(lenv* e, lval* a){
    //check for errors
    LASSERT(a, a->count == 1, "Function 'head' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect type");
    LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}");
    
    //otherwise get the first argument
    lval* v = lval_take(a, 0);

    //delete all non head elements and return
    while(v->count > 1){
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lenv* e, lval* a){
    //check errors
    LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' passed incorrect type");
    LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}");
    
    //get first argument
    lval* v = lval_take(a, 0);

    //delete first element and return it
    lval_del(lval_pop(v, 0));
    return v;
    
}

lval* builtin_list(lenv* e, lval* a){
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a){
    LASSERT(a, a->count == 1, "Function 'eval' passed too many args");
    LASSERT(a, a->cell[0]->type==LVAL_QEXPR,"Function 'eval' passed bad type");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a){
    for(int i = 0;i < a->count; i++){
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed wrong type");
    }

    lval* x = lval_pop(a, 0);

    while(a->count){
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}


lval* builtin(lenv* e, lval* a, char* func){
    if(strcmp("list", func) == 0)
        return builtin_list(e, a);
    if(strcmp("head", func) == 0)
        return builtin_head(e, a);
    if(strcmp("tail", func) == 0)
        return builtin_tail(e, a);
    if(strcmp("join", func) == 0)
        return builtin_join(e, a);
    if(strcmp("eval", func) == 0)
        return builtin_eval(e, a);
    if(strstr("+-/*", func))
        return builtin_op(e, a, func);
    lval_del(a);
    return lval_err("Unknown Function");

}

lval* builtin_add(lenv* e, lval* a){
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a){
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a){
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a){
    return builtin_op(e, a, "/");
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func){
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e){
    //list functions
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    //math functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_join);

}


int main(int argc, char *argv[])
{
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Lispy = mpc_new("lispy");
    

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                   \
            number  : /-?[0-9]+/ ;                              \
            symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;         \
            sexpr   : '(' <expr>* ')' ;                         \
            qexpr   : '{' <expr>* '}' ;                         \
            expr    : <number> | <symbol> | <sexpr> | <qexpr>;  \
            lispy   : /^/ <expr>* /$/ ;                         \
            ", 
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit \n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);
    while(1){


        //create prompt
        char* input = readline("> ");
        //read input max size 2048
        add_history(input);
        
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)){
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            /*lval result = eval(r.output);
            lval_println(result);*/
            mpc_ast_delete(r.output);
        }
        else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    lenv_del(e);

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    
    return 0;
}
