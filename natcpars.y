%{
/*************************************************************************
 *
 * natcpars.y - the SUPERNAT variable database manipulation program.
 *              For Supernat V3
 *
 *   The language is case-insensitive, with all values being
 *   cast to upper case. Internal variables are given names
 *   starting with a lower case character to distinguish them
 *   from the ones that are input. 
 *
 *   The main data structures are
 *     - a symbol table, essentially a hash of the symbol names
 *     - the output heap, which consists of all the things that have
 *       have been recognised. Each entry in the output heap has
 *        - a symbol structures, consisting of
 *           - a name
 *           - a type (string, date, number/boolean, constant)
 *           - a value (always a string)
 *           - a confidence factor (always a number)
 *           names are either external or generated (for intermediate results)
 *
 *   Generation only takes place if the parser returns validly,
 *   and consists of
 *        - writing out any new symbols
 *        - if the actions are not sg.read_write, updating any lvalues
 *      When maintaining this program, always start from the yacc specification
 *      (.y) rather than the .c form.
 *
 *      The following sequence of shell commands does the necessary conversion.
 *
        yacc natcpars.y
#                       standard yacc parser generation
        sed < y.tab.c '/        printf/ s/      printf[         ]*(/ fprintf(sg.errout,/g' > y.tab.pc
#                       Change the yacc debugging code to write to the
#                       error log file rather than stdout.
        sed < y.tab.pc '/^# *line/d' > natcpars.c
#                       Remove the line directives that supposedly allow
#                       the source line debugger to place you in the
#                       absolute source file (i.e. the .y) rather than the
#                       .c. This feature (of pdbx) does not work properly;
#                       the debugger does recognise the line numbers, but is
#                       not able to recognise the source files, so it places
#                       you at random points in the .c.
#
 *
 */

#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
extern double atof();
extern double strtod();
extern long strtol();
extern double floor();
extern double fmod();
extern char * getenv();
extern char * to_char();    /* date manipulation */
extern int errno;
#ifdef NT4
#define _ptr _p
#define _base _bf._base
#endif
#ifdef LINUX
/*
 * The stdio is based on the C++ implementation; member names are different
 */
#define _ptr _IO_read_ptr
#define _base _IO_read_base
#endif
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1993\n";

#include  "supernat.h"
#ifdef DEBUG
/********************************************************************
 *
 * redirect yacc debugging output, in case linked with forms stuff
 *
 */
                 extern int yydebug;
                 static int call_count=0;
#endif

JOB * natcalc_cur_job;                         /* the current context */
extern int errno;
/**********************************************************************
 *
 * Static lexical analyser status stuff
 *
 */
static FILE * in_file;
static char * in_base;
static char * in_buf;

static int yylex();		/* Lexical Analyser */
%}

%union {
int ival;
struct s_expr * tval;
}

%token <tval> STRING NUMBER IDENT IS_DATE TO_DATE TO_CHAR SYSDATE TRUNC LENGTH
%token <tval> ERR_RET ERR_CLR UPPER LOWER INSTR GETENV GETPID PUTENV SUBSTR
%token <tval> SYSTEM HOLD RELEASE EXPRESS GET_STATUS GET_TIME GET_JOB_NO
%token <tval> RE_QUEUE
%token <ival> '(' ')' ';' '{' '}' FINISH
%type <tval> list expr statblock assnstat condstat
%right '='
%right '?' ':'
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left '<' '>' LE GE
%left LS RS
%left '+' '-'
%left '*' '/' '%'
%left UMINUS
%right '!' '~'
%start list


%{
static struct res_word {char * res_word;
    int tok_val;
} res_words[] = { {"IS_DATE", IS_DATE},
    {"TO_DATE",TO_DATE},
    {"GETPID",GETPID},
    {"PUTENV",PUTENV},
    {"ERR_RET",ERR_RET},
    {"ERR_CLR",ERR_CLR},
    {"TRUNC",TRUNC},
    {"SYSDATE",SYSDATE},
    {"LENGTH",LENGTH},
    {"INSTR",INSTR},
    {"SUBSTR",SUBSTR},
    {"AND",AND},
    {"OR",OR},
    {"UPPER",UPPER},
    {"LOWER",LOWER},
    {"GETENV",GETENV},
    {"SYSTEM",SYSTEM},
    {"TO_CHAR",TO_CHAR},
    {"GET_JOB_NO",GET_JOB_NO},
    {"HOLD",HOLD},
    {"RELEASE",RELEASE},
    {"EXPRESS",EXPRESS},
    {"GET_STATUS",GET_STATUS},
    {"GET_TIME",GET_TIME},
    {"RE_QUEUE",RE_QUEUE},
    {"END",FINISH},
    {NULL,0} } ;
#include "natcexe.h"
%}

%%

list : /* empty */
     {
        expr_anchor = newexpr("xROOT");
        $$ = expr_anchor;
     }
     | list ';'
     | error
     {
#ifdef DEBUG
(void) fprintf( sg.errout,"Parser Error\n");
#endif
      lex_first = LERROR;
      YYACCEPT;
     }
     | list condstat '{' statblock '}'
     {
        struct s_expr * next_expr;
        /* Loop - generate a conditional depending on condstat for
         * each of the assigns in statblock; they are chained together
         * with a null pointer indicating the last element
         */
        for (next_expr = $4;
                 next_expr != (struct s_expr *) NULL;
                     next_expr = next_expr->link_expr)
             next_expr->cond_expr = $2;
        $1->eval_expr = $4;
        $$ = $4;
     }
     | list condstat
     {
        $$ = newexpr(new_loc_sym());
        $$->cond_expr = $2;
        $1->eval_expr = $$;
     }
     | list statblock 
     {
         $1->eval_expr = $2;
         $$ = $2;
     }
     | list error
     {
#ifdef DEBUG
(void) fprintf( sg.errout,"Parser Error:\n");
#endif
     lex_first = LERROR;
     YYACCEPT; }
     ;

statblock : assnstat
     | statblock assnstat 
     {
        /* link the new statement to its predecessor in the block */
         $2->link_expr = $1;
         $$ = $2;
     }
     ;
condstat : expr 
     | expr ERR_RET '(' expr ')'
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $4;
         $$ = opsetup(ERR_RET,2,x);
     }
     | expr ERR_CLR '(' expr ')'
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $4;
         $$ = opsetup(ERR_CLR,2,x);
     }
     ;
assnstat : IDENT '=' expr 
     {
         struct s_expr * x[2];
         TOK * x1;
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '=',2,x);
         for (x1 = $1->out_sym->sym_value.next_tok;
                 x1 != (TOK *) NULL;
                     x1 = x1->next_tok);    /* Find the end of the chain */
         x1 =  &($$->out_sym->sym_value);   /* Associate the IDENT with this
                                               value */
         
     }
     ;

expr : '(' expr ')'
     {   $$ = $2; }
     | expr EQ expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(EQ,2,x);
     }
     | expr NE expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(NE,2,x);
     }
     | expr GE expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(GE,2,x);
     }
     | expr LE expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(LE,2,x);
     }
     | expr '<' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '<',2,x);
     }
     | expr '>' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '>',2,x);
     }
     | expr '+' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '+',2,x);
     }
     | expr '-' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '-',2,x);
     }
     | expr '*' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '*',2,x);
     }
     | expr '/' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '/',2,x);
     }
     | expr '&' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '&',2,x);
     }
     | expr '|' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '|',2,x);
     }
     | expr AND expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(AND,2,x);
     }
     | expr '?' expr ':' expr
     {
         struct s_expr * x[3];
         x[0] = $1;
         x[1] = $3;
         x[2] = $5;
         $$ = opsetup((int) '?',3,x);
     }
     | expr OR expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(OR, 2, x);
     }
     | expr '^' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '^',2,x);
     }
     | expr RS expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(RS,2,x);
     }
     | expr LS expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup(LS,2,x);
     }
     | expr '%' expr
     {
         struct s_expr * x[2];
         x[0] = $1;
         x[1] = $3;
         $$ = opsetup((int) '%',2,x);
     }
     | '-' expr %prec UMINUS
     {
         $$ = opsetup(UMINUS, 1, &($2));
     }
     | '!' expr 
     {
         $$ = opsetup((int) '!', 1, &($2));
     }
     | '~' expr 
     {
         $$ = opsetup((int) '~', 1, &($2));
     }
     | IS_DATE '(' expr ';' expr ')'
     {
         struct s_expr * x[2];
         x[0] = $3;
         x[1] = $5;
         $$ = opsetup(IS_DATE,2,x);
     }
     | TO_DATE '(' expr ';' expr ')'
     {
         struct s_expr * x[2];
         x[0] = $3;
         x[1] = $5;
         $$ = opsetup(TO_DATE,2,x);
     }
       /* Get the date for this format */
     | TO_CHAR '(' expr ';' expr ')'
     {
         struct s_expr * x[2];
         x[0] = $3;
         x[1] = $5;
         $$ = opsetup(TO_CHAR,2,x);
     }
     | TRUNC '(' expr ';' expr ')'
     {
         struct s_expr * x[2];
         x[0] = $3;
         x[1] = $5;
         $$ = opsetup(TRUNC,2,x);
     }
     | TRUNC '(' expr  ')'
     {
         $$ = opsetup(TRUNC, 1, &($3));
     }
     | INSTR '(' expr ';' expr ')'
     {
         struct s_expr * x[2];
         x[0] = $3;
         x[1] = $5;
         $$ = opsetup(INSTR,2,x);
     }
     | SUBSTR '(' expr ';' expr ';' expr ')'
     {
         struct s_expr * x[3];
         x[0] = $3;
         x[1] = $5;
         x[2] = $7;
         $$ = opsetup(SUBSTR,3,x);
     }
     | GETENV '(' expr ')'
     {
         $$ = opsetup(GETENV, 1, &($3));
     }
     | PUTENV '(' expr ')'
     {
         $$ = opsetup(PUTENV, 1, &($3));
     }
     | GETPID
     {
         $$ = opsetup(GETPID, 0, (struct s_expr **) NULL);
     }
     | SYSTEM '(' expr ')'
     {
         $$ = opsetup(SYSTEM, 1, &($3));
     }
     | UPPER '(' expr ')'
     {
         $$ = opsetup(UPPER, 1, &($3));
     }
     | LOWER '(' expr ')'
     {
         $$ = opsetup(LOWER, 1, &($3));
     }
     | LENGTH '(' expr ')'
     {
         $$ = opsetup(LENGTH, 1, &($3));
     }
     | GET_STATUS '(' expr ')'
     {
         $$ = opsetup(GET_STATUS, 1, &($3));
     }
     | GET_TIME '(' expr ')'
     {
         $$ = opsetup(GET_TIME, 1, &($3));
     }
     | HOLD '(' expr ')'
     {
         $$ = opsetup(HOLD, 1, &($3));
     }
     | RE_QUEUE '(' expr ')'
     {
         $$ = opsetup(RE_QUEUE, 1, &($3));
     }
     | RELEASE '(' expr ')'
     {
         $$ = opsetup(RELEASE, 1, &($3));
     }
     | EXPRESS '(' expr ')'
     {
         $$ = opsetup(EXPRESS, 1, &($3));
     }
     | GET_JOB_NO '(' expr ';' expr ';' expr ';' expr ';' expr ')'
     {
         struct s_expr * x[5];
         x[0] = $3;
         x[1] = $5;
         x[2] = $7;
         x[3] = $9;
         x[4] = $11;
         $$ = opsetup(GET_JOB_NO,5,x);
     }
     | GET_JOB_NO '(' ')'
     {
         $$ = opsetup(GET_JOB_NO, 0, (struct s_expr **) NULL);
     }
     | SYSDATE
     | STRING
     | IDENT
     | NUMBER
     ;
%%
/* *************************************************************************

natcalc is called with the following syntax for the parameter string p:

natcalc <sequence of terms according to the yacc grammar above >

The terms are either assignments, or conditions. An assignment is an expression
of the form IDENT = expression. A condition is any other expression.

Everything has both a numeric (and boolean) value, and a string value.

Everything is categorised as a number, a string or a date.

The program attempts to preserve the date attributes, and numeric
attributes, as far as possible. However, its response to possible errors
is to change the type to something that makes sense. So, if two
alpha strings are multiplied, together, the boolean values will be
used, and zero will result. 

Strings have the value false, for boolean purposes.

The '+' sign serves as a concatenation operator when the arguments are
strings; note that two numeric strings cannot be concatenated, except
perhaps by '01234' + '' +'5678'.

Strings cannot have embedded delimiters, but concatenation and alternative
delimiters (\'"`) provide work-rounds.

All of the 'C' operators are supported, with the following exceptions;

  - (post|pre)-(in|dec)rement
  - the '+=' type assignments
  - the = assignment only works as the extreme left hand expression
  - all expressions are evaluated when considering ? :, &&, || etc.

Assignments and conditions are separated by semi-colons.

natcalc returns either when all the text has been parsed and
evaluated, or when a condition has evaluated false.

A condition can be associated with an error message, which will be
displayed in preference to the trigger fail step, should it evaluate
to false, thus:

 condition ERR_MSG ( string expression )

This provides a means for putting out dynamic messages, if required; supply
a condition of the form  0 == 1, and a string expression that includes
variable values.

natcalc will return 0 if a condition evaluates false,
otherwise it will return 1.

Multiple disjointed conditions act as if linked by && operators; they
have an efficiency advantage over multiple && operators, since the
evaluation ceases if any evaluates false. Ordinarily, all tests are
evaluated.

It is not useful to input assignments and a condition, unless the
assignments are only to be actioned if the condition is true (since the
assignments are thrown away if the user exit returns IAPFAIL).

SYSDATE is recognised as the UNIX system date and time (format 'DD-MMM-YY').

Function IS_DATE (string,format_string) tests the string is a valid date.

Function TO_DATE (string,format_string) converts a valid date string to a
data item of type date (from the point of view of this routine).

Function TO_CHAR (date,format_string) converts a numeric value into a
date string.

Date arithmetic is supported; you can add or subtract days to or from
a date.

Subtraction will give you the days between two dates.

Function TRUNC (number) (truncate to integer) (or TRUNC (number,number)
(truncate to so many decimal places)) is analogous to the
SQL TRUNC operator, but does not support the Date/Time truncation
(simply because the date formatting does not support the time; easily
added). 

Function LENGTH(string) returns the length of a string, just like the
SQL LENGTH() function.

Function UPPER(string) converts any lower case characters to upper case
in the string, just like the SQL UPPER() function.

Function LOWER(string) converts any upper case characters to lower case
in the string, just like the SQL LOWER() function.

Function SUBSTR(string,number,number) reproduces the behaviour of the
SQL SUBSTR() function (including its behaviour with funny numbers).

Function INSTR(string,string) reproduces the behaviour of the
SQL INSTR() function.

Function GETENV(expr) returns the value of the UNIX environment variable.
It returns a zero length string if the UNIX getenv() call fails.

Function SYSTEM(expr) returns the exit value of the UNIX command executed.
This function provides an alternative to #HOST, appropriate if the command
is to be run asynchronously, and there is no wish to clear the screen.

*************************************************************************** */
int	 natcalc(fp,text,read_flag,job)
FILE       *fp;		     /* Input file */
char * text;                 /* Input buffer (if file is NULL) */
int read_flag;
JOB * job;
{
    int save_flag;
    int last_exp_val;
    struct s_expr * this_expr;
    struct res_word * xres;    /* for populating reserved words */
    struct sym_link * next_link;  /* for finding DB updates */
    in_file = fp;
    in_buf = text;
    in_base = text;
    save_flag = sg.read_write;
    sg.read_write = read_flag;     /* -1; check syntax
                                   0; do not carry out updates
                                   1; carry out updates
                                */
    natcalc_cur_job = job;             /* current context */
    if (natcalc_dh_con == (DH_CON *) NULL)
        natcalc_dh_con = sg.cur_db;
#ifdef DEBUG
#ifdef YYDEBUG
    yydebug=1;
#endif
    call_count++;
#endif

    lex_first = LGOING;
    expr_anchor = (struct s_expr *) NULL;
    upd_anchor = (struct sym_link *) NULL;
/*
 * Create the symbol table hash table
 */
    sg.sym_hash = hash(EXPTABSIZE,string_hh, strcmp);
/*
 * Populate it with the reserved words, so that these
 * are available to the lexical analyser
 */
    for (xres = res_words;
            xres->res_word != NULL;
                xres++)
    {
        struct symbol * xsym;
        xsym = newsym(xres->res_word,RTOK);           /* create a symbol    */
        xsym->sym_value.tok_conf = xres->tok_val;
    }

    if (!setjmp(env))
        (void) yyparse();
    if (lex_first == LMEMOUT)                  /* malloc error detected */
    {
        perror("malloc() failed");
        Lfree();			/* return Lmalloc()'ed space */
        cleanup(sg.sym_hash);
        sg.read_write = save_flag;
        return -1;
    }
    else if (lex_first == LERROR)                  /* syntax error detected */
    {
        (void) fprintf( sg.errout,"Parser Error: %.*s\n",
                           (in_file == (FILE *) NULL) ?
                      (in_buf - in_base) : (in_file->_ptr - in_file->_base),
                      ((char *)((in_file == (FILE *) NULL) ? (int) in_base :
                                 (int) in_file->_base)));
        Lfree();			/* return Lmalloc()'ed space */
        cleanup(sg.sym_hash);
        sg.read_write = save_flag;
        return -1;
    }
    else if (sg.read_write == -1)
    {       /* Syntax check only */
        Lfree();			/* return Lmalloc()'ed space */
        cleanup(sg.sym_hash);
        sg.read_write = save_flag;
        return 1;
    }
/*****************************************************************************
 * Execute the parsed code; if it returns success,
 * apply the updates to the database.
 */

    for (this_expr = expr_anchor,
         last_exp_val = 0;
             this_expr != (struct s_expr *) NULL
          && ((last_exp_val & FINISHED) != FINISHED);
                 this_expr = this_expr->eval_expr)
        last_exp_val = instantiate( this_expr);
    if (read_flag && ( EVALTRUE & last_exp_val))
    {
#ifdef DEBUG
    fprintf(sg.errout,"Trigger returned true and not read_only\n");
    (void) fflush(sg.errout);
#endif
    for (next_link = upd_anchor;
             next_link != (struct sym_link *) NULL;
                 next_link = next_link->next_link)
    {
        long hoffset;
        union hash_entry hash_entry;
        struct symbol * x_sym;
        x_sym = next_link->sym;
#ifdef DEBUG
        (void) fprintf(sg.errout,"name_len %d, name_ptr %d, type %d\n",
        x_sym->name_len,
        (int) x_sym->name_ptr,
        (int) x_sym->sym_value.tok_type);
        (void) fprintf(sg.errout,"tok_len %d, tok_ptr %d, write_flag %d\n",
        (int) x_sym->sym_value.tok_len,
        (int) x_sym->sym_value.tok_ptr,
        (int) x_sym->sym_value.tok_conf);
        (void) fprintf(sg.errout,"================\n");
        (void) fflush(sg.errout);
#endif
/*
  * At this point, we update the value database
  */
        for ( hash_entry.hi.in_use = 0,
              hoffset = fhlookup(natcalc_dh_con,x_sym->name_ptr,
                              x_sym->name_len, &hash_entry);
                  hoffset > 0;
                      hoffset = fhlookup(natcalc_dh_con,x_sym->name_ptr,
                              x_sym->name_len, &hash_entry))
        {    /* The symbol is already known; update its value */
            int rt;
            int rl;
            struct symbol *x2;
            (void) fh_get_to_mem(natcalc_dh_con,hash_entry.hi.body,
                      &rt,(char **) &x2,&rl);
            (void) free(x2);
                           /* throw away the current values */
            if (rt == VAR_TYPE)
            {
                struct symbol * x;
                char * x1;
                x = (struct symbol *) malloc( sizeof(struct symbol) + 
                                              x_sym->name_len +
                                     x_sym->sym_value.tok_len + 2);
                *x = *x_sym; 
                x1 = (char *) (x + 1);
                memcpy(x1,x->name_ptr,x->name_len);
                *(x1 + x->name_len) = '\0';
                x1 = x1 + x->name_len + 1;
                memcpy(x1,x->sym_value.tok_ptr,x->sym_value.tok_len);
                *(x1 + x->sym_value.tok_len) = '\0';
                fh_chg_ref(natcalc_dh_con,hash_entry.hi.body, -1);
                hash_entry.hi.body =  fhmalloc(natcalc_dh_con,
                         sizeof(struct symbol) + 
                         x_sym->name_len +
                         x_sym->sym_value.tok_len + 2 +
                                            sizeof(int) + sizeof(int));
                (void) fh_put_from_mem(natcalc_dh_con,VAR_TYPE,
                                  x,
                          sizeof(struct symbol) + x_sym->name_len +
                          x_sym->sym_value.tok_len + 2);
                (void) fseek(natcalc_dh_con->fp,hoffset,0);
                (void) fwrite((char *) &hash_entry,sizeof(hash_entry),
                               1,natcalc_dh_con->fp);
                (void) fflush(natcalc_dh_con->fp);
#ifdef DEBUG
                fprintf(sg.errout,"Variable Update - Index at %u Data at %u\n\
    Name: %s Value %s\n",
                        hoffset,hash_entry.hi.body,
                        x->name_ptr,x->sym_value.tok_ptr);
                (void) fflush(sg.errout);
#endif
                (void) free(x);
                break;
            }
        }
        if (hoffset == 0)
        {            /* New value to insert */
            struct symbol * x;
            char * x1;
            x = (struct symbol *) malloc( sizeof(struct symbol) + 
                                          x_sym->name_len +
                                 x_sym->sym_value.tok_len + 2);
            *x = *x_sym; 
            x1 = (char *) (x + 1);
            memcpy(x1,x->name_ptr,x->name_len);
            *(x1 + x->name_len) = '\0';
            x1 = x1 + x->name_len + 1;
            memcpy(x1,x->sym_value.tok_ptr,x->sym_value.tok_len);
            *(x1 + x->sym_value.tok_len) = '\0';
            (void) fhinsert(natcalc_dh_con,x_sym->name_ptr,
                              x_sym->name_len, VAR_TYPE, 
                              (char *) x, 
                sizeof(struct symbol) + 
                x_sym->name_len +
                x_sym->sym_value.tok_len + 2);
#ifdef DEBUG
                fprintf(sg.errout,"Variable Insert - Name: %s Value %s\n",
                        x->name_ptr,x->sym_value.tok_ptr);
                (void) fflush(sg.errout);
#endif
           (void) free(x);
        }
    }
    }
#ifdef DEBUG
    else
    {
        fprintf(sg.errout,"Trigger returned false or read_only; no updates\n");
        (void) fflush(sg.errout);
    }
#endif
    Lfree();			/* return Lmalloc()'ed space */
    cleanup(sg.sym_hash);
    sg.read_write = save_flag;
    if (last_exp_val & EVALTRUE)
        return 1;  /* condition true or absent */
    else
        return 0;                    /* false */
}
/************************************************************************
 *  Lexical Analyser
 */

#ifdef AIX
#ifndef LINUX
#undef EOF
#define EOF 255
#endif
#endif
#ifdef OSXDC
#undef EOF
#define EOF 255
#endif
static int yylex()
{
/**************************************************************************
 * Lexical analysis of natcalc rules 
 */
   enum comstate {CODE,COMMENT};
   enum comstate comstate;
   char parm_buf[4096];             /* maximum size of single lexical element */
   char * parm = parm_buf;
   for ( *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file),
          comstate = (*parm == '#') ? COMMENT : CODE;
              *parm != EOF && ((comstate == CODE &&
                ((*parm <= ' '&& *parm > '\0') || *parm > (char) 126))
              ||   (comstate == COMMENT && *parm != '\0'));
                  *parm= (in_file == (FILE *) NULL)? *in_buf++:fgetc(in_file),
                 comstate = (comstate == CODE) ?
                      ((*parm == '#') ? COMMENT : CODE) :
                      ((*parm == '\n') ? CODE : COMMENT));
                /* skip over white space and comments */
   while(*parm != EOF && *parm != '\0')
   switch (*parm)
   {                               /* look at the character */
   case ';':
   case ',':
             yylval.ival = ';';	
             return ';';      /* return the operator */
   case '{':

             yylval.ival = '{';	
             return '{';      /* return the operator */
   case '[':
   case '(':
             yylval.ival = '(';	
             return '(';
   case '}':
             yylval.ival = '}';	
             return '}';      /* return the operator */
   case ']':
   case ')':
             yylval.ival = ')';	
             return ')';
   case '+':
   case '-':
   case '*':
   case '/':
   case '%':
   case ':':
   case '?':
             yylval.ival = (int) *parm;	
             return *parm;      /* return the operator */
   case '!':
             parm++;
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
             if (*parm  != '=')   
             {
                if (in_file == (FILE *) NULL)
                    in_buf--;
                else
                    (void) ungetc(*parm, in_file);
                yylval.ival = '!';	
                return '!';      /* return the operator */ 
             }
                                 /* else */
             return NE;          /* return '!='         */
   case '|':
             parm++;
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
             if (*parm != '|')   
             {
                if (in_file == (FILE *) NULL)
                    in_buf--;
                else
                    (void) ungetc(*parm, in_file);
                return '|';      /* return the operator */ 
             }
                                 /* else */
             yylval.ival = OR;	
             return OR;          /* return '||'         */
   case '&':
             parm++;
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
             if (*parm != '&')   
             {
                if (in_file == (FILE *) NULL)
                    in_buf--;
                else
                    (void) ungetc(*parm, in_file);
                return '&';      /* return the operator */ 
             }
                                 /* else */
             yylval.ival = AND;	
             return AND;          /* return '&&'         */
   case '=':
             parm++;
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
             if ( *parm != '=')   
             {
                if (in_file == (FILE *) NULL)
                    in_buf--;
                else
                    (void) ungetc(*parm, in_file);
                return '=';      /* return the operator */ 
             }
                                 /* else */
             yylval.ival = EQ;	
             return EQ;          /* return '=='         */
   case '<':
             parm++;
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
             if (*parm != '=')   
             {
                if (in_file == (FILE *) NULL)
                    in_buf--;
                else
                    (void) ungetc(*parm, in_file);
                yylval.ival = '<';	
                return '<';      /* return the operator */ 
             }
             yylval.ival = LE;	
             return LE;          /* return '<='         */
   case '>':
             parm++;
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
             if (*parm != '=')   
             {
                if (in_file == (FILE *) NULL)
                    in_buf--;
                else
                    (void) ungetc(*parm, in_file);
                yylval.ival = '>';	
                return '>';      /* return the operator */ 
             }
             yylval.ival = GE;	
             return GE;          /* return '>='         */
   case '"':
   case '\\':
   case '`':
   case '\'':                         /* string; does not support
                                       * embedded quotes, but concatenation
                                       * and alternative delimiters provide
                                       * work-rounds
                                       */
             yylval.tval = newexpr(new_loc_sym());  /* allocate a symbol */
             parm++;
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
             if (*parm == EOF) return 0;
                       /* ignore empty non-terminated string */
             for (yylval.tval->out_sym->sym_value.tok_len = 0;
                      *parm != EOF && *parm++ != parm_buf[0];
                            *parm = (in_file == (FILE *) NULL)? *in_buf++:
                                                              fgetc(in_file),
                            yylval.tval->out_sym->sym_value.tok_len++);
                                      /* advance to end of string */
                 
                 yylval.tval->out_sym->sym_value.tok_ptr =
                      Lmalloc(yylval.tval->out_sym->sym_value.tok_len+1);
                 memcpy(yylval.tval->out_sym->sym_value.tok_ptr,parm_buf+1,
                            yylval.tval->out_sym->sym_value.tok_len);
                 *(yylval.tval->out_sym->sym_value.tok_ptr
                      + yylval.tval->out_sym->sym_value.tok_len) = '\0';
                 yylval.tval->out_sym->sym_value.tok_val =
                            strtod(yylval.tval->out_sym->sym_value.tok_ptr,
                                      (char **) NULL);
                 yylval.tval->out_sym->sym_value.tok_type = STOK;
#ifdef DEBUG
   (void) fprintf(sg.errout,"Found STRING %s\n",
                 yylval.tval->out_sym->sym_value.tok_ptr);
   (void) fflush(sg.errout);
#endif   
                 return STRING ;
    case '.':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':                         /* apparently a number */
    {
        char * x;                      /* for where strtod() gets to */
        yylval.tval = newexpr(new_loc_sym());    /* allocate a symbol */
/*
 * First pass; collect likely numeric characters
 */
        for ( parm++,
             *parm = (in_file == (FILE *) NULL)? *in_buf++: fgetc(in_file);
                  *parm == '.' || ( *parm >='0' && *parm <= '9');
                       parm++,
                       *parm = (in_file == (FILE *) NULL)? *in_buf++:
                                                           fgetc(in_file));
           if (in_file == (FILE *) NULL)
               in_buf--;
           else
               (void) ungetc(*parm,in_file);
        *parm = '\0';
/*
 * Now apply a numeric validation to it
 */
       yylval.tval->out_sym->sym_value.tok_val = strtod( parm_buf, &x);
                                      /* convert */
       yylval.tval->out_sym->sym_value.tok_len = x - parm_buf;
/*
 * Push back any over-shoot
 */
       while (parm > x)
       {
           parm--;
           if (in_file == (FILE *) NULL)
               in_buf--;
           else
               (void) ungetc(*parm,in_file);
       }
       *parm = '\0';
       yylval.tval->out_sym->sym_value.tok_ptr =
                      Lmalloc(yylval.tval->out_sym->sym_value.tok_len+1);
       memcpy(yylval.tval->out_sym->sym_value.tok_ptr,parm_buf,
                            yylval.tval->out_sym->sym_value.tok_len);
       *(yylval.tval->out_sym->sym_value.tok_ptr
              + yylval.tval->out_sym->sym_value.tok_len) = '\0';
       yylval.tval->out_sym->sym_value.tok_type = NTOK;
#ifdef DEBUG
   (void) fprintf(sg.errout,"Found NUMBER %s\n",
                         yylval.tval->out_sym->sym_value.tok_ptr);
   (void) fflush(sg.errout);
#endif   
       return NUMBER;
     }
     default:                        /* assume everything else is an identifier
                                      * or a reserved word
                                      */
         for (;
             (isalnum (*parm) || *parm == '_' || *parm == '$' || *parm == '.' );
                  *parm = islower(*parm) ? toupper(*parm) : *parm,
                  parm++,
                  *parm = (in_file == (FILE *) NULL)? *in_buf++:fgetc(in_file));
         if (in_file == (FILE *) NULL)
             in_buf--;
         else
             (void) ungetc(*parm,in_file);        /* skip this character */
         if (parm_buf != parm)
         {
             HIPT ient;
             HIPT * xent;
             *parm = '\0';
             ient.name=parm_buf;
             if ((xent = lookup(sg.sym_hash,ient.name)) == (HIPT *) NULL)
             {                                   /* New symbol */
                 yylval.tval = newexpr(parm_buf);
                 ient.name=yylval.tval->out_sym->name_ptr;
                 ient.body= (char *) yylval.tval->out_sym;
                 (void) insert(sg.sym_hash,ient.name,ient.body);
                 yylval.tval->fun_arg = IDENT;
                 yylval.tval->eval_func = findop(IDENT);
             }
             else                                /* Existing Symbol */
             {
                 if (((struct symbol *) xent->body)->sym_value.tok_type == RTOK)
                 {
                     if (((struct symbol *) xent->body)->sym_value.tok_conf == 
                           SYSDATE)
                     {
                         ient.name="xSYSDATE";
                         if ((xent = lookup(sg.sym_hash,ient.name))
                               == (HIPT *) NULL)
                         {
                             yylval.tval = newexpr("xSYSDATE");
                                                       /* allocate a symbol */
                             ient.name=yylval.tval->out_sym->name_ptr;
                             ient.body= (char *) yylval.tval->out_sym;
                             (void) insert(sg.sym_hash,ient.name,ient.body);
                             yylval.tval->out_sym->sym_value.tok_ptr
                                     = (char *) yylval.tval;
                             yylval.tval->fun_arg = SYSDATE;
                             yylval.tval->eval_func = findop(SYSDATE);
                         }
                         else
                             yylval.tval = (struct s_expr *)
                            ((struct symbol *) xent->body)->sym_value.tok_ptr;
#ifdef DEBUG
(void) fprintf(sg.errout,"SYSDATE: Output %.16g\n",
                yylval.tval->out_sym->sym_value.tok_val);
    (void) fflush(sg.errout);
#endif
                         return SYSDATE;
                     }
                     else yylval.ival =
                           ((struct symbol *) xent->body)->sym_value.tok_conf;
                     if (yylval.ival == FINISH)
                         return 0;
                     else 
                         return yylval.ival;
                }
                else
                    yylval.tval = (struct s_expr *)
                              ((struct symbol *) xent->body)->sym_value.tok_ptr;
             }
#ifdef DEBUG
   (void) fprintf(sg.errout,"Found IDENT %s\n",parm_buf);
   (void) fflush(sg.errout);
#endif   
             return IDENT;
        }
    } /* switch is repeated on next character if have not returned */
    if (*parm == EOF || *parm == '\0')
       return (0);                   /* exit if all done */	

    return ';'; /* ie statement terminator, nice and harmless */
}
#ifdef AIX
#undef EOF
#define EOF -1
#endif
#ifdef OSXDC
#undef EOF
#define EOF -1
#endif
int yyerror()
{
/* We do not have errors */
    return 0;
}
/*
 * commands come in on stdin
 */
int natcalc_main(argc,argv,envp)
int argc;
char **argv;
char ** envp;
{
    int c;
    int hash_size;
    int extent;
    char * db_name;
    char buf[BUFSIZ];
    int job_no;
    JOB * job;
    FILE * fp;
    int read_flag;
    hash_size = 8192;
    extent = 32768;
    read_flag = 0;
    job_no = 0;
    job = (JOB *) NULL;
    natinit(argc,argv);
    checkperm(U_READ|U_WRITE,(QUEUE *) NULL);
    db_name = sg.supernatdb;
    while ((c = getopt(argc, argv, "wch:e:n:j:")) != EOF)
    {
         switch(c)
         {
          case 'c' :              /* DB create */
              checkperm(U_READ|U_WRITE|U_EXEC,(QUEUE *) NULL);
              fhcreate(db_name,hash_size,extent,job_no);
              job_no = 0;
              break;
          case 'w' :              /* Not Read Only */
              read_flag = 1;
              break;
          case 'h' :              /* Hash Size */
              hash_size = atoi(optarg);
              if (hash_size == 0)
                  hash_size = 8192;
              break;
          case 'j' :              /* Job Number */
              job_no = atoi(optarg);
              break;
          case 'e' :              /* The amount to extend the DB */
              extent = atoi(optarg);
              if (extent == 0)
                  extent = 32768;
              break;
          case 'n' :              /* The name of the DB */
              db_name = optarg;
              break;
          default:
          case '?' : /* Default - invalid opt.*/
                  (void) fprintf(sg.errout,"Invalid argument,must be:\n\
-j job_number\n\
-n database to access\n\
-c (create)\n\
-w (update access)\n\
-h hash size\n\
-e extend size\n");
                  exit(1);
               break;
         }
    }
    if ((natcalc_dh_con = fhopen(db_name,string_hash)) == (DH_CON *) NULL)
        exit(1);
    if (job_no)
    {
        if ((job = findjobbynumber(&natcalc_queue_anchor,job_no))
                                   == (JOB *) NULL)
        {
#ifdef DEBUG
           (void) fprintf(sg.errout,"No such job: %d\n",job_no);
           (void) fflush(sg.errout);
#endif   
            exit(1);
        }
        else if (strcmp(job->job_owner,sg.username) &&
                 strcmp("root",sg.username))
        {
            job_destroy(job);
            free(job);
            job = (JOB *) NULL;   /* SYSTEM() can only be executed
                                     by the person in question */
        }
    }
    if (read_flag == 1)
    {                 /* Updates; must single thread */
        char * work_file;
        work_file = getnewfile(sg.supernat,"tmp");
        fp = openfile("/",work_file,"w");
        if (fp == (FILE *) NULL)
        {
           (void) fprintf(sg.errout, "Cannot create work file %s\n", work_file);
            exit(1);
        }
        while ((fgets(buf,sizeof(buf),stdin)) > (char *) 0)
            (void) fprintf(fp,"%s",buf);
        (void) fclose(fp);
/*
 * At this point, the data is in the work file
 */
        (void) sprintf(buf,"natcalc %s %s\n",work_file,sg.username);
 
        if (wakeup_natrun(buf))
            exit(0);           /* O.K., successfully submitted */

        (void) fhclose(natcalc_dh_con);
        sg.read_write = 1;
        if ((natcalc_dh_con = fhopen(db_name,string_hash)) == (DH_CON *) NULL)
        {
            (void) fprintf(sg.errout,"Cannot re-open the database");
            exit(1);
        }
        if ((fp = fopen(work_file,"r")) == (FILE *) NULL)
        {
            (void) fprintf(sg.errout,"Cannot re-open the work file");
            exit(1);
        }
        else
            setbuf(fp,buf);
    }
    else
        fp = stdin;
    c =  natcalc(fp, (char *) NULL,read_flag,job);
    (void) fhclose(natcalc_dh_con);
    if (c)
    {
        exit(0);
    }
    else
        exit(1);
}
/* End of natcpars.y */
