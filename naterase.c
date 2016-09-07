/*************************************************************************
 *
 * naterase.c - the SUPERNAT variable database erasure program.
 *              For Supernat V3
 *
 *   The language is case-insensitive, with all values being
 *   cast to upper case.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1993\n";
#include  "supernat.h"
#include  "natcexe.h"

extern int errno;

/*************************************************************************** */
void varerase(var_name)
char * var_name;         /* Variable to erase */
{
long hoffset;
union hash_entry hash_entry;
struct symbol x_sym;

    x_sym.name_ptr = var_name;
    x_sym.name_len = strlen(var_name);
#ifdef DEBUG
    (void) fprintf(sg.errout,"Trying to delete name_len %d, name_ptr %d\n",
                       x_sym.name_len, (int) x_sym.name_ptr);
#endif
/*
  * At this point, we update the value database
  */
    for ( hash_entry.hi.in_use = 0,
          hoffset = fhlookup(natcalc_dh_con,x_sym.name_ptr,
                              x_sym.name_len, &hash_entry);
              hoffset > 0;
                  hoffset = fhlookup(natcalc_dh_con,x_sym.name_ptr,
                              x_sym.name_len, &hash_entry))
    {    /* The symbol is known; erase it */
        int rt;
        int rl;
        struct symbol *x2;
        (void) fh_get_to_mem(natcalc_dh_con,hash_entry.hi.body,
                      &rt,(char **) &x2,&rl);
        (void) free(x2);
                           /* throw away the current values */
        if (rt == VAR_TYPE)
        {
            hash_entry.hi.next = hoffset;
            fhremove(natcalc_dh_con,hash_entry.hi.name,0,&hash_entry);
            fh_chg_ref(natcalc_dh_con,hash_entry.hi.body, -1);
#ifdef DEBUG
            fprintf(sg.errout,"Variable delete - Index at %u Data at %u\n\
    Name: %s\n",
                        hoffset,hash_entry.hi.body,
                        x_sym.name_ptr);
                (void) fflush(sg.errout);
#endif
            break;
        }
    }
    return;                    /* false */
}
/*
 * Variables to be deleted are arguments
 */
int naterase_main(argc,argv,envp)
int argc;
char **argv;
char ** envp;
{
int c;
char * db_name;
char buf[BUFSIZ];
char * x, *x1, **y;

    natinit(argc,argv);
    checkperm(U_READ|U_WRITE,(QUEUE *) NULL);
    db_name = sg.supernatdb;
    while ((c = getopt(argc, argv, "wh:e:n:j:")) != EOF)
    {
        switch(c)
        {
        case 'n' :              /* The name of the DB */
            db_name = optarg;
            break;
        default:
        case '?' : /* Default - invalid opt.*/
            (void) fprintf(sg.errout,"Invalid option, must be:\n\
-n database to access\n");
            exit(1);
            break;
        }
    }
    if ((natcalc_dh_con = fhopen(db_name,string_hash)) == (DH_CON *) NULL)
        exit(1);
    (void) sprintf(buf,"naterase %s",sg.username);
   
    for ( x = buf + strlen(buf), y = &argv[optind]; y < &argv[argc]; y++)
    {
        for (x1 = *y; *x1 != '\0'; x1++)
            if (islower( *x1))
                *x1 = toupper( *x1);
        sprintf(x, " %s",*y);
        x += strlen(x);
    }
    if (wakeup_natrun(buf))
        exit(0);           /* O.K., successfully submitted */

    (void) fhclose(natcalc_dh_con);
    sg.read_write = 1;
    if ((natcalc_dh_con = fhopen(db_name,string_hash)) == (DH_CON *) NULL)
    {
        (void) fprintf(sg.errout,"Cannot re-open the database");
        exit(1);
    }
    for ( y = &argv[optind]; y < &argv[argc]; y++)
        varerase(*y);
    exit(0);
}
/* End of naterase.c */
