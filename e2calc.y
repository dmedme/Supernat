%{
#define malloc malloc
#define free free
/*************************************************************************
 *
 * e2calc.y - ORACLE SQL*FORMS user exit that can be built standalone to
 * give a shell date manipulating calculator program.
 * It needs datalib.o for the date capabilities.
 *
 Copyright (c) E2 Systems. All rights reserved.
                 : This software is furnished under a licence. It may not be
                   copied or further distributed without premission in writing
                   from the copyright holder. The copyright holder makes no
                   warranty, express or implied, as to the usefulness or
                   functionality of this software. The licencee accepts
                   full responsibility for any applications built with it,
                   unless that software be covered by a separate agreement
                   between the licencee and the copyright holder.
 *
 *   Notes on Use
 *   ------------ 
 *	
 *	For information about the features offered by this User Exit,
 *	see the documentation prior to the function declaration below,
 *      and the companion file e2calc.doc.
 *	
 *      When maintaining this program, always start from the yacc specification
 *      (.y) rather than the .pc form.
 *
 *      The following sequence of shell commands does the necessary conversion.
 *	
	yacc e2calc.y
#                       standard yacc parser generation
	sed < y.tab.c '/	printf/ s/	printf[ 	]*(/ fprintf(errout,/g' > y.tab.pc
#                       Change the yacc debugging code to write to the
#                       error log file rather than stdout.
	sed < y.tab.pc '/^# *line/d' > e2calc.pc 
#                       Remove the line directives that supposedly allow
#                       the source line debugger to place you in the
#                       absolute source file (i.e. the .y) rather than the
#                       .c. This feature (of pdbx) does not work properly;
#                       the debugger does recognise the line numbers, but is
#                       not able to recognise the source files, so it places
#                       you at random points in the .c.
#
# If compiled with -DDEBUG, each run of the program produces a family of
# of files that allow the trigger steps to be traced. The files are
# named e2calc???.
#
# If in addition it is compiled with -DYYDEBUG, the yacc parser diagnostics
# are also placed in these files.
 *
 *      If it is ever necessary to link this code with another derived from
 *      a yacc specification, the sed script that derives the .pc from y.tab.c
 *      must be extended to do something to the yy.* variables, to avoid linkage
 *      problems.
 *	
 *   Parameters
 *   ----------
 *
 *	Standard for iap User Exits.
 *
 *   Options
 *   --------
 *
 *   Assumptions
 *   -----------
 *
 *	See the documentation prior to the function declaration below.
 *
 *   Related Files
 *   -------------
 *
 *   Change History
 *   --------------
 *   Date   Author  Description
 *   ----   ------  ----------- 
 *   190290 DME     v.1.2; add getpid() function
 *   040292 DME     Changed so that it can be used standalone.
 *
 */
static char * sccs_id="@(#) $Name$ $Id$\n Copyright (c) E2 Systems\n";

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef SQLFORMS
#include "usrxit.h"
#include <malloc.h>
#else
#define IAPSUCC 1
#define IAPFAIL 0
#define IAPFTL 0
#define extf
#endif
extern double atof();
extern double strtod();
extern long strtol();
extern double floor();
extern double fmod();
extern char * getenv();
extern char * to_char();    /* date manipulation */
extf	int    e2calc();    /* the user exit; all else is internal */
#ifdef DEBUG
/********************************************************************
 *
 * redirect yacc debugging output, because linked with ORACLE forms stuff
 *
 */
                 extern int yydebug;
                 FILE * errout;
                 static int call_count=0;
                 static char err_file_name[40];
#endif
#ifdef SQLFORMS
EXEC SQL WHENEVER SQLERROR CONTINUE;
EXEC SQL BEGIN DECLARE SECTION;
	VARCHAR retvar[240];
				/* Fetch value from form into this var  */
	VARCHAR formfld[240];	/* Store block.field here         */
EXEC SQL END DECLARE SECTION;
EXEC SQL INCLUDE SQLCA.H;
#else

struct { short int len;
unsigned char arr[240];
} retvar;
struct { short int len;
unsigned char arr[240];
} formfld;
#endif

/**********************************************************************
 *
 * Where the lexical analysis has got to
 *
 */

enum lex_state { LBEGIN, /* First time */
                LGOING, /* Currently processing */
                LERROR,  /*  Syntax error detected by yacc parser */
                LMEMOUT  /*  Syntax error detected by yacc parser */
              };
static enum lex_state lex_first;
jmp_buf env;
extern int errno;

enum tok_type { NTOK, /* Number */
                STOK, /* String */
                ITOK, /* Identifier */
                DTOK  /* Date */
              };
typedef struct tok {
     enum tok_type tok_type;
     char * tok_ptr;
     int tok_length;
     double tok_val;
} TOK;

/***************************************************************************
 *
 *  Stuff to save on the IAF GETs
 *
 */
#define SYMTABSIZE 80
struct symbol
{
  short int name_len;
  char * name_ptr;
  TOK sym_value;
} symbol[SYMTABSIZE],
 *next_sym,
 *top_sym = &symbol[SYMTABSIZE];

/**********************************************************************
 *
 * Static lexical analyser status stuff
 *
 */
static int last_exp_val;
static char * parm;
static int parmlen;

static int yylex();		/* Lexical Analyser */

/*********************************************************************
 *
 * Dynamic space allocation
 *
 */
extern char * malloc();	        /* standard space allocation */
extern void free();		/* standard space deallocation */
static char * Lmalloc();	/* calloc() simulation */
static void Lfree();	        /* throw-away all Lmalloc()'ed stuff */
#define NUMSIZE 23
                                /* space malloc()'ed to take doubles */

%}

%union {
int ival;
TOK tval;
}

%token <tval> STRING NUMBER IDENT IS_DATE TO_DATE TO_CHAR SYSDATE TRUNC LENGTH
%token <tval> ERR_MSG UPPER LOWER INSTR GETENV GETPID PUTENV SUBSTR SYSTEM
%token <ival> '(' ')' ';' 
%type <tval> expr stat assnstat condstat
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
%%

list : /* empty */
     | list ';'
     | error
     {
#ifdef DEBUG
(void) fprintf( errout,"Parser Error, left %d chars %s\n",parmlen,parm);
#endif
      lex_first = LERROR;
      YYACCEPT;
     }
     | list stat 
     | list error
     {
#ifdef DEBUG
(void) fprintf( errout,"Parser Error, left %d chars %s\n",parmlen,parm);
#endif
     lex_first = LERROR;
     YYACCEPT; }
     ;

stat : assnstat
     | condstat
     ;
condstat : expr 
     {
#ifdef DEBUG
(void) fprintf( errout,"Expression without assign\n");
#endif
      if ( $1.tok_val == (double) 0.0)
      {
          last_exp_val = 0;
          YYACCEPT;
      }
      else last_exp_val = 1;
     }
     | expr ERR_MSG '(' expr ')'
     {
      if ($1.tok_val == (double) 0.0)
      {
          int corr_pass = $4.tok_length;
#ifdef SQLFORMS
          (void) sqliem($4.tok_ptr,&corr_pass);
#else
          (void) fprintf(stderr,"%s\n",$4.tok_ptr);
#endif
          last_exp_val = 0;
          YYACCEPT;
      }
      else last_exp_val = 1;
     }
     ;
assnstat : IDENT '=' expr 
     {
/*
 *               Update the Form Field
 */
           memcpy(formfld.arr,$1.tok_ptr,$1.tok_length);
                                     /* name of field */
           formfld.arr[$1.tok_length]='\0';
           formfld.len = $1.tok_length;
           if ($3.tok_type == NTOK && strpbrk($3.tok_ptr,"Ee" ) != (char *)NULL)
           {   /* make sure we do not put in an exponent notation field */
               short int i;
               retvar.len = sprintf(retvar.arr,"%.38f",$3.tok_val);
               for (i = retvar.len - 1; retvar.arr[i] == '0'; i--)
               {
                   retvar.len--;
                   retvar.arr[i]='\0';
               }
               if (retvar.arr[i] == '.')
               {
                   retvar.len--;
                   retvar.arr[i]='\0';
               }
           }
           else
           {
               memcpy(retvar.arr,$3.tok_ptr,$3.tok_length);
                                    /* value of field */
               retvar.len= $3.tok_length;
               retvar.arr[retvar.len]='\0';
           }
#ifdef SQLFORMS
           EXEC IAF PUT :formfld VALUES ( :retvar) ;
#else
/*
 * Must be left aligned to hide it from the edit script
 */
printf("%s=%s\n",formfld.arr,retvar.arr);
#endif
                                     /* put values, ignoring errors */
#ifdef DEBUG
(void) fprintf( errout,"PUT  %s value %s\n",formfld.arr,retvar.arr);
#endif
     }
     ;

expr : '(' expr ')'
     {   $$ = $2; }
     | expr EQ expr
     {
       $$.tok_type = NTOK;
       $$.tok_ptr = "0";
       $$.tok_length = 1;
       $$.tok_val = (double)0.0;

       if ($1.tok_val == $3.tok_val)
       if (($1.tok_type == NTOK && $3.tok_type == NTOK)
              || ($1.tok_type == DTOK && $3.tok_type == DTOK))
       {
           $$.tok_ptr = "1";
           $$.tok_val = (double)1.0;
       }
       else if ($1.tok_length == $3.tok_length)
       {
          register short int i = $1.tok_length;        
          register char * x1_ptr = $1.tok_ptr;        
          register char * x2_ptr = $3.tok_ptr;        
          for (;i && (*x2_ptr == *x1_ptr);
                         i-- , *x2_ptr++, *x1_ptr++);
          if (i == 0)
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }

     }
     | expr NE expr
     {
       $$.tok_type = NTOK;
       $$.tok_ptr = "1";
       $$.tok_length = 1;
       $$.tok_val = (double)1.0;

       if ($1.tok_val == $3.tok_val)
       if (($1.tok_type == NTOK && $3.tok_type == NTOK)
              || ($1.tok_type == DTOK && $3.tok_type == DTOK))
       {
           $$.tok_ptr = "0";
           $$.tok_val = (double)0.0;
       }
       else if ($1.tok_length == $3.tok_length)
       {
          register short int i = $1.tok_length;        
          register char * x1_ptr = $1.tok_ptr;        
          register char * x2_ptr = $3.tok_ptr;        
          for (;i  && (*x2_ptr == *x1_ptr);
                         i-- , *x2_ptr++, *x1_ptr++);
          if (i== 0)
          {
              $$.tok_ptr = "0";
              $$.tok_val = (double)0.0;
          }
       }
     }
     | expr GE expr
     {
       $$.tok_type = NTOK;
       $$.tok_ptr = "0";
       $$.tok_length = 1;
       $$.tok_val = (double)0.0;

       if (($1.tok_type == NTOK && $3.tok_type == NTOK)
              || ($1.tok_type == DTOK && $3.tok_type == DTOK))
       {
          if ($1.tok_val >= $3.tok_val)
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }
       else
       {
          register short int i = $1.tok_length >= $3.tok_length
                               ?  $3.tok_length
                               :  $1.tok_length;
          register char * x1_ptr = $1.tok_ptr;        
          register char * x2_ptr = $3.tok_ptr;        
          for (;i && (*x2_ptr == *x1_ptr);
                         i-- , *x2_ptr++, *x1_ptr++);
          if ((i != 0 && (*x2_ptr < *x1_ptr)) ||
              (i == 0 && ($1.tok_length >= $3.tok_length)))
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }
     }
     | expr LE expr
     {
       $$.tok_type = NTOK;
       $$.tok_ptr = "0";
       $$.tok_length = 1;
       $$.tok_val = (double)0.0;

       if (($1.tok_type == NTOK && $3.tok_type == NTOK)
              || ($1.tok_type == DTOK && $3.tok_type == DTOK))
       {
          if ($1.tok_val <= $3.tok_val)
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }
       else
       {
          register short int i = $1.tok_length >= $3.tok_length
                               ?  $3.tok_length
                               :  $1.tok_length;
          register char * x1_ptr = $1.tok_ptr;        
          register char * x2_ptr = $3.tok_ptr;        
          for (;i && (*x2_ptr == *x1_ptr);
                         i-- , *x2_ptr++, *x1_ptr++);
          if ((i != 0 && (*x2_ptr > *x1_ptr)) ||
              (i == 0 && ($1.tok_length <= $3.tok_length)))
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }
     }
     | expr '<' expr
     {
       $$.tok_type = NTOK;
       $$.tok_ptr = "0";
       $$.tok_length = 1;
       $$.tok_val = (double)0.0;

       if (($1.tok_type == NTOK && $3.tok_type == NTOK)
              || ($1.tok_type == DTOK && $3.tok_type == DTOK))
       {
          if ($1.tok_val < $3.tok_val)
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }
       else
       {
          register short int i = $1.tok_length >= $3.tok_length
                               ?  $3.tok_length
                               :  $1.tok_length;
          register char * x1_ptr = $1.tok_ptr;        
          register char * x2_ptr = $3.tok_ptr;        
          for (;i && (*x2_ptr == *x1_ptr);
                         i-- , *x2_ptr++, *x1_ptr++);
          if ((i != 0 && (*x2_ptr > *x1_ptr)) ||
              (i == 0 && ($1.tok_length < $3.tok_length)))
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }
     }
     | expr '>' expr
     {
       $$.tok_type = NTOK;
       $$.tok_ptr = "0";
       $$.tok_length = 1;
       $$.tok_val = (double)0.0;

       if (($1.tok_type == NTOK && $3.tok_type == NTOK)
              || ($1.tok_type == DTOK && $3.tok_type == DTOK))
       {
          if ($1.tok_val > $3.tok_val)
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double)1.0;
          }
       }
       else
       {
          register short int i = $1.tok_length >= $3.tok_length
                               ?  $3.tok_length
                               :  $1.tok_length;
          register char * x1_ptr = $1.tok_ptr;        
          register char * x2_ptr = $3.tok_ptr;        
          for (;i && (*x2_ptr == *x1_ptr);
                         i-- , *x2_ptr++, *x1_ptr++);
          if ((i != 0 && (*x2_ptr < *x1_ptr)) ||
              (i == 0 && ($1.tok_length > $3.tok_length)))
          {
              $$.tok_ptr = "1";
              $$.tok_val = (double) 1.0;
          }
       }
     }
     | expr '+' expr
     {
       if (($1.tok_type == STOK ||
           $3.tok_type == STOK) ||
        ($1.tok_type == DTOK &&
           $3.tok_type == DTOK))
       { /* string concatenation */
           $$.tok_type = STOK;
           $$.tok_length = $1.tok_length+$3.tok_length;
           $$.tok_ptr = Lmalloc($1.tok_length+$3.tok_length+1);
           memcpy($$.tok_ptr,$1.tok_ptr,$1.tok_length);
           memcpy($$.tok_ptr + $1.tok_length,
                   $3.tok_ptr,$3.tok_length);
           $$.tok_val = (double) 0.0;
       }
       else /* assume arithmetic */
       {
           if ($1.tok_type == DTOK)
           {
                $$.tok_type = DTOK;
                $$.tok_val = $1.tok_val + $3.tok_val*((double) 86400.0);
           }
           else if ($3.tok_type == DTOK)
           {
                $$.tok_type = DTOK;
                $$.tok_val = $3.tok_val + $1.tok_val*((double) 86400.0);
           }
           else
           {
                $$.tok_type = NTOK;
                $$.tok_val = $3.tok_val + $1.tok_val;
           }
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           if ($$.tok_type == NTOK)
               $$.tok_length = sprintf($$.tok_ptr,"%.16g",$$.tok_val);
           else
           {
               strcpy ($$.tok_ptr, to_char("DD-MON-YYYY",$$.tok_val));
               $$.tok_length = strlen($$.tok_ptr);
           }
       }
     }
     | expr '-' expr
     {
       /* assume arithmetic */
           if ($1.tok_type == DTOK)
               if ($3.tok_type != DTOK)
               {
                   $$.tok_type = DTOK;
                   $$.tok_val = $1.tok_val - $3.tok_val*((double) 86400.0);
               }
               else
               {
                   $$.tok_type = NTOK;
                   $$.tok_val = ($1.tok_val - $3.tok_val)/((double) 86400.0);
               }
           else
           {
               $$.tok_type = NTOK;
               $$.tok_val = $1.tok_val - $3.tok_val;
           }
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           if ($$.tok_type == NTOK)
               $$.tok_length = sprintf($$.tok_ptr,"%.16g",$$.tok_val);
           else
           {
               strcpy ($$.tok_ptr, to_char("DD-MON-YYYY",$$.tok_val));
               $$.tok_length = strlen($$.tok_ptr);
           }
     }
     | expr '*' expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val = $1.tok_val * $3.tok_val;
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr '/' expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           if ($3.tok_val == 0.0)
           $$.tok_val = 0.0;
           else $$.tok_val = $1.tok_val / $3.tok_val;
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr '&' expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val =  (double)((int) $1.tok_val &
                              (int) $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr '|' expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val =  (double)((int) $1.tok_val |
                              (int) $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr AND expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val =  (double)((int) $1.tok_val &&
                              (int) $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr '?' expr ':' expr
     {
       /* assume arithmetic */
           if ($1.tok_val == (double) 0.0)
                 $$ = $5;
           else  $$ = $3;
     }
     | expr OR expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val =  (double)((int) $1.tok_val ||
                              (int) $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr '^' expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val =  (double)((int) $1.tok_val ^
                              (int) $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr RS expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val =  (double)((int) $1.tok_val >>
                              (int) $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr LS expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val =  (double)((int) $1.tok_val <<
                              (int) $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | expr '%' expr
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val = fmod($1.tok_val, $3.tok_val);
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | '-' expr %prec UMINUS
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val = - $2.tok_val ;
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | '!' expr 
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           if ($2.tok_val == (double) 0.0)
           {
                $$.tok_val = (double) 1.0;
                $$.tok_ptr = "1";
           }
           else
           {
                $$.tok_val = (double) 0.0;
                $$.tok_ptr = "0";
           }
           $$.tok_length =1;
     }
     | '~' expr 
     {
       /* assume arithmetic */
           $$.tok_type = NTOK;
           $$.tok_val = (double) (~ (int) $2.tok_val) ;
           $$.tok_ptr = Lmalloc(NUMSIZE);  /* default value */
           $$.tok_length =sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | IS_DATE '(' expr ';' expr ')'
     {
       /* Is it a valid date of this format? */
           char * x= Lmalloc($5.tok_length+1);  /* format must be null
                                                   terminated */
           (void) memcpy (x,$5.tok_ptr,$5.tok_length);
           *(x + $5.tok_length) = '\0';
           $$.tok_type = NTOK;
           $$.tok_length = 1;
           if ( date_val ($3.tok_ptr,x,&x,&($$.tok_val)))
           {                                    /* returned valid date */
#ifdef DEBUG
(void) fprintf(errout,"IS_DATE Valid: Input %*.*s,%*.*s\n",
               $3.tok_length,$3.tok_length,$3.tok_ptr, 
               $5.tok_length,$5.tok_length,$5.tok_ptr);
(void) fprintf(errout,"             : Output %.16g\n",$$.tok_val);
#endif
               $$.tok_val = (double) 1.0;
               $$.tok_ptr="1";
           }
           else
           {
#ifdef DEBUG
(void) fprintf(errout,"IS_DATE Failed: Input %*.*s,%*.*s\n",
               $3.tok_length,$3.tok_length,$3.tok_ptr, 
               $5.tok_length,$5.tok_length,$5.tok_ptr);
(void) fprintf(errout,"              : Output %.16g\n",$$.tok_val);
#endif
               $$.tok_val = (double) 0.0;
               $$.tok_ptr="0";
           }
     }
     | TO_DATE '(' expr ';' expr ')'
     {
       /* Get the date for this format */
           char * x= Lmalloc($5.tok_length+1);  /* format must be null
                                                   terminated */
           (void) memcpy (x,$5.tok_ptr,$5.tok_length);
           *(x + $5.tok_length) = '\0';
           $$.tok_type = DTOK;
           (void) date_val ($3.tok_ptr,x,&x,&($$.tok_val));
                         /* throw away return code */
           $$.tok_ptr = $3.tok_ptr; /* i.e. whatever it was */
           $$.tok_length = $3.tok_length;
#ifdef DEBUG
(void) fprintf(errout,"TO_DATE:  Input %*.*s,%*.*s\n",
               $3.tok_length,$3.tok_length,$3.tok_ptr, 
               $5.tok_length,$5.tok_length,$5.tok_ptr);
(void) fprintf(errout,"             : Output %.16g\n",$$.tok_val);
#endif
     }
     | TO_CHAR '(' expr ';' expr ')'
     {
       /* Generate a formatted string */
           char * x= Lmalloc($5.tok_length+1);  /* format must be null
                                                   terminated */
           (void) memcpy (x,$5.tok_ptr,$5.tok_length);
           *(x + $5.tok_length) = '\0';
           $$.tok_type = DTOK;
           x = to_char (x,$3.tok_val);
           $$.tok_length= strlen(x);
           $$.tok_ptr= Lmalloc($$.tok_length+1);
           (void) memcpy($$.tok_ptr,x, $$.tok_length);
     }
     | TRUNC '(' expr ';' expr ')'
     {
         if ($3.tok_type != NTOK || $5.tok_type != NTOK)
             $$ = $3 ;
         else
         {
             short int i = (short int) floor($5.tok_val);
             short int j = i;
             $$.tok_type = NTOK;
             $$.tok_ptr= Lmalloc(NUMSIZE);
             if (i == 0)
                 $$.tok_val = floor($3.tok_val);
             else
             {
                 double x_tmp;
                 for (x_tmp = (double) 10.0;
                        i-- > 1 ;
                            x_tmp *= 10.0);
                 $$.tok_val = floor (x_tmp * $3.tok_val)/x_tmp;
             }
             $$.tok_length = sprintf($$.tok_ptr,"%.*f",(int)
                    j, $$.tok_val);
         }
     }
     | INSTR '(' expr ';' expr ')'
     {
         if ($5.tok_length == 0 || $3.tok_length == 0)
         {
             $$.tok_type = STOK;
             $$.tok_val = (double) 0.0;
             $$.tok_length = 0;
             $$.tok_ptr = "";
         }
         else if ($5.tok_length > $3.tok_length)
         {
             $$.tok_type = NTOK;
             $$.tok_val = (double) 0.0;
             $$.tok_length = 1;
             $$.tok_ptr = "0";
         }
         else
         {
             char * x3_ptr = $3.tok_ptr;
             register char * x5_ptr = $5.tok_ptr;
             short int i = $3.tok_length - $5.tok_length +1;
             register char * x1_ptr;
             register short int j;

             for (;i; i--, x3_ptr++)
             {
                 for (j = $5.tok_length,
                      x1_ptr = x3_ptr,
                      x5_ptr = $5.tok_ptr;
                         j > 0 && *x1_ptr++ == *x5_ptr++;
                           j--);
                 if (j == 0) break;
             }
             $$.tok_type = NTOK;
             if (i == 0)
             {
                 $$.tok_val = (double) 0.0;
                 $$.tok_length = 1;
                 $$.tok_ptr = "0";
             }
             else
             {
                 $$.tok_ptr = Lmalloc(NUMSIZE);
                 $$.tok_val = (double) ($3.tok_length - $5.tok_length +2 -i);
                 $$.tok_length = sprintf($$.tok_ptr,"%.16g",$$.tok_val);
             }
         }
     }
     | SUBSTR '(' expr ';' expr ';' expr ')'
     {
         $$.tok_type = STOK;
         if (($7.tok_val <= (double) 0.0) ||
              ($3.tok_length == 0) ||
              (((int) $5.tok_val) > $3.tok_length) ||
              (($5.tok_val < (double) 0.0) &&
              (-((int) $5.tok_val) > $3.tok_length)))
         {
             $$.tok_ptr="";
             $$.tok_val=(double) 0.0;
             $$.tok_length=0;
         }
         else
         {
             register short int i;
             register char * x2_ptr;
             register char * x1_ptr;
             char * strtodgot;
             if ($5.tok_val == (double) 0.0)
             {
                 i= $3.tok_length;
                 x1_ptr = $3.tok_ptr;
             }
             else if ($5.tok_val > (double) 0.0)
             {
                 if (((int) ($5.tok_val + $7.tok_val -1.0) <= $3.tok_length))
                     i = $7.tok_val;
                 else
                     i = 1 + $3.tok_length - ((int) $5.tok_val);
                 x1_ptr = $3.tok_ptr + ((int) $5.tok_val) -1;
             }
             else
             {
                 if (($$.tok_length - ((int) ($5.tok_val) + (int)
                      ($7.tok_val -1.0) <= $3.tok_length)))
                     i = $7.tok_val;
                 else
                     i = -((int) $5.tok_val);
                 x1_ptr = $3.tok_ptr + $3.tok_length + ((int) $5.tok_val);
             }
               
             x2_ptr= Lmalloc(i+1);
             $$.tok_ptr = x2_ptr;
             $$.tok_length = i;
             while (i--)
                *x2_ptr++ = *x1_ptr++;
             *x2_ptr='\0';
             $$.tok_val = strtod( $$.tok_ptr,&strtodgot);
                                     /* attempt to convert to number */
   
/*             if (strtodgot != $$.tok_ptr) strtodgot--;
                                     /o correct strtod() overshoot
                                     /o doesn't happen on Pyramid */
             if (strtodgot == $$.tok_ptr + $$.tok_length)
                                     /* valid numeric */ 
                 $$.tok_type = NTOK;
             else
                 $$.tok_type = STOK;
         }
     }
     | TRUNC '(' expr ')'
     {
         if ($3.tok_type != NTOK)
             $$ = $3 ;
         else
         {
             $$.tok_ptr= Lmalloc(NUMSIZE);
             $$.tok_type = NTOK;
             $$.tok_val = floor($3.tok_val);
             $$.tok_length = sprintf($$.tok_ptr,"%.16g",$$.tok_val);
         }

     }
     | GETENV '(' expr ')'
     {
         char * x= Lmalloc($3.tok_length+1);  /* environment string must
                                                 be null terminated */
         (void) memcpy (x,$3.tok_ptr,$3.tok_length);
         *(x + $3.tok_length) = '\0';
         if (($$.tok_ptr = getenv(x)) == (char *)NULL)
         {
             $$.tok_type = STOK;
             $$.tok_ptr = "";
             $$.tok_length = 0; 
             $$.tok_val = (double) 0.0; 
         }
         else
         {
             char * strtodgot;
             $$.tok_length = strlen($$.tok_ptr);
             $$.tok_val = strtod( $$.tok_ptr,&strtodgot);
                                     /* attempt to convert to number */
/*             if (strtodgot != $$.tok_ptr) strtodgot--;
                                     /o correct strtod() overshoot
                                     /o doesn't happen on Pyramid */
             if (strtodgot == $$.tok_ptr + $$.tok_length)
                                     /* valid numeric */ 
                 $$.tok_type = NTOK;
             else
                 $$.tok_type = STOK;
         }
     }
     | PUTENV '(' expr ')'
     {
         char * x= (char *) malloc($3.tok_length+1);  /* environment string must
                                              * be null terminated
                                              * and should be static
                                              * Call malloc() directly to avoid
                                              * re-use.
                                              */
         (void) memcpy (x,$3.tok_ptr,$3.tok_length);
         *(x + $3.tok_length) = '\0';
         $$.tok_val = (double) putenv(x);
         $$.tok_type = NTOK;
         $$.tok_ptr= Lmalloc(NUMSIZE);
         $$.tok_length = sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | GETPID
     {
         $$.tok_type = NTOK;
         $$.tok_val = (double) getpid();
         $$.tok_ptr= Lmalloc(NUMSIZE);
         $$.tok_length = sprintf($$.tok_ptr,"%.16g",$$.tok_val);
     }
     | SYSTEM '(' expr ')'
     {
         char * x= Lmalloc($3.tok_length+1);  /* system command string must
                                                 be null terminated */
         (void) memcpy (x,$3.tok_ptr,$3.tok_length);
         *(x + $3.tok_length) = '\0';
         $$.tok_type = NTOK;
         $$.tok_val = (double) system(x);
         $$.tok_ptr= Lmalloc(NUMSIZE);
         $$.tok_length = sprintf($$.tok_ptr,"%.16g",$$.tok_val);
/*
 * All attempts to run background processes so far have caused the ORACLE
 * process to wait for termination of the spawned process, even if it is
 * run asynchronously; eg, the following, which attempted to break the
 * connexion between the grand-child process and this one by terminating
 * the parent:
 *       if (!vfork())
 *         if (!vfork())
 *            _exit(0);
 *         else
 *         {
 *            execl(getenv("SHELL"),"sh","-c",x,(char *) NULL);
 *            _exit(-1);
 *         }
 *       $$.tok_val=0.0;
 *       $$.tok_ptr="0";
 *       $$.tok_length=1;
 */
     }
     | UPPER '(' expr ')'
     {
         register short int i = $3.tok_length;
         register char * x2_ptr= Lmalloc($3.tok_length+1);
         register char * x1_ptr = $3.tok_ptr;
         $$.tok_ptr = x2_ptr;
         $$.tok_type = STOK;
         $$.tok_length = i;
         while (i--)
             if (islower(*x1_ptr)) *x2_ptr++ = *x1_ptr++ & 0xdf;
             else *x2_ptr++ = *x1_ptr++;
         $$.tok_val = $3.tok_val;
         *x2_ptr='\0';
     }
     | LOWER '(' expr ')'
     {
         register short int i = $3.tok_length;
         register char * x2_ptr= Lmalloc($3.tok_length+1);
         register char * x1_ptr = $3.tok_ptr;
         $$.tok_ptr = x2_ptr;
         $$.tok_type = STOK;
         $$.tok_length = i;
         while (i--)
             if (isupper(*x1_ptr)) *x2_ptr++ = *x1_ptr++ | 0x20;
             else *x2_ptr++ = *x1_ptr++;
         $$.tok_val = $3.tok_val;
         *x2_ptr='\0';
     }
     | LENGTH '(' expr ')'
     {
         $$.tok_type = NTOK;
         $$.tok_ptr= Lmalloc(NUMSIZE);
         $$.tok_length = sprintf($$.tok_ptr,"%ld",$3.tok_length);
         $$.tok_val = (double) $3.tok_length;
     }
     | SYSDATE
     | STRING
     | IDENT
     {
         register struct symbol * x_sym_ptr;

         for (x_sym_ptr = symbol;
                        x_sym_ptr < next_sym;
                                 x_sym_ptr ++)
         {                        /* have we already got this one? */
             if (x_sym_ptr->name_len == $1.tok_length) 
             {
                 register short int i = $1.tok_length;        
                 register char * x1_ptr = $1.tok_ptr;        
                 register char * x2_ptr = x_sym_ptr->name_ptr;
                 for (;i && (*x2_ptr == *x1_ptr);
                         i-- , *x2_ptr++, *x1_ptr++);
                 if (i == 0) break;
             }
         }
         if (x_sym_ptr < next_sym)
                                 /* We have got this already */

             $$ = x_sym_ptr->sym_value;
         
         else                     /* New symbol */
         {
             char * x;                /* for where strtod() gets to */

             memcpy(formfld.arr,$1.tok_ptr,$1.tok_length);
             formfld.arr[$1.tok_length]='\0';
             formfld.len = $1.tok_length;
                                     /* name of field */
#ifdef SQLFORMS
             EXEC IAF GET :formfld INTO :retvar   ;
             retvar.arr[retvar.len]='\0';
#else
             (void) strcpy(retvar.arr,formfld.arr);
             retvar.len = formfld.len;
#endif
                                     /* get values, ignoring errors */
#ifdef DEBUG
(void) fprintf( errout,"GET %s value %s\n",formfld.arr,retvar.arr);
#endif
             $$.tok_ptr = Lmalloc((int) retvar.len+1);
                                     /* save returned value */
             $$.tok_length = retvar.len;
             memcpy($$.tok_ptr,retvar.arr,(int) retvar.len);
             *($$.tok_ptr+retvar.len) = '\0';
             $$.tok_val = strtod( retvar.arr,&x);
                                     /* attempt to convert to number */
   
/*             if ((unsigned char *) x != retvar.arr) x--;
                                     /o correct strtod() overshoot o/
                                     /o doesn't happen on Pyramid */
             if ((unsigned char *) x == &retvar.arr[retvar.len])
                                     /* valid numeric */ 
             {
                 $$.tok_type = NTOK;
#ifdef DEBUG
(void) fprintf( errout,"NUMERIC TOKEN, length %ld ,value %.16g\n",
                 $$.tok_length,
                 $$.tok_val);
#endif
             }
             else
             {
                 $$.tok_type = STOK;
#ifdef DEBUG
(void) fprintf( errout,"STRING TOKEN length %ld ,value %.16g\n",
               $$.tok_length,
               $$.tok_val);
#endif
             }
             if (next_sym < top_sym)
             {                         /* there is room for another */

                 next_sym->name_len = $1.tok_length;
                 next_sym->name_ptr = $1.tok_ptr;
                 next_sym->sym_value = $$;
                 next_sym++;
             }
         }
     }
     | NUMBER
     ;
%%
/* *************************************************************************

e2calc is called with the following syntax for the parameter string p:

e2calc <sequence of terms according to the yacc grammar above >

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

e2calc returns either when all the text has been parsed and
evaluated, or when a condition has evaluated false.

A condition can be associated with an error message, which will be
displayed in preference to the trigger fail step, should it evaluate
to false, thus:

 condition ERR_MSG ( string expression )

This provides a means for putting out dynamic messages, if required; supply
a condition of the form  0 == 1, and a string expression that includes
variable values.

e2calc will return IAPFAIL if a condition evaluates false,
otherwise it will return IAPSUCC.

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

A problem is that caiap won't exit if any backgroound jobs are still running;
this seems to be a feature of the ORACLE RELEASE sub-command.

*************************************************************************** */


int	 e2calc(p, paramlen, erm, ermlen, query)
register    char       *p;		     /* Parameter string */
	    int	       *paramlen;	     /* Ptr to param string length */
	    char       *erm;		     /* Error message if doesnt match */
	    int	       *ermlen;	     	     /* Ptr to error message length */
	    int	       *query;	             /* Ptr to query status flag */
{
#ifdef DEBUG
#ifdef YYDEBUG
yydebug=1;
#endif
call_count++;
(void) sprintf(err_file_name,"e2calc%ld",call_count);
errout = fopen(err_file_name,"w");
(void) fprintf( errout,"Passed length paramlen %ld, %s \n",*paramlen,p);
#endif
   parm = p;                                 /* tell yylex() where parameters
                                                are
                                              */
   parmlen = *paramlen;
                                             /* tell yylex() length of params
                                              */
   lex_first = LBEGIN;
   last_exp_val = -1;                        /* set to 'nothing' value */
   next_sym = symbol;                        /* clear symbol table */
   if (!setjmp(env))
       (void) yyparse();
   Lfree();				     /* return Lmalloc()'ed space */
#ifdef DEBUG
(void) fprintf( errout,"Returned %ld\n",last_exp_val);
(void) fclose(errout);
#endif
   if (lex_first == LERROR)                  /* syntax error detected */
   {
       char error_text[81];
       int error_len=80;
(void) sprintf( error_text,"PARSE ERROR: %67.67s",parm);
#ifdef SQLFORMS
       (void) sqliem (error_text,&error_len);
#else
       (void) fprintf(stderr,"%s\n",error_text);
#endif
       return (IAPFTL);
   }
   else if (lex_first == LMEMOUT)                  /* malloc error detected */
   {
       char error_text[81];
       int error_len=80;
(void) sprintf( error_text,"malloc() failed: OS error %d ",errno);
#ifdef SQLFORMS
       (void) sqliem (error_text,&error_len);
#else
       (void) fprintf(stderr,"%s\n",error_text);
#endif
       return (IAPFTL);
   }
   else if (last_exp_val) return (IAPSUCC);  /* condition true or absent */
   else return (IAPFAIL);                    /* false */

}
/************************************************************************

  Lexical Analyser

*/

static int yylex()
{
/**************************************************************************
 Lexical analysis of User Exit parameter string

*/
   if (lex_first == LBEGIN)
                                   /* first time, skip the name */
       for (lex_first = LGOING;
               parmlen > 0 && *parm != ' ' && *parm != '\n' && *parm != '\t';
                         parm++, parmlen--);
   if (parmlen <= 0) return (0);                   /* exit if all done */	
   for (; parmlen > 0 && ( *parm == ' ' || *parm == '\n' || *parm == '\t');
                              parm++, parmlen--);
                                                   /* skip over white space */
   if (parmlen <= 0) return (0);                   /* exit if all done */	

   while(parmlen >= 0)
   switch (*parm)
   {                               /* look at the character */
   case ';':
   case ',':
             parmlen--;
             parm++;      
             return ';';      /* return the operator */
   case '{':
   case '[':
   case '(':
             parmlen--;
             parm++;
             return '(';
   case '}':
   case ']':
   case ')':
             parmlen--;
             parm++;
             return ')';
   case '?':
   case ':':
   case '+':
   case '-':
   case '*':
   case '/':
   case '%':
   case '~':
   case '^':
             parmlen--;
             return *parm++;      /* return the operator */
   case '!':
             parmlen--;
             parm++;
             if (parmlen <=0 || *parm != '=')   
                return '!';      /* return the operator */ 
                                 /* else */
             parmlen--;
             parm++;
             return NE;          /* return '!='         */
   case '|':
             parmlen--;
             parm++;
             if (parmlen <=0 || *parm != '|')   
                return '|';      /* return the operator */ 
                                 /* else */
             parmlen--;
             parm++;
             return OR;          /* return '||'         */
   case '&':
             parmlen--;
             parm++;
             if (parmlen <=0 || *parm != '&')   
                return '&';      /* return the operator */ 
                                 /* else */
             parmlen--;
             parm++;
             return AND;          /* return '&&'         */
   case '=':
             parmlen--;
             parm++;
             if (parmlen <=0 || *parm != '=')   
                return '=';      /* return the operator */ 
                                 /* else */
             parmlen--;
             parm++;
             return EQ;          /* return '=='         */
   case '<':
             parmlen--;
             parm++;
             if (parmlen <=0 || (*parm != '<' && *parm != '='))   
                return '<';      /* return the operator */ 
             else
             if (*parm == '=')
             {
                  parmlen--;
                  parm++;
                  return LE;          /* return '<='         */
             }
             else
             if (*parm == '<')
             {
                  parmlen--;
                  parm++;
                  return LS;          /* return '<<'         */
             }
   case '>':
             parmlen--;
             parm++;
             if (parmlen <=0 || (*parm != '>' && *parm != '='))   
                return '>';      /* return the operator */ 
             else
             if (*parm == '=')
             {
                  parmlen--;
                  parm++;
                  return GE;          /* return '>='         */
             }
             else
             if (*parm == '>')
             {
                  parmlen--;
                  parm++;
                  return RS;          /* return '>>'         */
             }
   case '"':
   case '\\':
   case '`':
   case '\'':                         /* string; does not support
                                         embedded quotes, but concatenation
                                         and alternative delimiters provide
                                         work-rounds
                                       */
             if (--parmlen <= 0) return 0;
                                      /* ignore empty non-terminated string */
             {
                 char * string_term=parm;
                 yylval.tval.tok_ptr = ++parm;
                 yylval.tval.tok_type = STOK;
                 for (yylval.tval.tok_length = 0;
                       parmlen-- > 0 && *parm++ != *string_term;
                            yylval.tval.tok_length++);
                                      /* advance to end of string */
             yylval.tval.tok_val = atof(yylval.tval.tok_ptr);
                                      /* just in case it's useful */
#ifdef DEBUG
   (void) fprintf(errout,"Found STRING %*.*s\n",yylval.tval.tok_length,
                        yylval.tval.tok_length,yylval.tval.tok_ptr);
#endif   
             return STRING ;
             }
 
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
       yylval.tval.tok_val = strtod( parm, &x);
                                      /* convert */
       if (x != parm)                 /* valid */ 
       {
/* x--;                     /o correct strtod() overshoot,
                            /o not needed on Pyramid */
             yylval.tval.tok_ptr = parm;
             yylval.tval.tok_length = (int) (x - parm);
             yylval.tval.tok_type = NTOK;
             parm = x;
             parmlen -= yylval.tval.tok_length;
#ifdef DEBUG
   (void) fprintf(errout,"Found NUMBER %.16g\n",yylval.tval.tok_val);
#endif   
             return NUMBER;
       }
     }
     default:                        /* assume everything else is an identifier
                                      */
             yylval.tval.tok_type = ITOK;
             yylval.tval.tok_ptr = parm;
             for (yylval.tval.tok_length = 0;
                       parmlen > 0 && (isalnum ((int) *parm) ||
                                       *parm == '_' ||
                                       *parm == '$' ||
                                       *parm == '.' ) ;
                            parmlen--,
                            parm++,
                            yylval.tval.tok_length++);
             if (yylval.tval.tok_ptr == parm) /* non-ASCII */
             {
                 parm++;                     /* skip this character */
                 parmlen--;
             }
             else
             {
                 register char * x1_ptr;
                 register short int i;
                 for (i=yylval.tval.tok_length,
                      x1_ptr=yylval.tval.tok_ptr;
                           i; 
                                i--,x1_ptr++)
                     if (islower(*x1_ptr)) *x1_ptr &= 0xdf;
                 switch (yylval.tval.tok_length)
                 {
                 case 7:
                   /* Most of the interesting cases */
                     if (!strncmp(yylval.tval.tok_ptr,"SYSDATE",7))
                     {
                         yylval.tval.tok_ptr = Lmalloc(12);
                         yylval.tval.tok_type = DTOK;
                         (void) date_val((char *) 0,"SYSDATE", (char *) 0,
                                          &(yylval.tval.tok_val));
                         (void) strcpy (yylval.tval.tok_ptr,
                                   to_char("DD-MON-YY",yylval.tval.tok_val));
                         yylval.tval.tok_length = 11;
#ifdef DEBUG
(void) fprintf(errout,"SYSDATE:  Output %.16g\n",yylval.tval.tok_val);
#endif
                         return SYSDATE;
                     }
                     else if (!strncmp(yylval.tval.tok_ptr,"IS_DATE",7))
                         return IS_DATE;
                     else if (!strncmp(yylval.tval.tok_ptr,"TO_DATE",7))
                         return TO_DATE;
                     else if (!strncmp(yylval.tval.tok_ptr,"TO_CHAR",7))
                         return TO_CHAR;
                     else if (!strncmp(yylval.tval.tok_ptr,"ERR_MSG",7))
                         return ERR_MSG;
                     break;
                 case 5:
                     if (!strncmp(yylval.tval.tok_ptr,"TRUNC",5))
                         return TRUNC;
                     else if (!strncmp(yylval.tval.tok_ptr,"UPPER",5))
                         return UPPER;
                     else if (!strncmp(yylval.tval.tok_ptr,"LOWER",5))
                         return LOWER;
                     else if (!strncmp(yylval.tval.tok_ptr,"INSTR",5))
                         return INSTR;
                     break;
                 case 6:
                     if (!strncmp(yylval.tval.tok_ptr,"LENGTH",6))
                         return LENGTH;
                     else if (!strncmp(yylval.tval.tok_ptr,"SUBSTR",6))
                         return SUBSTR;
                     else if (!strncmp(yylval.tval.tok_ptr,"PUTENV",6))
                         return PUTENV;
                     else if (!strncmp(yylval.tval.tok_ptr,"GETENV",6))
                         return GETENV;
                     else if (!strncmp(yylval.tval.tok_ptr,"GETPID",6))
                         return GETPID;
                     else if (!strncmp(yylval.tval.tok_ptr,"SYSTEM",6))
                         return SYSTEM;
                     break;
                 default:
                     break;
                 }

                 return IDENT;
             }

    } /* switch is repeated on next character if have not returned */
    return ';'; /* ie statement terminator, nice and harmless */
}
int yyerror()
{
/* We do not have errors */
    return 0;
}
/***********************************************************************
 * Dynamic space management avoiding need to track all calls to malloc() in
 * the program.
 */
static char * alloced[1028];       /* space to remember allocations */
static char ** first_free = alloced;
static char * Lmalloc(space_size)
int space_size;
{
    *first_free = (char *) malloc(space_size);
    if (*first_free == (char *)NULL)
    {
       lex_first=LMEMOUT;
       longjmp(env,0);
    }
    if (first_free < &alloced[1027])
        return *first_free++;
    else
        return *first_free;   /* Memory leak preferred to corruption */
}
static void Lfree()
{                                  /* return space to malloc() */
     while (--first_free >= alloced)
        if (*first_free != (char *)NULL) free(*first_free);
     first_free = alloced;
     return;
}
#ifdef STAND
main(argc,argv)
int argc;
char * argv[];
{
    char buf[BUFSIZ];
    int i;
    buf[0] = '\0';
    for (i = 0; i < argc; i++)
    {
        strcat(buf,argv[i]);
        strcat(buf," ");
    }
    i = strlen(buf);
    return e2calc(buf,&i,(char *) NULL,(int *) NULL, (int *) NULL);
}
#endif

/* End of e2calc.y */
