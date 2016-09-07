/************************************************************
 * nataperm.c - program to alter a user's permissions
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1991";
/*
 * Copyright (c) E2 Systems 1990. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "supernat.h"

static USER    u;
/*
 *    Usage: nataperm <perm> user...
 */
int nataperm_main(argc, argv, envp)
int    argc;
char    **argv;
char    **envp;
{
USER    *up;
int    ok;
int    i;
int ret;

    natinit(argc, argv);
    checkperm(U_READ|U_WRITE|U_EXEC, (QUEUE *) NULL);
    ret = 0;
    if (argc < 3 || (u.u_perm = doperms(argv[1])) < 0)
    {
        (void) fprintf(stderr, "Usage: %s [rwx] user...\n", *argv);
        exit(1);
    }
    for (i = 2 ; i < argc ; i++)
    {
        (void) strcpy(u.u_name, argv[i]);
        if ((up = getuserbyname(USERLIST, u.u_name)) == (USER *) NULL ||
                     strcmp(u.u_name, up->u_name))
        {
            ok = adduser((QUEUE *) NULL, &u);
            if (up != (USER *) NULL)
                free(up);
        }
        else
        {
            ok = chguser((QUEUE *) NULL, up, &u);
            free(up);
        }
        if (!ok)
        {
            ret = 1;
            (void) fprintf(stderr,
                             "ERROR - User %s. Status unchanged\n", u.u_name);
        }
    }
    exit(ret);
}
