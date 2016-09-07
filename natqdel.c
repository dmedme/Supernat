/************************************************************
 * natqdel.c - program to delete a queue
 * The update is firstly by sending to the natrun process, and
 * if that fails, this program does it itself.
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1991";
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "supernat.h"

#define MOAN(fmt, str)    (void) fprintf(stderr, fmt, str)

/*
 *    Usage: natqdel queue ...
 */
int natqdel_main(argc, argv, envp)
int    argc;
char    **argv;
char    **envp;
{
QUEUE    *queue;
QUEUE    *anchor;
int    i;
int ret;
char buf[BUFSIZ];

    NEW (QUEUE,queue);
    anchor = (QUEUE *) NULL;
    natinit(argc, argv);
    checkperm(U_READ|U_WRITE,(QUEUE *) NULL);
    ret = 0;
    for (i = optind ; i < argc ; i++)
    {
        strcpy(queue->queue_name,argv[i]);
        if (queuetodir(queue,&anchor) == (char *) NULL)
        {
            MOAN("No such queue '%s'\n", argv[i]);
            ret = 1;
        }
        else
        {
            if (strlen(queue->queue_dir))
                checkperm(U_READ|U_WRITE|U_EXEC,queue);
            (void) sprintf(buf,"%s %s %s\n",argv[0],argv[i],sg.username);
            if(!wakeup_natrun(buf))
            {
                if (!delqueue(queue,&anchor))
                {
                    MOAN("Queue %s not deleted\n", argv[i]);
                    ret = 1;
                }
            }
        }
    }
    exit(ret);
}
