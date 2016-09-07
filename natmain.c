/*****************************************************************************
 * natmain.c - common startup code to minimise the image size
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1992";
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
int nat_main();
int natalist_main();
int natalog_main();
int nataperm_main();
int natcalc_main();
int natdump_main();
int naterase_main();
int natglist_main();
int natgperm_main();
int natq_main();
int natqadd_main();
int natqdel_main();
int natqlist_main();
int natrebuild_main();
int natrm_main();
int natrun_main();
int natcmd_main();
static struct link_names {
char * prog_name;
int (*prog_main)();
} link_names[] = 
{
{ "nat", nat_main },
{ "natalist", natalist_main },
{ "natalog", natalog_main },
{ "nataperm", nataperm_main },
{ "natcalc", natcalc_main },
{ "natdump", natdump_main },
{ "naterase", naterase_main },
{ "natglist", natglist_main },
{ "natgperm", natgperm_main },
{ "natq", natq_main },
{ "natqadd", natqadd_main },
{ "natqchg", natqadd_main },
{ "natqdel", natqdel_main },
{ "natqlist", natqlist_main },
{ "natrebuild", natrebuild_main },
{ "natrm", natrm_main },
{ "natrun", natrun_main },
{ "natcmd", natcmd_main },
{0,0}};
int main (argc,argv,envp)
int argc;
char ** argv;
char ** envp;
{
    struct link_names * l;
    for (l = link_names;
              l->prog_name != (char *) 0;
                 l++)
    {
        char *x;
        x = strrchr(argv[0],'/');
        if (x == (char *) NULL)
            x = argv[0];
        else
            x++;
        if (!strcmp(x,l->prog_name))
            exit((*(l->prog_main))(argc,argv,envp));
    } 
    exit(1);
}
