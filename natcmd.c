/************************************************************
 * natqcmd.c - program to submit a command to natrun through a socket.
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1991";
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "supernat.h"

#define MOAN(fmt, str)    (void) fprintf(stderr, fmt, str)

/*
 *    Usage: natcmd Arguments
 */
int natcmd_main(argc, argv, envp)
int    argc;
char    **argv;
char    **envp;
{
int    i;
int ret;

    natinit(argc, argv);
    checkperm(U_READ|U_WRITE|U_EXEC,(QUEUE *) NULL);
    ret = 0;
    for (i = optind ; i < argc ; i++)
    {
         char * x =(char *) NULL;
         char buf[16];
         if (!strcmp(argv[i],"status"))
             x = "-d";
         else
         if (!strcmp(argv[i],"halt") && !strcmp(sg.username,"root"))
             x = "-h";
         else
         if (!strcmp(sg.username,"root"))
         {
             sprintf(buf,"-s %d",atoi(argv[i]));
             x = buf;
         }
         if (x != (char *) NULL)
             if (!wakeup_natrun(x))
                 exit(1);
    }
    exit(0);
}
