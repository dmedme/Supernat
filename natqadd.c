/************************************************************
 * natqadd.c - program to create a new queue or change an
 * existing queue.
 *
 * There are two forms of input:
 * - Parameters passed on the command line, for backwards compatibility
 * - Input in the requisite format, on stdin. 
 * In either case:
 * - Data in the required format is placed in a file
 * - An attempt is made to signal natrun, and to send it
 *   the command and the name of the file
 *   - If successful, exit
 *   - Otherwise, the updates are done here
 * The action the program takes depends on its link name;
 * natqadd, add.
 * natqchg, amend.
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1991";
/*
 * Copyright (c) E2 Systems 1990. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "supernat.h"

/*
 *    Usage: natqadd queue queue details
 *      or     queue details on stdin
 */
int natqadd_main(argc, argv, envp)
int    argc;
char    **argv;
char ** envp;
{
char  buf[BUFLEN];
QUEUE * lp;
QUEUE ** q_anchor;
int i;
int ret;
FILE * fp;
char * work_file;

    natinit(argc, argv);
    checkperm(U_READ|U_WRITE, (QUEUE *) NULL);
    q_anchor = (QUEUE **) NULL;
    work_file = getnewfile(sg.supernat,"tmp");
    fp = openfile("/",work_file,"w");
    if (fp == (FILE *) NULL)
    {
        (void) fprintf(stderr, "Cannot create work file %s\n", work_file);
        exit(1);
    }
    else
    {
        buf[0] = '\0';
        ret = 0;
        if (argc > 1)
        {
            i = 1;
            (void) fprintf(fp,"DEFINE=%.*s\n",MAXUSERNAME,argv[i]);
            (void) fprintf(fp,"USE=GLOBAL\n");
            (void) fprintf(fp,"END\n");
            (void) fprintf(fp,"AVAILABILITY=\n");
            for (i = 2; i < argc; i++)
            {
                int j;
                j = (i - 2) % 4;
                switch (j)
                {
                case 0:
                    if (i != 2)
                        (void) fprintf(fp,"AVAILABILITY=\n");
                    (void) fprintf(fp,"PRIORITY=%s\n",argv[i]);
                    break;
                case 1:
                    (void) fprintf(fp,"COUNT=%s\n",argv[i]);
                    break;
                case 2:
                    (void) fprintf(fp,"START_TIME=%s\n",argv[i]);
                    break;
                case 3:
                    (void) fprintf(fp,"END_TIME=%s\n",argv[i]);
                    (void) fprintf(fp,"END\n");
                    break;
                }
            }
            if (i == 2 || ((i - 2) % 4) != 0)
                (void) fprintf(fp,"END\n");
        }
        else              /* data is on stdin */
             while ((i= fread(buf,sizeof(char),sizeof(buf),stdin)) > 0)
                 (void) fwrite(buf,sizeof(char),i,fp);
        (void) fclose(fp);
/*
 * At this point, the data is in the work file  
 */
        (void) sprintf(buf,"%s %s %s\n",argv[0],work_file,sg.username);
        if (!wakeup_natrun(buf) &&
            ((!strcmp(argv[0],"natqadd") && !addqueue(work_file, &q_anchor )) ||
            (!strcmp(argv[0],"natqchg") && !chgqueue(work_file, &q_anchor  ))))
        {
            (void) fprintf(stderr, "Queue File Update Failed\n");
            exit(1);
        }
        else
            exit(0);
    }
}
