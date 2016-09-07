/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     STRING = 258,
     NUMBER = 259,
     IDENT = 260,
     IS_DATE = 261,
     TO_DATE = 262,
     TO_CHAR = 263,
     SYSDATE = 264,
     TRUNC = 265,
     LENGTH = 266,
     ERR_RET = 267,
     ERR_CLR = 268,
     UPPER = 269,
     LOWER = 270,
     INSTR = 271,
     GETENV = 272,
     GETPID = 273,
     PUTENV = 274,
     SUBSTR = 275,
     SYSTEM = 276,
     HOLD = 277,
     RELEASE = 278,
     EXPRESS = 279,
     GET_STATUS = 280,
     GET_TIME = 281,
     GET_JOB_NO = 282,
     RE_QUEUE = 283,
     FINISH = 284,
     OR = 285,
     AND = 286,
     NE = 287,
     EQ = 288,
     GE = 289,
     LE = 290,
     RS = 291,
     LS = 292,
     UMINUS = 293
   };
#endif
/* Tokens.  */
#define STRING 258
#define NUMBER 259
#define IDENT 260
#define IS_DATE 261
#define TO_DATE 262
#define TO_CHAR 263
#define SYSDATE 264
#define TRUNC 265
#define LENGTH 266
#define ERR_RET 267
#define ERR_CLR 268
#define UPPER 269
#define LOWER 270
#define INSTR 271
#define GETENV 272
#define GETPID 273
#define PUTENV 274
#define SUBSTR 275
#define SYSTEM 276
#define HOLD 277
#define RELEASE 278
#define EXPRESS 279
#define GET_STATUS 280
#define GET_TIME 281
#define GET_JOB_NO 282
#define RE_QUEUE 283
#define FINISH 284
#define OR 285
#define AND 286
#define NE 287
#define EQ 288
#define GE 289
#define LE 290
#define RS 291
#define LS 292
#define UMINUS 293




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 100 "natcpars.y"

int ival;
struct s_expr * tval;



/* Line 2068 of yacc.c  */
#line 133 "y.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


