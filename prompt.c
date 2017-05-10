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

long eval_op(long x, char* op, long y){
    if(strcmp(op, "+") == 0)
        return x+y;
    if(strcmp(op, "-") == 0)
        return x-y;
    if(strcmp(op, "*") == 0)
        return x*y;
    if(strcmp(op, "/") == 0)
        return x/y;
    return 0;
}


long eval(mpc_ast_t* t){
    //if its a number return it
    if (strstr(t->tag, "number")){
        return atoi(t->contents);
    }

    //operator is second child
    char* op = t->children[1]->contents;

    long x = eval(t->children[2]);

    int i = 3;
    while(strstr(t->children[i]->tag, "expr")){
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}


int main(int argc, char *argv[])
{
    /*mpc_parser_t* Adjective = mpc_new("adjective");
    mpc_parser_t* Noun = mpc_new("noun");
    mpc_parser_t* Phrase = mpc_new("phrase");
    mpc_parser_t* Doge = mpc_new("doge");*/

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");
    

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                       \
            number  : /-?[0-9]+/ ;                  \
            operator: '+' | '-' | '*' | '/';        \
            expr    : <number> | '(' <operator> <expr>+ ')';\
            lispy   : /^/ <operator> <expr>+ /$/ ;  \
            ",            
            Number, Operator, Expr, Lispy);
    

    /*mpca_lang(MPCA_LANG_DEFAULT,
            "                                       \
            adjective: \"wow\" | \"many\"           \
                     | \"so\" | \"such\";           \
            noun     : \"lisp\" | \"language\"      \
                     | \"book\" | \"build\" | \"c\";\
            phrase   : <adjective> <noun>;          \
            doge     : <phrase>*;                   \
            ",                                       
            Adjective, Noun, Phrase, Doge);

    mpc_cleanup(4, Adjective, Noun, Phrase, Doge);*/
    
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit \n");

    while(1){


        //create prompt
        char* input = readline("> ");
        //read input max size 2048
        add_history(input);
        
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)){
            long result = eval(r.output);
            printf("%li\n", result);
                       
            mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
            //load AST from output
/*
            mpc_ast_t* a = r.output;
            printf("Tag: %s\n", a->tag);
            printf("Contents: %s\n", a->contents);
            printf("Number of children: %i\n", a->children_num);

            //get first child
            mpc_ast_t* c0 = a->children;
            printf("First Child Tag: %s\n", c0->tag);
            printf("First Child Contents %s\n", c0->contents);
            printf("First Child Number of children: %i\n", c0->children_num);
*/
        }
        else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        
        //print input back
        //printf("%s \n", input);

        free(input);
    }
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    
    return 0;
}
