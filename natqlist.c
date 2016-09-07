/************************************************************
 * natqlist.c - program to list all the queues
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1990";
/*
 * Copyright (c) E2 Systems 1990. All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "supernat.h"
/*
 *	Usage: natqlist
 * Lists the queues on stdout.
 */
int natqlist_main(argc, argv, envp)
int	argc;
char	**argv;
char	**envp;
{
QUEUE * q_anchor = (QUEUE *) NULL;
QUEUE * q_last = (QUEUE *) NULL;

    natinit(argc, argv);
    checkperm(U_READ|U_WRITE, (QUEUE *)NULL);
    if (!queue_file_open(0,(FILE**) NULL ,QUEUELIST,&q_anchor,&q_last))
        exit(1);
    queue_file_write(stdout,q_anchor);
    exit(0);
}
