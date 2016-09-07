/************************************************************
 * natalist.c - program to list users and their permissions
 */
static char * sccs_id="@(#) $Name$ $Id$\n Copyright (c) E2 Systems 1990";
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/*
 * Copyright (c) E2 Systems 1990. All rights reserved.
 */

#include "supernat.h"

int natalist_main(argc, argv, envp)
int    argc;
char    **argv;
char    **envp;
{
USER    *up;
FILE    *fp;
int    lflag = 0;
int    i = 0;
int    n;
int    c;

    /* see if a long listing's wanted */
    while ((c = getopt(argc, argv, "l")) != EOF)
    {
        switch(c)
	{
        case 'l' :
            lflag = 1;
            break;
        }
    }

    /* check that user can read $SUPERNAT */
    natinit(argc, argv);
    checkperm(U_READ, (QUEUE *) NULL);

    /* do it */
    if ((fp = openfile(sg.supernat, USERLIST, "r")) != (FILE *) NULL) {
        for ( ; (up = getuserent(fp)) != (USER *) NULL ; i++) {
            printf("%s", up->u_name);
            if (lflag)
                printf("\t%s", pperm(up->u_perm));
            (void) putchar('\n');
        }
        (void) fclose(fp);
    }
    if (lflag)
        printf("Total: %d users\n", i);
    exit(0);
}
