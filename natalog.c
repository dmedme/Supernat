/************************************************************
 * natlog.c - program to log use of a nat shell script
 * Call it with the shell script name and arguments as its arguments
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1990\n";
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "supernat.h"

int natalog_main(argc, argv, envp)
int    argc;
char    **argv;
char    **envp;
{
    natinit(argc-1, &argv[1]);
    checkperm(U_READ, (QUEUE *) NULL);
    exit(0);
}
