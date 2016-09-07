/***********************************************************************
 * natrebuild - program for reconstructing the SUPERNAT database.
 */ 
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1993";
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "supernat.h"

/************************************************************************
 * Main program starts here
 */
int natrebuild_main(argc,argv,envp)
int argc;
char * argv[];
char * envp[];
{
int hash_size;
int extent;
char * db_name;
int job_no;
int c;
QUEUE * queue_anchor, *last_queue;
QUEUE * targ;
JOB * job;
/****************************************************
 *    Initialise
 */
    natinit(argc,argv);
    queue_anchor = (QUEUE *) NULL;
    last_queue = (QUEUE *) NULL;
/*
 * Process the arguments
 */
    checkperm(U_READ|U_WRITE|U_EXEC,(QUEUE *) NULL);
    db_name = sg.supernatdb;
    hash_size = 8192;
    extent = 32768;
    while ((c = getopt(argc, argv, "h:e:n:")) != EOF)
    {
         switch(c)
         {
          case 'h' :              /* Hash Size */
              hash_size = atoi(optarg);
              if (hash_size <= 0)
                  hash_size = 8192;
              break;
          case 'e' :              /* The amount to extend the DB */
              extent = atoi(optarg);
              if (extent <= 0)
                  extent = 32768;
              break;
          case 'n' :              /* The name of the DB */
              db_name = optarg;
              break;
          default:
          case '?' : /* Default - invalid opt.*/
                  (void) fprintf(sg.errout,"Invalid argument,must be:\n\
-n database to access\n\
-h hash size\n\
-e extend size\n");
                  exit(1);
               break;
         }
    }
    (void) queue_file_open(0,(FILE **) NULL,
                                  QUEUELIST,&queue_anchor,&last_queue);

/*
 * Loop through the identified queues to find the maximum job number
 */
    for (targ = queue_anchor,job_no = 0;
            targ != (QUEUE *) NULL;
                targ = getqueueent(targ))
    {
        if (targ->queue_dir[0] == '\0')
            continue;                 /* Not a Queue as such */
        if ((!queue_open(targ))) 
        {
            perror("cannot open the queue");
            exit (1);
        }
/*
 * Loop - process the jobs in this queue to find the highest job number.
 */
        while ((job = queue_fetch(targ)) != (JOB *) NULL)
            if (job_no < job->job_no)
                job_no = job->job_no;
    }
    fhcreate(db_name,hash_size,extent,job_no +1);
    sg.read_write = 1;                  /* We are going to do updates */
    (void) dh_open();                       /* Open the database */
/*
 * Loop through the identified queues, writing out the job records.
 */
    for (targ = queue_anchor,job_no = 0;
            targ != (QUEUE *) NULL;
                targ = getqueueent(targ))
    {
        if (targ->queue_dir[0] == '\0')
            continue;                 /* Not a Queue as such */
        queue_rewind(targ);
/*
 * Loop - process the jobs in this queue
 */
        while ((job = queue_fetch(targ)) != (JOB *) NULL)
            createjob_db(job);
        queue_close(targ);
    }
    (void) fhclose(sg.cur_db);
    exit(0);
}
