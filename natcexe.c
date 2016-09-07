/*
 * natcexe.c - module that implements the Supernat Expert System Interpreter.
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1992";
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <errno.h>
#include <ctype.h>
extern double atof();
extern double strtod();
extern long strtol();
extern double floor();
extern double fmod();
extern char * getenv();
extern char * to_char();    /* date manipulation */
#include "supernat.h"
#include "natcalc.h"
#include "natcexe.h"
struct symbol * newsym();
struct s_expr * expr_anchor, *newexpr(), * opsetup();
/*
 * Functions implemented herein
 */
static int altern();
static int arithop();
static int assgn();
static int err_clr();
static int err_ret();
static int err_ret_clr();
static int get_job_no();
static int get_status();
static int get_time();
static int hold_release_express();
static int ident();
static int instr();
static int is_date();
static int length();
static int lgetenv();
static int lgetpid();
static int logop();
static int lputenv();
static int lsystem();
static int lto_char();
static int matchop();
static int negop();
static int re_queue();
static int substr();
static int sysdate();
static int to_date();
static int trunc();
static int updown();
/*
 * The functions that implement particular expressions
 * Some functions do more than one type.
 */
static struct eval_funs {
   int op;
   int (*eval_fun)();
} eval_funs [] = {
{IS_DATE, is_date},
{TO_DATE, to_date},
{TO_CHAR, lto_char},
{ IDENT, ident},
{ SYSDATE, sysdate},
{TRUNC, trunc},
{LENGTH, length},
{ERR_RET, err_ret},
{ERR_CLR, err_clr},
{UPPER, updown},
{LOWER, updown},
{INSTR, instr},
{GETENV, lgetenv},
{GETPID, lgetpid},
{PUTENV, lputenv},
{SUBSTR, substr},
{SYSTEM, lsystem},
{HOLD, hold_release_express},
{RELEASE, hold_release_express},
{EXPRESS, hold_release_express},
{GET_STATUS, get_status},
{GET_TIME, get_time},
{GET_JOB_NO, get_job_no},
{RE_QUEUE, re_queue},
{'=', assgn},
{'?', altern},
{OR, logop},
{AND, logop},
{'|', arithop},
{'^', arithop},
{'&', arithop},
{EQ, matchop},
{NE, matchop},
{'<',matchop},
{'>',matchop},
{LE,matchop},
{GE, matchop},
{LS,arithop},
{RS, arithop},
{'+',arithop},
{'-', arithop},
{'*',arithop},
{'/',arithop},
{'%', arithop},
{UMINUS, arithop},
{'!',arithop},
{'~', arithop},
{0,(int (*)()) NULL}
};
static struct expr_link * in_link();
static struct sym_link * upd_link();
static int locsym_count=0;     /* local symbol count */

/*******************************************************************
 * Routines that are called from elsewhere (principally, natcpars.y)
 */
char * new_loc_sym()         /* create a new local symbol name */
{
static char locsym_name[40];   /* local symbol name  */
    (void) sprintf(locsym_name,"a%ld", locsym_count++);
    return locsym_name;
}
/*******************************************************************
 * Allocate a new symbol
 *
 * Return a pointer to the symbol created.
 */
struct symbol * newsym(symname,tok_type)
char * symname;
enum tok_type tok_type;
{
    HIPT xent;
    struct symbol * xsym;
    if ((xsym = (struct symbol *)
                Lmalloc ( sizeof( struct symbol)))
                    != (struct symbol *) NULL)
    {                                 /* there is room for another */
        xsym->name_len = strlen(symname);       /* passed value */
        xsym->name_ptr = Lmalloc(xsym->name_len+1);
        strcpy(xsym->name_ptr, symname);
        xsym->sym_value.tok_type = tok_type;
        xsym->sym_value.tok_ptr= (char *) NULL;
        xsym->sym_value.next_tok= (TOK *) NULL;
        xsym->sym_value.tok_len= 0;
        xsym->sym_value.tok_conf = 0;
        xent.name = symname;
        xent.body = (char *) xsym;
        if (insert(sg.sym_hash,xent.name,xent.body) == (HIPT *) NULL)
        {
#ifdef DEBUG
            (void) fprintf(sg.errout,"Failed to add symbol %s to hash table\n",
                   symname);
#endif
            lex_first=LMEMOUT;
            longjmp(env,0);
        }                              /* Add it to the hash table */
    }
    else
    {  
#ifdef DEBUG
        (void) fprintf(sg.errout,"Too many symbols\n");
#endif    
        lex_first=LMEMOUT;
        longjmp(env,0);
    }
    return xsym;
}
/***************************************************************************
 * Allocate a new expression, returning a pointer to the the expression
 * created.
 */
struct s_expr * newexpr(symname)
char * symname;
{
    struct s_expr * new_expr;
    if (((new_expr = (struct s_expr *) Lmalloc ( sizeof( struct s_expr)))
                    != (struct s_expr *) NULL)
    && ((new_expr->out_sym = newsym(symname,UTOK))
                    != (struct symbol *) NULL))
    {                        /* there is room for another */

        new_expr->fun_arg = 0;
        new_expr->eval_func = (int (*)()) NULL;
        new_expr->eval_expr = (struct s_expr *) NULL;
        new_expr->link_expr = (struct s_expr *) NULL;
        new_expr->cond_expr = (struct s_expr *) NULL;
        new_expr->in_link = (struct expr_link *) NULL;
        new_expr->out_sym->sym_value.tok_ptr = (char *) new_expr;
                             /* Point the symbol back to the expression,
                                until it has a value */
    }
    else
    {  
#ifdef DEBUG
        (void) fprintf(sg.errout,"Too many expressions\n");
#endif    
        lex_first=LMEMOUT;
        longjmp(env,0);
    }
    return new_expr;
}
/*****************************************************************************
 * Chain together the input expressions
 */
static struct expr_link * in_link(in_expr)
struct s_expr * in_expr;
{
    struct expr_link * new_link;
    if ((new_link = (struct expr_link *) Lmalloc ( sizeof( struct expr_link)))
                    != (struct expr_link *) NULL)
    {                        /* there is room for another */

        new_link->s_expr = in_expr;
        new_link->next_link = (struct expr_link *) NULL;
    }
    else
    {  
#ifdef DEBUG
        (void) fprintf(sg.errout,"Too little memory\n");
#endif    
        lex_first=LMEMOUT;
        longjmp(env,0);
    }
    return new_link;
}
/**************************************************************************
 * Function to identify the function that processes a given expression type
 */
int (*findop(op))()
int op;
{
struct eval_funs * x;
    for (x = eval_funs;;x++)
       if (x->op == op || x->op == 0)
           return x->eval_fun;
}
/**************************************************************************
 * Function to link together expressions for processing
 */
struct s_expr * opsetup(op,in_cnt,in_expr)
int op;
int in_cnt;
struct s_expr ** in_expr;
{
    struct expr_link ** x_link;
    struct s_expr ** x_expr;
    struct s_expr * new_expr = newexpr(new_loc_sym());
                                          /* allocate a new expression */
    new_expr->fun_arg = op;
    new_expr->eval_func = findop(op);
    for (x_link = &new_expr->in_link,
         x_expr = in_expr;
             in_cnt;
                 in_cnt --,
                 x_link = &((*x_link)->next_link),
                 x_expr = x_expr + 1)
        *x_link = in_link ( *x_expr);
    return new_expr;            /* Stack a pointer to the new expression */
}


/*************************************************************************
 * All these functions process an expression node.
 * Points:
 * -  A lot of the processing is operator independent:
 *    -  Deciding whether the input expressions need to be evaluated
 *    -  Setting up the output values
 *    -  Returning yes or no.
 * -  However, with AND, OR and alternatives, do not necessarily need
 *    to process all the inputs.
 * -  So, the choice is between a small number of 'high level' functions,
 *    calling simple functions that just evaluate arguments, and a single
 *    'instantiate' function that is called from the simple functions to fill
 *    in values, and which itself calls simple functions.
 * -  The latter is closer in spirit to what we already have, so that is the
 *    way we shall go.
 *
 ****************************************************************************
 * This function is the main driver. It has the following tasks:
 * - Loop through all linked expressions
 *   For each:
 *    - Check whether the condition applies.
 *      -  Inspect the value for the conditional expression.
 *      -  If it has not yet been processed, call instantiate() to process it
 *      -  If FINISH is indicated, finish
 *    - If the condition does not apply, continue
 *      (ie. the rule is inapplicable)
 *    - If the condition does apply, look at the output symbol. 
 *      Follow the chain of values, looking for an undefined reference 
 *      with a back pointer to this value that is unused. 
 *    - If it exists, and it is unused, stamp it as used
 *    - Otherwise, search for an unused back-pointer
 *    - If found, call instantiate() for its parent expression
 *  - If nothing is found return EVALTRUE or EVALFALSE, depending on the
 *    truth value of the last value element encountered (with the UPDATE
 *    flag set, if any have this flag).
 *  - Otherwise, return the value of the last indicated function
 */
int instantiate ( s_expr )
struct s_expr * s_expr;
{
struct s_expr * l_expr;
TOK * this_tok;
int ret_value = EVALTRUE;  /* make sure that it has a value */
    for (l_expr = s_expr;
             l_expr != (struct s_expr *) NULL;
                 l_expr = l_expr->link_expr)
    {
#ifdef DEBUG
(void)    fprintf(sg.errout,"Instantiating %s, function %d\n",
                  l_expr->out_sym->name_ptr,
                  l_expr->fun_arg);
(void)    fflush(sg.errout);
#endif
        if (l_expr->cond_expr != (struct s_expr *) NULL)
        {
#ifdef DEBUG
(void)    fprintf(sg.errout,"Following a condition\n");
(void)    fflush(sg.errout);
#endif
            ret_value = instantiate(l_expr->cond_expr);
            if (ret_value & FINISHED)
                return ret_value;
            else
            if (ret_value & EVALFALSE)
                continue;
        }
/*
 *      Follow the chain of values, looking for an undefined reference 
 *      with a back pointer to this value that is unused. 
 */
        for (this_tok =  &(l_expr->out_sym->sym_value);
                 this_tok != (TOK *) NULL
              && this_tok->tok_ptr != (char *) l_expr
              && this_tok->tok_conf != UNUSED;
                     this_tok = this_tok->next_tok);
        if (this_tok != (TOK *) NULL)
        {
#ifdef DEBUG
(void)    fprintf(sg.errout,"Processing the function\n");
(void)    fflush(sg.errout);
#endif
            this_tok->tok_conf = INUSE; /* If it exists stamp it as used */
            if (l_expr->eval_func != (int (*)()) NULL)
                ret_value = (*l_expr->eval_func) ( l_expr );
            if (ret_value & FINISHED)
                return ret_value;
        }
        else
        {               /* Otherwise, search for any unused back-pointer */
            for (this_tok =  &(l_expr->out_sym->sym_value);
                     this_tok != (TOK *) NULL
                  && this_tok->tok_conf != UNUSED;
                         this_tok = this_tok->next_tok);
            if (this_tok != (TOK *) NULL)
            {
#ifdef DEBUG
(void)    fprintf(sg.errout,"Following a related reference\n");
(void)    fflush(sg.errout);
#endif
                ret_value = instantiate ( (struct s_expr *) this_tok->tok_ptr);
                if (ret_value & FINISHED)
                    return ret_value;
            }
        }
    }
#ifdef DEBUG
(void)    fprintf(sg.errout,"No more at this level, ret_value %d\n", ret_value);
(void)    fflush(sg.errout);
#endif
    return ret_value;
}
/*****************************************************************************
 * Add the variables that need to be updated to the chain.
 */
/*****************************************************************************
 * Chain together the input expressions
 */
static struct sym_link * upd_link(in_sym)
struct symbol * in_sym;
{
static struct sym_link * last_link;
struct sym_link * new_link;
    if ((new_link = (struct sym_link *) Lmalloc ( sizeof( struct sym_link)))
                    != (struct sym_link *) NULL)
    {                        /* there is room for another */
        new_link->sym = in_sym;
        new_link->next_link = (struct sym_link *) NULL;
        if (upd_anchor == (struct sym_link *) NULL)
            upd_anchor = new_link;
        else
            last_link->next_link = new_link;
        last_link = new_link;
    }
    else
    {  
#ifdef DEBUG
        (void) fprintf(sg.errout,"Too little memory\n");
#endif    
    }
    return new_link;
}
/****************************************************************************
 * Individual functions:
 * - These generally work as follows:
 *   -  Instantiate as many input variables as are necessary
 *   -  Process the input arguments
 *   -  Set the value of the output corresponding to this expression
 *   -  Return the truth value
 ***************************************************************************
 * Process the '?' ':' operation.
 */
static int altern(s_expr)
struct s_expr * s_expr;
{
    int ret_value;
    struct symbol * x;
    struct symbol * x1;
    struct s_expr * s;
    x = s_expr->out_sym;
    ret_value = instantiate(s_expr->in_link->s_expr);
    if (ret_value & EVALTRUE)
        s = s_expr->in_link->next_link->s_expr;
    else
        s = s_expr->in_link->next_link->next_link->s_expr;
    ret_value = instantiate(s);
    x1 = s->out_sym;
    x->sym_value.tok_ptr = x1->sym_value.tok_ptr ;
    x->sym_value.tok_type = x1->sym_value.tok_type ;
    x->sym_value.tok_val = x1->sym_value.tok_val ;
    x->sym_value.tok_len = x1->sym_value.tok_len ;
    return ret_value;
}
/****************************************************************************
 * Implement all the arithmetic operators, plus string concatenation
 */
static int arithop(s_expr)
struct s_expr * s_expr;
{
    struct symbol * x0, *x1, *x3;
    x0 = s_expr->out_sym;
    x1 = s_expr->in_link->s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    if (s_expr->in_link->next_link != (struct expr_link *) NULL)
    {
        (void) instantiate(s_expr->in_link->next_link->s_expr);
        x3 = s_expr->in_link->next_link->s_expr->out_sym;
    }
    if (s_expr->fun_arg == (int) '+'
       && ( (x1->sym_value.tok_type == STOK
           || x3->sym_value.tok_type == STOK)
           || (x1->sym_value.tok_type == DTOK
               && x3->sym_value.tok_type == DTOK)))
    {             /* String concatenation */
        x0->sym_value.tok_type = STOK;
        x0->sym_value.tok_len = x1->sym_value.tok_len+x3->sym_value.tok_len;
        x0->sym_value.tok_ptr =
               Lmalloc(x1->sym_value.tok_len+x3->sym_value.tok_len+1);
        memcpy(x0->sym_value.tok_ptr,
               x1->sym_value.tok_ptr,x1->sym_value.tok_len);
        memcpy(x0->sym_value.tok_ptr + x1->sym_value.tok_len,
                x3->sym_value.tok_ptr,x3->sym_value.tok_len);
        x0->sym_value.tok_val = (double) 0.0;
    }
    else /* Treat as arithmetic */
    {
        switch (s_expr->fun_arg)
        {
        case '+':
            if (x1->sym_value.tok_type == DTOK)
            {
                 x0->sym_value.tok_type = DTOK;
                 x0->sym_value.tok_val = x1->sym_value.tok_val +
                      x3->sym_value.tok_val*((double) 86400.0);
            }
            else if (x3->sym_value.tok_type == DTOK)
            {
                 x0->sym_value.tok_type = DTOK;
                 x0->sym_value.tok_val = x3->sym_value.tok_val +
                                    x1->sym_value.tok_val*((double) 86400.0);
            }
            else
            {
                 x0->sym_value.tok_type = NTOK;
                 x0->sym_value.tok_val = x3->sym_value.tok_val +
                                                x1->sym_value.tok_val;
            }
            break;
        case '-':
            if (x1->sym_value.tok_type == DTOK)
            {
                if (x3->sym_value.tok_type != DTOK)
                { 
                    x0->sym_value.tok_type = DTOK;
                    x0->sym_value.tok_val = x1->sym_value.tok_val -
                                    x3->sym_value.tok_val*((double) 86400.0);
                } 
                else
                {   
                    x0->sym_value.tok_type = NTOK;
                    x0->sym_value.tok_val = (x1->sym_value.tok_val -
                                    x3->sym_value.tok_val)/((double) 86400.0);
                } 
            } 
            else
            {
                x0->sym_value.tok_type = NTOK;
                x0->sym_value.tok_val = x1->sym_value.tok_val -
                                                x3->sym_value.tok_val;
            }
            break;
        case '*':
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val = x1->sym_value.tok_val *
                                                x3->sym_value.tok_val;
            break;
        case '/':
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val = x1->sym_value.tok_val /
                                                x3->sym_value.tok_val;
            break;
        case UMINUS:
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val = -x1->sym_value.tok_val;
            break;
        case '!':
            x0->sym_value.tok_type = NTOK;
            if (x1->sym_value.tok_val == (double) 0.0)
                x0->sym_value.tok_val = (double) 1.0;
            else
                x0->sym_value.tok_val = (double) 0.0;
            break;
        case '~':
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val = (double)( ~ ((int) x1->sym_value.tok_val));
            break;
        case '%':
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val = fmod(x1->sym_value.tok_val ,
                                                x3->sym_value.tok_val);
            break;
        case '&':
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val =  (double)((int) x1->sym_value.tok_val &
                              (int) x3->sym_value.tok_val);
 
            break;
        case '|':
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val =  (double)((int) x1->sym_value.tok_val |
                              (int) x3->sym_value.tok_val);
 
            break;
        case '^':
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val =  (double)((int) x1->sym_value.tok_val ^
                              (int) x3->sym_value.tok_val);
 
            break;
        case RS:
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val =  (double)((int) x1->sym_value.tok_val >>
                              (int) x3->sym_value.tok_val);
 
            break;
        case LS:
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val =  (double)((int) x1->sym_value.tok_val <<
                              (int) x3->sym_value.tok_val);
 
            break;
        }
        x0->sym_value.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
        if (x0->sym_value.tok_type == NTOK)
        {
            (void) sprintf(x0->sym_value.tok_ptr,"%.16g",
                                            x0->sym_value.tok_val);
            x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
        }
        else
        {    /* Date */
            strcpy (x0->sym_value.tok_ptr,
                    to_char("DD-MON-YYYY",x0->sym_value.tok_val));
            x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
        }
    }
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/****************************************************************************
 * An assignment of a value to an identifier.
 */
static int assgn(s_expr)
struct s_expr * s_expr;
{
    int ret_value;
    struct symbol * x;
    struct symbol * x1;
    (void) instantiate(s_expr->in_link->s_expr);
    ret_value = instantiate(s_expr->in_link->next_link->s_expr);
    x = s_expr->in_link->s_expr->out_sym;
    x1 = s_expr->in_link->next_link->s_expr->out_sym;
    if (sg.read_write == 1 && ((x->sym_value.tok_conf & UPDATE) == 0))
    {
        x->sym_value.tok_conf |= UPDATE;
        (void) upd_link(x);
    }
    x->sym_value.tok_ptr = x1->sym_value.tok_ptr ;
    x->sym_value.tok_type = x1->sym_value.tok_type ;
    x->sym_value.tok_val = x1->sym_value.tok_val ;
    x->sym_value.tok_len = x1->sym_value.tok_len ;
    x = s_expr->out_sym;
    x->sym_value.tok_ptr = x1->sym_value.tok_ptr ;
    x->sym_value.tok_type = x1->sym_value.tok_type ;
    x->sym_value.tok_val = x1->sym_value.tok_val ;
    x->sym_value.tok_len = x1->sym_value.tok_len ;
    return ret_value;
}
/****************************************************************************
 * If false terminate processing, output a message and return TRUE,
 * else continue
 */
static int err_clr(s_expr)
struct s_expr * s_expr;
{
    return err_ret_clr(s_expr,s_expr->fun_arg);
}
/****************************************************************************
 * If false, terminate processing, return FALSE, and output a message
 */
static int err_ret(s_expr)
struct s_expr * s_expr;
{
    return err_ret_clr(s_expr,s_expr->fun_arg);
}
/****************************************************************************
 * err_ret() and err_clr().
 *
 * If false, terminate processing, and output a message. If err_clr(),
 * return TRUE, else return FALSE.
 */
static int err_ret_clr(s_expr, fun_arg)
struct s_expr * s_expr;
int fun_arg;
{
    int ret_value;
    ret_value = instantiate(s_expr->in_link->s_expr);
    if (ret_value & EVALFALSE)
    {
        (void) instantiate(s_expr->in_link->next_link->s_expr);
        (void) fprintf(sg.errout,"%s\n",
           s_expr->in_link->next_link->s_expr->out_sym->sym_value.tok_ptr);
        if (fun_arg == ERR_CLR)
        {
            ret_value &= ~EVALFALSE;
            ret_value |= EVALTRUE;
        }
        ret_value |= FINISHED;
    }
    return ret_value;
}
/*****************************************************************************
 * Find the job number, given various attributes of the job
 * - x1 is the job name
 * - x2 is the job queue
 * - x3 is the job user
 * - x4 is the job number
 * - x5 is the occurrence
 */
static int get_job_no(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1, *x2, *x3, *x4, *x5;
    JOB_ACC j, *job_acc;
    int i11;
    struct expr_link * x_link;
    x0 = s_expr->out_sym;
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    x_link = s_expr->in_link;
    if ( x_link == (struct expr_link *) NULL)
    {
         if (natcalc_cur_job != (JOB *) NULL)
            x0->sym_value.tok_val = (double) natcalc_cur_job->job_no;
        else
            x0->sym_value.tok_val = 0.0;
    }
    else
    {
        (void) instantiate(x_link->s_expr);
        x1 = x_link->s_expr->out_sym;
        x_link = x_link->next_link;
        (void) instantiate(x_link->s_expr);
        x2 = x_link->s_expr->out_sym;
        x_link = x_link->next_link;
        (void) instantiate(x_link->s_expr);
        x3 = x_link->s_expr->out_sym;
        x_link = x_link->next_link;
        (void) instantiate(x_link->s_expr);
        x4 = x_link->s_expr->out_sym;
        x_link = x_link->next_link;
        (void) instantiate(x_link->s_expr);
        x5 = x_link->s_expr->out_sym;
        memset(&j,'\0',sizeof(j));
        (void) strncpy(j.job_name,x1->sym_value.tok_ptr,
                      sizeof(j.job_name) - 1);
        j.job_name[sizeof(j.job_name) - 1] = '\0';
        (void) strncpy(j.queue_name,x2->sym_value.tok_ptr,
                      sizeof(j.queue_name) - 1);
        j.queue_name[sizeof(j.queue_name) - 1] = '\0';
        (void) strncpy(j.user_name,x3->sym_value.tok_ptr,
                      sizeof(j.user_name) - 1);
        j.user_name[sizeof(j.user_name) - 1] = '\0';
        j.job_no = (int) x4->sym_value.tok_val;
        i11 = (int) x5->sym_value.tok_val;
        if (findjobacc_db(&j,i11,&job_acc))
            x0->sym_value.tok_val = (double) job_acc->job_no;
        else
            x0->sym_value.tok_val = 0.0;
    }
    (void) sprintf(x0->sym_value.tok_ptr,"%.16g", x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Return the status of a job
 */
static int get_status(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1;
    JOB * j;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    /* Get the status of a job, identified by the job number;
     * 0 means the current job
     */
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    if ( x1->sym_value.tok_val == 0.0 )
        j = natcalc_cur_job;
    else
        j = findjobbynumber(&natcalc_queue_anchor,(int)
                    x1->sym_value.tok_val);
    if (j != (JOB *) NULL)
        x0->sym_value.tok_val = (double) j->status;
    else
        x0->sym_value.tok_val = -1.0;
    (void) sprintf(x0->sym_value.tok_ptr,"%.16g", x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    if (j != natcalc_cur_job && j != (JOB *) NULL)
    {
        job_destroy(j);
        free(j);
    }
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Get the execution time of a job identified by the job number;
 * - 0 means the current job
 * - The time returned is LOCAL TIME, not GMT, although the value stored
 *   against the job is GMT.
 */
static int get_time(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1;
    JOB * j;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    if ( x1->sym_value.tok_val == 0.0 )
        j = natcalc_cur_job;
    else
        j = findjobbynumber(&natcalc_queue_anchor,(int)
                    x1->sym_value.tok_val);
    if (j != (JOB *) NULL)
    {
        x0->sym_value.tok_type = DTOK;
        x0->sym_value.tok_val = local_secs( j->job_execute_time);
        x0->sym_value.tok_len = 20;
        (void) strcpy (x0->sym_value.tok_ptr,
                      to_char("DD-MON-YYYY HH24:MI:SS",
                               x0->sym_value.tok_val));
    }
    else
    {
        x0->sym_value.tok_type = NTOK;
        x0->sym_value.tok_val = -1.0;
        x0->sym_value.tok_ptr= "-1.0";
        x0->sym_value.tok_len = 4;
    }
    if (j != natcalc_cur_job && j != (JOB *) NULL)
    {
        job_destroy(j);
        free(j);
    }
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Put a job on hold, release it or express it.
 */
static int hold_release_express(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1;
    JOB * j;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    if ( x1->sym_value.tok_val == 0.0 )
        j = natcalc_cur_job;
    else
        j = findjobbynumber(&natcalc_queue_anchor,(int)
                    x1->sym_value.tok_val);
    if (j != (JOB *) NULL)
    {
        if (s_expr->fun_arg == HOLD)
            j->status = JOB_HELD;
        else
        if (s_expr->fun_arg == RELEASE)
            j->status = JOB_RELEASED;
        else  /* Express */
        {
            JOB * job = job_clone(j);
            (void) free(j->express);
            j->express = strnsave("yes",3);
            x0->sym_value.tok_val = (double) renamejob(job,j);
            job_destroy(job);
            free(job);
        }
        if (sg.read_write == 1)
        {
#ifdef DEBUG
            (void) fprintf(sg.errout,
                   "Arg %d ing job %s No: %d\n",s_expr->fun_arg,
                                                j->job_name, j->job_no);
            (void) fflush(sg.errout);
#endif
            renamejob_db(j);
        }
        x0->sym_value.tok_val = 1.0;
    }
    else
        x0->sym_value.tok_val = 0.0;
    (void) sprintf(x0->sym_value.tok_ptr,"%.16g", x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    if (j != natcalc_cur_job && j != (JOB *) NULL)
    {
        job_destroy(j);
        free(j);
    }
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Get the value of a variable from the database. Only executes once.
 */
static int ident(s_expr)
struct s_expr * s_expr;
{
    union hash_entry hash_entry;    
    long hoffset;

    if (s_expr->out_sym->sym_value.tok_type != ITOK)
    {
        s_expr->out_sym->sym_value.tok_type = ITOK;
        s_expr->out_sym->sym_value.tok_val = 0.0;
        s_expr->out_sym->sym_value.tok_ptr = "";
        s_expr->out_sym->sym_value.tok_len = 0;
        s_expr->out_sym->sym_value.tok_conf = 0;
/*
 * Look to see if the symbol already exists in the database
 */
        for ( hash_entry.hi.in_use = 0,
              hoffset = fhlookup(natcalc_dh_con,s_expr->out_sym->name_ptr,
                             s_expr->out_sym->name_len, &hash_entry);
                  hoffset > 0;
                      hoffset = fhlookup(natcalc_dh_con,
                                         s_expr->out_sym->name_ptr,
                             s_expr->out_sym->name_len, &hash_entry))
        {    /* The symbol is already known; read its value */
            int rt;
            int rl;
            struct symbol *x2;  
            (void) fh_get_to_mem(natcalc_dh_con,hash_entry.hi.body,
                      &rt,(char **) &x2,&rl);
            if (rt == VAR_TYPE)
            {
                if (strncmp((char *) (x2 +1),
                            s_expr->out_sym->name_ptr,
                            s_expr->out_sym->name_len))
                {
                    fprintf(sg.errout,"Logic error: Variable name mismatch\n\
%s != %s\n", (char *) (x2 +1), s_expr->out_sym->name_ptr);
                }
                else
                {
                    s_expr->out_sym->sym_value = x2->sym_value;
                    s_expr->out_sym->sym_value.tok_conf = 0;
                    s_expr->out_sym->sym_value.tok_ptr =
                         Lmalloc(s_expr->out_sym->sym_value.tok_len + 1);
                    memcpy( s_expr->out_sym->sym_value.tok_ptr,
                            ((char *) (x2 + 1)) + x2->name_len + 1,
                            s_expr->out_sym->sym_value.tok_len+1);
#ifdef DEBUG
                    fprintf(sg.errout,"Read Variable - Name: %s Value %s\n",
                                 s_expr->out_sym->name_ptr,
                         ((char *) (x2 +1)) + x2->name_len + 1);
                    (void) fflush(sg.errout);
#endif
                }
                (void) free(x2);
                break;
            }
            else
                (void) free(x2);
        }
    }
    if (s_expr->out_sym->sym_value.tok_val == 0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * ORACLE-style INSTR()
 * Note that with NULL arguments it returns NULL, not a number.
 */
static int instr(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1, *x3;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    (void) instantiate(s_expr->in_link->next_link->s_expr);
    x3 = s_expr->in_link->next_link->s_expr->out_sym;
    if (x3->sym_value.tok_len == 0 || x1->sym_value.tok_len == 0)
    {
        x0->sym_value.tok_type = STOK;
        x0->sym_value.tok_val = (double) 0.0;
        x0->sym_value.tok_len = 0;
        x0->sym_value.tok_ptr = "";
    }
    else if (x3->sym_value.tok_len > x1->sym_value.tok_len)
    {
        x0->sym_value.tok_type = NTOK;
        x0->sym_value.tok_val = (double) 0.0;
        x0->sym_value.tok_len = 1;
        x0->sym_value.tok_ptr = "0";
    }
    else
    {
        char * x3_ptr = x1->sym_value.tok_ptr;
        register char * x5_ptr = x3->sym_value.tok_ptr;
        int i = x1->sym_value.tok_len - x3->sym_value.tok_len +1;
        register char * x1_ptr;
        register int j;

        for (;i; i--, x3_ptr++)
        {
            for (j = x3->sym_value.tok_len,
                 x1_ptr = x3_ptr,
                 x5_ptr = x3->sym_value.tok_ptr;
                    j > 0 && *x1_ptr++ == *x5_ptr++;
                      j--);
            if (j == 0) break;
        }
        x0->sym_value.tok_type = NTOK;
        if (i == 0)
        {
            x0->sym_value.tok_val = (double) 0.0;
            x0->sym_value.tok_len = 1;
            x0->sym_value.tok_ptr = "0";
        }
        else
        {
            x0->sym_value.tok_ptr = Lmalloc(NUMSIZE);
            x0->sym_value.tok_val = (double) (x1->sym_value.tok_len -
                                             x3->sym_value.tok_len +2 -i);
            (void) sprintf(x0->sym_value.tok_ptr,"%.16g",
                                            x0->sym_value.tok_val);
            x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
        }
    }
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Check the date is of the required format
 */
static int is_date(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1, *x3;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    (void) instantiate(s_expr->in_link->next_link->s_expr);
    x3 = s_expr->in_link->next_link->s_expr->out_sym;
    x= Lmalloc(x3->sym_value.tok_len+1);  /* format must be null
                                                   terminated */
    (void) memcpy (x,x3->sym_value.tok_ptr,x3->sym_value.tok_len);
    *(x + x3->sym_value.tok_len) = '\0';
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_len = 1;
    if ( date_val (x1->sym_value.tok_ptr,x,&x,&(x0->sym_value.tok_val)))
    {                                    /* returned valid date */
#ifdef DEBUG
(void) fprintf(sg.errout,"IS_DATE Valid: Input %*.*s,%*.*s\n",
        x1->sym_value.tok_len,x1->sym_value.tok_len,
                              x1->sym_value.tok_ptr, 
        x3->sym_value.tok_len,x3->sym_value.tok_len,
                              x3->sym_value.tok_ptr);
(void) fprintf(sg.errout,"             : Output %.16g\n",x0->sym_value.tok_val);
#endif
        x0->sym_value.tok_val = (double) 1.0;
        x0->sym_value.tok_ptr="1";
        return EVALTRUE;
    }
    else
    {
#ifdef DEBUG
(void) fprintf(sg.errout,"IS_DATE Failed: Input %*.*s,%*.*s\n",
        x1->sym_value.tok_len,x1->sym_value.tok_len,
                              x1->sym_value.tok_ptr, 
        x3->sym_value.tok_len,x3->sym_value.tok_len,
                              x3->sym_value.tok_ptr);
(void) fprintf(sg.errout,"              : Output %.16g\n",x0->sym_value.tok_val);
#endif
        x0->sym_value.tok_val = (double) 0.0;
        x0->sym_value.tok_ptr="0";
        return EVALFALSE;
    }
}
/*****************************************************************************
 * Return the length of a string
 */
static int length(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    (void) sprintf(x0->sym_value.tok_ptr,"%d", x1->sym_value.tok_len);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    x0->sym_value.tok_val = (double) x1->sym_value.tok_len;
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Extract a value from the environment
 */
static int lgetenv(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    x= Lmalloc(x1->sym_value.tok_len+1);  /* environment string must
                                            be null terminated */
    (void) memcpy (x,x1->sym_value.tok_ptr,x1->sym_value.tok_len);
    *(x + x1->sym_value.tok_len) = '\0';
    if ((x0->sym_value.tok_ptr = getenv(x)) == NULL)
    {
        x0->sym_value.tok_type = STOK;
        x0->sym_value.tok_ptr = "";
        x0->sym_value.tok_len = 0; 
        x0->sym_value.tok_val = (double) 0.0; 
    }
    else
    {
        char * strtodgot;
        x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
        x0->sym_value.tok_val = strtod( x0->sym_value.tok_ptr,&strtodgot);
                                /* attempt to convert to number */
#ifdef OLD_DYNIX
        if (strtodgot != x0->sym_value.tok_ptr) strtodgot--;
                                /* correct strtod() overshoot
                                   doesn't happen on Pyramid */
#endif
        if (strtodgot == x0->sym_value.tok_ptr + x0->sym_value.tok_len)
                                /* valid numeric */ 
            x0->sym_value.tok_type = NTOK;
        else
            x0->sym_value.tok_type = STOK;
    }
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/****************************************************************************
 * Get the pid
 */
static int lgetpid(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0;
    x0 = s_expr->out_sym;
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_val = (double) getpid();
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    (void) sprintf(x0->sym_value.tok_ptr,"%.16g", x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    return EVALTRUE;
}
/****************************************************************************
 * Implement AND and OR
 */
static int logop(s_expr)
struct s_expr * s_expr;
{
    int ret_value;
    struct symbol * x0;
    x0 = s_expr->out_sym;
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_len = 1;
    ret_value = instantiate(s_expr->in_link->s_expr);
    if (((ret_value & EVALFALSE) && s_expr->fun_arg == OR)
      || (ret_value & EVALTRUE) && s_expr->fun_arg == AND)
        ret_value = instantiate(s_expr->in_link->next_link->s_expr);
    if (ret_value & EVALTRUE)
    {
        x0->sym_value.tok_ptr = "1";
        x0->sym_value.tok_val = (double)1.0;
    }    
    else
    {
        x0->sym_value.tok_ptr = "0";
        x0->sym_value.tok_val = (double)0.0;
    }
    return ret_value;
}
/*****************************************************************************
 * Put something in the environment
 */
static int lputenv(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    x= (char *) malloc(x1->sym_value.tok_len+1);
/*
 * The environment string must be null terminated and should be static.
 * Call malloc() directly to avoid re-use.
 */
    (void) memcpy (x,x1->sym_value.tok_ptr,x1->sym_value.tok_len);
    *(x + x1->sym_value.tok_len) = '\0';
    x0->sym_value.tok_val = (double) putenv(x);
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    (void) sprintf(x0->sym_value.tok_ptr,"%.16g", x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Execute a sub-shell
 */
static int lsystem(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1, *x3;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
/*
 * Call a routine to exec a sub-process owned by the person in question,
 * and connected to the passed file descriptors
 * - For security, the routine can only be called when the
 *   job context is defined
 *
 */
    if (natcalc_cur_job == (JOB *) NULL)
        x0->sym_value.tok_val = 0.0;
    else
    {
#ifdef SCO
        static char *  pargv[]={{"sh"},{"-c"}, { NULL } , { NULL }};
#else
        static char *  pargv[]={"sh", "-c", NULL,  NULL };
#endif
        int wait_status;
        static char *  penvp= (char *) NULL;
        int rem_in, rem_out, rem_err, pid;
        pargv[2] = Lmalloc(x1->sym_value.tok_len+1);
                                         /* Command string must
                                            be null terminated */
        (void) memcpy (pargv[2],x1->sym_value.tok_ptr,
               x1->sym_value.tok_len);
        *(pargv[2] + x1->sym_value.tok_len) = '\0';
        pid = lcmd(&rem_in,&rem_out,&rem_err,
            natcalc_cur_job->job_owner,natcalc_cur_job->job_group,"/bin/sh",pargv,&penvp);
            /* spawn the sub-shell */
        if ( rem_in == -1)
            x0->sym_value.tok_val = 0.0;
        else
        {
            (void) close(rem_in);
            (void) close(rem_out);
            (void) close(rem_err);
            pid = waitpid(pid,&wait_status,0);
            if (wait_status)
                x0->sym_value.tok_val = 0.0;
            else
                x0->sym_value.tok_val = 1.0;
        }
    }
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    (void) sprintf(x0->sym_value.tok_ptr,"%.16g", x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Convert an internal date entity into a printable string
 */
static int lto_char(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1, *x3;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    (void) instantiate(s_expr->in_link->next_link->s_expr);
    x3 = s_expr->in_link->next_link->s_expr->out_sym;
    x= Lmalloc(x3->sym_value.tok_len+1);  /* format must be null
                                            terminated */
    (void) memcpy (x,x3->sym_value.tok_ptr,x3->sym_value.tok_len);
    *(x + x3->sym_value.tok_len) = '\0';
    x0->sym_value.tok_type = DTOK;
    x = to_char (x,x1->sym_value.tok_val);
    x0->sym_value.tok_len= strlen(x);
    x0->sym_value.tok_ptr= Lmalloc(x0->sym_value.tok_len+1);
    (void) memcpy(x0->sym_value.tok_ptr,x, x0->sym_value.tok_len);
    return EVALTRUE;
}
/****************************************************************************
 * Establish the relationship between two TOK's
 * Returns -1 if the first is less, 0 if equal, +1 if the first one is greater
 */
static int tok_comp( t1, t3)
struct symbol * t1;
struct symbol * t3;
{
  /*
   * Can we do a numeric comparison?
   */
    if ((t1->sym_value.tok_type == NTOK && t3->sym_value.tok_type == NTOK)
        || (t1->sym_value.tok_type == DTOK &&
                   t3->sym_value.tok_type == DTOK))
    {
        if (t1->sym_value.tok_val == t3->sym_value.tok_val)
            return 0;
        else
        if (t1->sym_value.tok_val < t3->sym_value.tok_val)
            return -1;
        else
            return 1;
    }
    else
/*
 * In the event of a type misamtch, compare as strings
 */
    {
        register int i = t1->sym_value.tok_len >= t3->sym_value.tok_len
                               ?  t3->sym_value.tok_len
                               :  t1->sym_value.tok_len;
        register char * x1_ptr = t1->sym_value.tok_ptr;
        register char * x2_ptr = t3->sym_value.tok_ptr;
        for (;i && (*x2_ptr == *x1_ptr);
                         i-- , x2_ptr++, x1_ptr++);
        if (i == 0)
        {
            if (t1->sym_value.tok_len == t3->sym_value.tok_len)
                return 0;
            else
            if (t1->sym_value.tok_len < t3->sym_value.tok_len)
                return -1;
            else
                return 1;
        }
        else
        if (*x1_ptr < *x2_ptr)
            return -1;
        else
            return 1;
    }
}
/****************************************************************************
 * Implement the comparison operators
 * EQ, <, > , NE, GE, LE
 */
static int matchop(s_expr)
struct s_expr * s_expr;
{
    int comp_value;
    struct symbol * x0, *x1, *x3;
    (void) instantiate(s_expr->in_link->s_expr);
    (void) instantiate(s_expr->in_link->next_link->s_expr);
    x0 = s_expr->out_sym;
    comp_value = tok_comp(s_expr->in_link->s_expr->out_sym,
                          s_expr->in_link->next_link->s_expr->out_sym);
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_len = 1;
    if ((comp_value == 0
         && ( s_expr->fun_arg == EQ
           || s_expr->fun_arg == GE
           || s_expr->fun_arg == LE))
    || (comp_value == -1
         && ( s_expr->fun_arg == NE
           || s_expr->fun_arg == (int) '<'
           || s_expr->fun_arg == LE))
    || (comp_value == 1
         && ( s_expr->fun_arg == NE
           || s_expr->fun_arg == (int) '>'
           || s_expr->fun_arg == GE)))
    {
        x0->sym_value.tok_ptr = "1";
        x0->sym_value.tok_val = (double)1.0;
    }    
    else
    {
        x0->sym_value.tok_ptr = "0";
        x0->sym_value.tok_val = (double)0.0;
    }    
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/***************************************************************************
 * Change the execution time to another date; use with repeating
 * jobs.
 *
 * The input time is LOCAL TIME. It needs to be converted to GMT.
 */
static int re_queue(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1;
    JOB * j;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    x0->sym_value.tok_type = NTOK;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    j = job_clone(natcalc_cur_job);
    natcalc_cur_job->job_execute_time = (time_t) gm_secs((time_t)
                                          floor(x1->sym_value.tok_val));
    if (sg.read_write == 1)
        x0->sym_value.tok_val = (double) renamejob(j,natcalc_cur_job);
    else
        x0->sym_value.tok_val = 1.0;
    job_destroy(j);
    free(j);
    (void) sprintf(x0->sym_value.tok_ptr,"%.16g", x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * ORACLE-style SUBSTR()
 */
static int substr(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1, *x2, *x3;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    (void) instantiate(s_expr->in_link->next_link->s_expr);
    x2 = s_expr->in_link->next_link->s_expr->out_sym;
    (void) instantiate(s_expr->in_link->next_link->next_link->s_expr);
    x3 = s_expr->in_link->next_link->next_link->s_expr->out_sym;
    x0->sym_value.tok_type = STOK;
    if ((x3->sym_value.tok_val <= (double) 0.0) ||
         (x1->sym_value.tok_len == 0) ||
         (((int) x2->sym_value.tok_val) > x1->sym_value.tok_len) ||
         ((x2->sym_value.tok_val < (double) 0.0) &&
         (-((int) x2->sym_value.tok_val) > x1->sym_value.tok_len)))
    {
        x0->sym_value.tok_ptr="";
        x0->sym_value.tok_val=(double) 0.0;
        x0->sym_value.tok_len=0;
    }
    else
    {
        register int i;
        register char * x2_ptr;
        register char * x1_ptr;
        char * strtodgot;
        if (x2->sym_value.tok_val == (double) 0.0)
        {
            i= x1->sym_value.tok_len;
            x1_ptr = x1->sym_value.tok_ptr;
        }
        else if (x2->sym_value.tok_val > (double) 0.0)
        {
            if (((int) (x2->sym_value.tok_val + 
                     x3->sym_value.tok_val -1.0) <= x1->sym_value.tok_len))
                i = x3->sym_value.tok_val;
            else
                i = 1 + x1->sym_value.tok_len -
                          ((int) x2->sym_value.tok_val);
            x1_ptr = x1->sym_value.tok_ptr + ((int) x2->sym_value.tok_val) -1;
        }
        else
        {
            if ((x0->sym_value.tok_len -
                          ((int) (x2->sym_value.tok_val) + (int)
                 (x3->sym_value.tok_val -1.0) <= x1->sym_value.tok_len)))
                i = x3->sym_value.tok_val;
            else
                i = -((int) x2->sym_value.tok_val);
            x1_ptr = x1->sym_value.tok_ptr + x1->sym_value.tok_len
                          + ((int) x2->sym_value.tok_val);
        }
          
        x2_ptr= Lmalloc(i+1);
        x0->sym_value.tok_ptr = x2_ptr;
        x0->sym_value.tok_len = i;
        while (i--)
           *x2_ptr++ = *x1_ptr++;
        *x2_ptr='\0';
        x0->sym_value.tok_val = strtod( x0->sym_value.tok_ptr,&strtodgot);
                                /* attempt to convert to number */
   
#ifdef OLD_DYNIX
        if (strtodgot != x0->sym_value.tok_ptr) strtodgot--;
                                /* correct strtod() overshoot
                                /* doesn't happen on Pyramid */
#endif
        if (strtodgot == x0->sym_value.tok_ptr + x0->sym_value.tok_len)
                                /* valid numeric */ 
            x0->sym_value.tok_type = NTOK;
        else
            x0->sym_value.tok_type = STOK;
    }
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Get the current date - only executed once per execution
 */
static int sysdate(s_expr)
struct s_expr * s_expr;
{
    if (s_expr->out_sym->sym_value.tok_type != DTOK)
    {
        s_expr->out_sym->sym_value.tok_type = DTOK;
        s_expr->out_sym->sym_value.tok_ptr = Lmalloc(NUMSIZE);
        s_expr->out_sym->sym_value.tok_len = 9;
        (void) date_val((char *) 0,"SYSDATE", (char *) 0,
                    &(s_expr->out_sym->sym_value.tok_val));
        (void) strcpy (s_expr->out_sym->sym_value.tok_ptr,
                     to_char("DD-MON-YY",s_expr->out_sym->sym_value.tok_val));
#ifdef DEBUG
(void) fprintf(sg.errout,"SYSDATE: Output %.16g\n",
                    s_expr->out_sym->sym_value.tok_val);
#endif
    }
    return EVALTRUE;
}
/****************************************************************************
 * Convert a date string to an internal date representation.
 */
static int to_date(s_expr)
struct s_expr * s_expr;
{
    char * x;
    struct symbol * x0, *x1, *x3;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    (void) instantiate(s_expr->in_link->next_link->s_expr);
    x3 = s_expr->in_link->next_link->s_expr->out_sym;
    x= Lmalloc(x3->sym_value.tok_len+1);  /* format must be null
                                            terminated */
    (void) memcpy (x,x3->sym_value.tok_ptr,x3->sym_value.tok_len);
    *(x + x3->sym_value.tok_len) = '\0';
    x0->sym_value.tok_type = DTOK;
    (void) date_val (x1->sym_value.tok_ptr,x,&x,
                              &(x0->sym_value.tok_val));
                  /* throw away return code */
    x0->sym_value.tok_ptr = x1->sym_value.tok_ptr;
                               /* i.e. whatever it was */
    x0->sym_value.tok_len = x1->sym_value.tok_len;
#ifdef DEBUG
(void) fprintf(sg.errout,"TO_DATE:  Input %*.*s,%*.*s\n",
        x1->sym_value.tok_len,x1->sym_value.tok_len,
                              x1->sym_value.tok_ptr, 
        x3->sym_value.tok_len,x3->sym_value.tok_len,
                              x3->sym_value.tok_ptr);
(void) fprintf(sg.errout,"             : Output %.16g\n",
                              x0->sym_value.tok_val);
#endif
    return EVALTRUE;
}
/****************************************************************************
 * Truncate a number to some number of decimal places
 */
static int trunc(s_expr)
struct s_expr * s_expr;
{
    char * x;
    int j;
    struct symbol * x0, *x1, *x3;
    struct symbol x4;
    x0 = s_expr->out_sym;
    x0->sym_value.tok_ptr= Lmalloc(NUMSIZE);
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    if (s_expr->in_link->next_link != (struct expr_link *) NULL)
    {
        (void) instantiate(s_expr->in_link->next_link->s_expr);
        x3 = s_expr->in_link->next_link->s_expr->out_sym;
    }
    else
    {
       x3 = &x4;
       x4.sym_value.tok_type = STOK;
    }
    if (x1->sym_value.tok_type != NTOK || x3->sym_value.tok_type != NTOK
        || floor(x3->sym_value.tok_val) == ((double) 0.0))
    {
        j = 0;
        if (x1->sym_value.tok_type == DTOK)
        {
            x0->sym_value.tok_type = DTOK;
            x0->sym_value.tok_val = floor(x1->sym_value.tok_val/86400.0) *
                                          86400.0;
            x0->sym_value.tok_len = x1->sym_value.tok_len;
            x0->sym_value.tok_ptr = x1->sym_value.tok_ptr;
            return EVALTRUE;
        }
        else
        {
            x0->sym_value.tok_type = NTOK;
            x0->sym_value.tok_val = floor(x1->sym_value.tok_val);
        }
    }
    else
    {
        double x_tmp;
        short int i = (short int) floor(x3->sym_value.tok_val);
        j = i;
        x0->sym_value.tok_type = NTOK;
        for (x_tmp = (double) 10.0;
               i-- > 1 ;
                   x_tmp *= 10.0);
        x0->sym_value.tok_val = floor (x_tmp * x1->sym_value.tok_val) /x_tmp;
    }
    (void) sprintf(x0->sym_value.tok_ptr,"%.*f",
                                (int) j, x0->sym_value.tok_val);
    x0->sym_value.tok_len = strlen(x0->sym_value.tok_ptr);
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/*****************************************************************************
 * Set everything to the same case (lower or upper)
 */
static int updown(s_expr)
struct s_expr * s_expr;
{
    register int i;
    register char * x2_ptr;
    register char * x1_ptr;
    struct symbol * x0, *x1;
    x0 = s_expr->out_sym;
    (void) instantiate(s_expr->in_link->s_expr);
    x1 = s_expr->in_link->s_expr->out_sym;
    i = x1->sym_value.tok_len;
    x2_ptr= Lmalloc(x1->sym_value.tok_len+1);
    x1_ptr = x1->sym_value.tok_ptr;
    x0->sym_value.tok_ptr = x2_ptr;
    x0->sym_value.tok_type = STOK;
    x0->sym_value.tok_len = i;
    if (s_expr->fun_arg == UPPER)
    {
        while (i--)
            if (islower(*x1_ptr)) *x2_ptr++ = *x1_ptr++ & 0xdf;
            else *x2_ptr++ = *x1_ptr++;
    }
    else
    {
        while (i--)
            if (isupper(*x1_ptr)) *x2_ptr++ = *x1_ptr++ | 0x20;
            else *x2_ptr++ = *x1_ptr++;
    }
    x0->sym_value.tok_val = x1->sym_value.tok_val;
    *x2_ptr='\0';
    if ( x0->sym_value.tok_val == (double)0.0)
        return EVALFALSE;
    else
        return EVALTRUE;
}
/***********************************************************************
 * Dynamic space management avoiding need to track all calls to malloc() in
 * the program.
 */
static char * alloced[EXPTABSIZE*4];    /* space to remember allocations */
static char ** first_free = alloced;
char * Lmalloc(space_size)
int space_size;
{
    *first_free = (char *) malloc(space_size);
    if (*first_free == NULL)
    {
       lex_first=LMEMOUT;
       longjmp(env,0);
    }
    if (first_free < &alloced[1027])
        return *first_free++;
    else
        return *first_free;   /* Memory leak preferred to corruption */
}
void Lfree()
{                                  /* return space to malloc() */
     while (--first_free >= alloced)
        if (*first_free != NULL) free(*first_free);
     first_free = alloced;
     return;
}
