/***********************************************************************
 * natqlib; Routines to open, fetch and close a queue
 *                   
 */ 
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2systems 1991";
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

/***********************************************************************
 *
 * Functions in this file
 */
int xcomp();                 /* compares execution time order */
int ccomp();                 /* compares create time order */
/************************************************************************
 * Open a queue to get at the job records
 */
int queue_open(targ)
QUEUE * targ;
{
DIR * job_chan;
JOB * job;
JOB * prev_job;

/****************************************************
 *    Initialise
 */
    if (targ->job_cnt)
        return (1);            /* Queue is already open */
    if (chdir(targ->queue_dir))
    {
        perror("Cannot chdir() to job queue directory");
        return(0);
    }

    if ((job_chan=opendir(targ->queue_dir)) == (DIR *) NULL) 
    {
        perror("cannot open the job directory");
        return (0);
    }
/*
 * Loop through the files in the directory
 */
    targ->job_cnt = 0;
    targ->first_job = (JOB *) NULL;
    targ->job_order = (JOB **) NULL;
    targ->cur_job_ind = (JOB **) NULL;
    while ((job = getnextjob(job_chan,targ)) != (JOB *) NULL)
    {
        if (targ->first_job == (JOB *) NULL)
            targ->first_job = job;
        else
            prev_job->next_job = job;
        prev_job = job;
        job->next_job = (JOB *) NULL;
        targ->job_cnt++;
    }           /* end of loop through the files in the directory */
    closedir(job_chan);
#ifdef AIX_NATIVE
    free(job_chan);
#endif
    if (targ->job_cnt == 0)
        return 1;                 /* all done if nothing found */
    if ((targ->job_order = (JOB **) malloc ( targ->job_cnt * sizeof ( JOB *))) 
               == (JOB **) NULL)
    {
        perror("Failed to allocate space for queue sort");
        return 0;
    }
    
    for ( targ->cur_job_ind = targ->job_order,
          job = targ->first_job;
               job != (JOB *) NULL;
                    job = job->next_job)
        *(targ->cur_job_ind++) = job;
/*
 * Generate the desired order for the queue
 */
    (void) queue_reorder(targ,xcomp);
    return 1;
}
/*
 * Return the next job on the queue
 */
JOB * queue_fetch(targ)
QUEUE * targ;
{
    if (targ->cur_job_ind - targ->job_order >= targ->job_cnt)
        return (JOB *) NULL;
    else
        return *targ->cur_job_ind++;
}
/*
 * Reset to the beginning of the list
 */
void queue_rewind(targ)
QUEUE * targ;
{
    targ->cur_job_ind = targ->job_order;
}
/*
 * Reset to the beginning of the list
 */
void queue_reorder(targ,func)
QUEUE * targ;
int (*func)();
{
    (void) qsort((char *) targ->job_order,
                   targ->job_cnt,
                   sizeof(JOB *),func);
    targ->cur_job_ind = targ->job_order;
    return;
}

/************************************************************************
 * Close a queue; mainly a question of throwing away all the malloc'ed space
 */
void queue_close(targ)
QUEUE * targ;
{
JOB * job, *next_job;
/*
 * Loop through the jobs chained together
 */
    if (targ->job_cnt == 0)
        return;                 /* all done if nothing found */
    
    for ( job = targ->first_job;
               job != (JOB *) NULL;
                    job = next_job)
    {
        next_job = job->next_job;
        job_destroy(job);
        (void) free(job);
    }
    (void) free (targ->job_order);
/*
 * Reset queue values
 */
    targ->job_cnt = 0;
    targ->first_job = (JOB *) NULL;
    targ->job_order = (JOB **) NULL;
    targ->cur_job_ind = (JOB **) NULL;
    return;
}
/*
 * Sort in execution time order, with the express ones coming top
 */
int xcomp(el1,el2)
JOB ** el1, ** el2;
{
    if (*((*el1)->express) == *((*el2)->express))
    {
        if ((*el1)->job_execute_time == (*el2)->job_execute_time)
            return ccomp(el1,el2);
        else if ((*el1)->job_execute_time < (*el2)->job_execute_time)
            return -1;
        else return 1;
    }
    else
    if (*((*el1)->express) == 'y')
            return -1;
    else return 1;
}
/*
 * Sort into creation time order
 */
int ccomp(el1,el2)
JOB ** el1, ** el2;
{
    if ((*el1)->job_create_time == (*el2)->job_create_time) return 0;
    else if ((*el1)->job_create_time < (*el2)->job_create_time) return -1;
    else return 1;
}
