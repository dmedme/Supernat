/************************************************************
 * natgperm.c - program to grant permissions on a queue
 * Invocation - queue permissions users...
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
static    QUEUE    queue;

/*
 *    Usage: natgperm <queue> <perm> user...
 */
int natgperm_main(argc, argv, envp)
int    argc;
char    **argv;
char    **envp;
{
USER    *up;
char    *grant;
char * dir;
int    ok;
int    i;
int ret;
QUEUE * anchor = (QUEUE *) NULL;

    natinit(argc, argv);
    ret = 0;
    u.u_perm = 0;
    if (argc > 1)
        strcpy(queue.queue_name,argv[1]);
    if (argc < 4 ||
            (dir = queuetodir(&queue,&anchor)) == (char *) NULL ||
           (u.u_perm = doperms(argv[2])) < 0)
    {
        (void) fprintf(stderr,
                       "Usage: %s queue [rwx] user...\n", *argv);
        exit(1);
    }
    else for (i = 3 ; i < argc ; i++)
    {
        (void) strcpy(u.u_name, argv[i]);
        grant = grantname(&queue);
        if ((up = getuserbyname(grant, u.u_name)) == (USER *) NULL ||
                     strcmp(u.u_name,up->u_name))
        {
            ok = adduser(&queue, &u);
            if (up != (USER *) NULL)
                free(up);
        }
        else
        {
            ok = chguser(&queue, up, &u);
            free(up);
        }
        if (!ok)
        {
            (void) fprintf(stderr,
                          "ERROR - User %s. Status unchanged\n", u.u_name);
            ret = 1;
        }
    }
    exit(ret);
}
