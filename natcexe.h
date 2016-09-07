/*
 * natcexe.h - Common definitions for natcpars.y and natcexe.c
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems Limited 1992
 */

/****************************
 * Variable Database information
 */
enum tok_type { NTOK, /* Number */
                STOK, /* String */
                CTOK, /* constant */
                ITOK, /* Identifier */
                DTOK, /* Date */
                RTOK, /* Reserved Word */
                UTOK  /* Unevaluated expression */
              };
typedef struct tok {
     enum tok_type tok_type;
     char * tok_ptr;
     double tok_val;
     int tok_len;
     int tok_conf;            /* token ID for reserved words,
                                 write_flag for variables,
                                 used flag for expressions
                               */
     struct tok * next_tok;   /* chain of values/back pointers */
} TOK;

#define UNUSED 0
#define UPDATE 1
#define INUSE 2
#define EVALTRUE 4
#define EVALFALSE 8
#define UNFINISHED 16
#define FINISHED 32
struct symbol
{
  int name_len;
  char * name_ptr;
  TOK sym_value;
};
struct expr_link {
   struct s_expr * s_expr;
   struct expr_link * next_link;
};
struct sym_link {
   struct symbol * sym;
   struct sym_link * next_link;
};
struct s_expr {
   struct symbol * out_sym;   /* The output from the evaluation */
   struct expr_link * in_link; /* Pointer to linked list of input
                                 symbols */
   int fun_arg;               /* Argument for evaluation function/
                                 expression operator */
   int (*eval_func)();        /* The evaluation function */
   struct s_expr * eval_expr; /* Pointer to the next expression in the
                               * block
                               */
   struct s_expr * link_expr; /* Pointer to the next expression in the
                               * block
                               */
   struct s_expr * cond_expr; /* Pointer to the conditional expression whose
                               * truth this depends on
                               */
};
char * new_loc_sym();         /* create a new local symbol name */
struct symbol * newsym();
struct sym_link * upd_anchor;
struct s_expr * expr_anchor, *newexpr(), * opsetup();
int (*findop())();
char * Lmalloc();
void Lfree();
#define NUMSIZE 23
                                /* space malloc()'ed to take doubles */
/**********************************************************************
 * Details to do with the database
 */
DH_CON * natcalc_dh_con;                     /* The database details */
QUEUE * natcalc_queue_anchor;

int instantiate();
struct symbol * findvar();
/***************************************************************************
 *  The symbol table and the output heap
 */
#define EXPTABSIZE 1000

/**********************************************************************
 *
 * Where the lexical analysis has got to
 *
 */

enum lex_state { LBEGIN, /* First time */
                LGOING, /* Currently processing */
                LERROR,  /*  Syntax error detected by yacc parser */
                LMEMOUT  /*  malloc() failure */
              }; 
enum lex_state lex_first;
 
jmp_buf env;
JOB * natcalc_cur_job;                         /* The current context */
