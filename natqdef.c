/*
 *
 * natqdef.c - Handles Definitions of Queue/Machine details
 *
 * Rules for handling various different types of thing
 * ===================================================
 *
 * The idea is:
 *      -  Default values get defined at the global level
 *      -  Defaults can be over-ridden at:
 *         -   Machine level
 *         -   Queue level
 *
 * Each of these actions can be defined initially, and over-ridden in
 * a particular instance.
 *
 * The actions of the update engine are:
 *      -  Read in the definitions
 *      -  Read in the new values
 *      -  Apply Updates
 *      -  Write out the definitions
 *
 * The grammar for this file is:
 *
 * file := entries
 *
 * entries := entry
 *         |  entries entry
 *
 * entry :==
 *         | env
 *         | def
 *         | avail
 *
 * def :== 'DEFINE=' defname rules 'END'
 * avail :== 'AVAILABILITY=' defname avail_values 'END'
 * defname :== ordname
 *         |   inout
 *
 * inout :== queue_name ':' machine ':' description
 * ordname :== '[A-Za-z_][A-Za-z0-9_]*'
 * queue_name :== '[A-Za-z0-9_]*'
 * machine :== '[A-Za-z_][A-Za-z0-9_]*'
 * description :== '.*'
 * rules :== command_def
 *       |   env
 *
 * command_def :== 'USE=' defname
 *             | 'DIRECTORY=' string
 *             | 'SUBMITTED=' string
 *             | 'RELEASED=' string
 *             | 'STARTED=' string
 *             | 'HELD=' string
 *             | 'SUCCEEDED=' string
 *             | 'FAILED=' string
 *
 * avail_time :== 'COUNT=' number
 *             | 'MEMORY=' number
 *             | 'ALGORITHM=' number
 *             | 'CPU=' number
 *             | 'IO=' number
 *             | 'PRIORITY=' number
 *             | 'START_TIME=' time
 *             | 'END_TIME=' time
 *
 * env:== '[A-Za-z_][A-Za-z0-9_]*=' string
 * string :== '.*'
 * number :== '[0-9][0-9]*'
 *        | '[0-9]*\.[0-9][0-9]*'
 * time :== '[0-9][:.][0-5][0-9]'
 *        | '[0-1][0-9][:.][0-5][0-9]'
 *        | '2[0-3][:.][0-5][0-9]'
 *        | '[0-9][0-5][0-9]'
 *        | '[0-1][0-9][0-5][0-9]'
 *        | '2[0-3][0-5][0-9]'
 *
 * The semantics of the named commands are:
 * -  env       - place in the environment prior to execution
 * -  USE       - inherit from the named definition
 *
 * The following are, in fact, shell commands; there are some shell
 * variables defined to go with them; see below.
 *
 * -  DIRECTORY  - Work directory for this queue.
 *                  This is not actually defined, but generated.
 * -  SUBMITTED  - What to do when (re)-submitted/released.
 * -  RELEASED   - What to do when (re)-submitted/released.
 * -  STARTED    - What to do when started.
 * -  HELD       - What to do when held.
 * -  SUCCEEDED  - What to do if success.
 * -  FAILED     - What to do if failure.
 * 
 * All commands terminate on a line, unless the escape character (\)
 * appears.
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1992";
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#ifndef SCO
#ifndef PTX
#include <strings.h>
#endif
#endif
#include <time.h>
#include <pwd.h>

#include "supernat.h"
#define WORKSPACE 32768

enum look_status {CLEAR, PRESENT};

static struct named_token {
char * tok_str;
enum tok_id tok_id;
} known_toks[] = {{ "DEFINE=", DEFINE},
{"END", END},
{"USE=", USE},
{"SUBMITTED=", SUBMITTED},
{"DIRECTORY=", DIRECTORY},
{"HELD=", HELD},
{"RELEASED=", RELEASED},
{"STARTED=", STARTED},
{"SUCCEEDED=", SUCCEEDED},
{"FAILED=", FAILED},
{"COUNT=",COUNT},
{"AVAILABILITY=", AVAILABILITY},
{"MEMORY=",MEMORY},
{"ALGORITHM=",ALGORITHM},
{"CPU=",CPU},
{"IO=",IO},
{"PRIORITY=",PRIORITY},
{"START_TIME=",START_TIME},
{"END_TIME=",END_TIME},
{"", E2STR}};

static char * tbuf;
static char * tlook;
static enum look_status look_status;
static enum tok_id get_tok ANSIARGS((FILE * fp));
/*
 * Function for comparing AVAIL_TIME structures
 */
static int vcomp(el1,el2)
AVAIL_TIME ** el1, ** el2;
{
    if ((*el1)->resource_limits.low_time ==
        (*el2)->resource_limits.low_time)
        return 0;
    else if ((*el1)->resource_limits.low_time
             < (*el2)->resource_limits.low_time)
        return -1;
    else return 1;
}
/******************************************************************
 * Single threading ensured by a lock flag
 * The routine returns the anchor for a linked list of QUEUE structures
 */

QUEUE * queue_file_open(write_flag,ret_fp,file_to_read,queue_anchor,last_queue)
int write_flag;
FILE ** ret_fp;
char * file_to_read;
QUEUE ** queue_anchor;
QUEUE ** last_queue;
{
FILE *fp;
static char write_buf[BUFSIZ]; /* There is only ever one Queue File
                                * open for writing; therefore use a
                                * single buffer. Otherwise we have a
                                * memory leak.
                                */
char * buf_ptr;

    if ((fp = openfile(sg.supernat, file_to_read, "r+")) == (FILE *) NULL)
        return((QUEUE *) NULL);
    if (*queue_anchor == (QUEUE *) NULL)
        *last_queue = (QUEUE *) NULL;
    else for (*last_queue = *queue_anchor;
                  (*last_queue)->q_next != (QUEUE *) NULL;
                      *last_queue = (*last_queue)->q_next);
    if (!write_flag)
    {
        buf_ptr = (char *) malloc(BUFSIZ);
        setbuf(fp,buf_ptr);
    }
    else
        setbuf(fp,&write_buf[0]);
    if (!queue_file_read(fp,queue_anchor,last_queue))
    {
        (void) fclose(fp);
        if (!write_flag)
            (void) free(buf_ptr);
        return((QUEUE *) NULL);
    }
    if (!write_flag)
    {
        (void) fclose(fp);
        if (ret_fp != (FILE **) NULL)
            *ret_fp = (FILE *) NULL;
        (void) free(buf_ptr);
    }
    else
        *ret_fp = fp;
    return *queue_anchor;
}
/*
 * Function to read the definitions (in Queuelist)
 * fp, queue_anchor and queue_last must have been initialised
 */
int queue_file_read(fp,queue_anchor,queue_last)
FILE * fp;
QUEUE ** queue_anchor;
QUEUE ** queue_last;
{
enum tok_id tok_id;
QUEUE * q;
tbuf = malloc(WORKSPACE);
tlook = malloc(WORKSPACE);
look_status = CLEAR;

    tok_id = get_tok(fp);
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
            ENV * top_env;
        case E2STR:
            top_env = save_env(DO_COMBINE);
            if (top_env != (ENV *) NULL)
                putenv(top_env->e_env);
            break;
        case DEFINE:
            do_define(fp,queue_anchor,queue_last);
            break;
        case AVAILABILITY:
            do_availability(fp,*queue_anchor,*queue_last);
            break;
        case END:
ERROR(__FILE__, __LINE__, 0, "Syntax error in %s END unexpected", QUEUELIST,NULL);
            (void) fclose(fp);
            return 0;
        default:
            ERROR(__FILE__, __LINE__, 0, "Syntax error in %s", QUEUELIST,NULL);
            (void) fclose(fp);
            return 0;
        }
        tok_id = get_tok(fp);
    }
/*
 * We have read in the queue details; now tidy up
 */
    for (q = *queue_anchor;
             q != (QUEUE *) NULL;
                 q = q->q_next)
    {
        AVAIL_TIME * a;
        int i;
        if (q->time_cnt == 0)
            continue;             /* Don't bother if there aren't any */
        for (a = q->first_time;
                 a != (AVAIL_TIME *) NULL;
                     a = a->next_time)
            if (a->resource_limits.low_time > a->resource_limits.high_time)
            {
                AVAIL_TIME * split_ptr;
/*
 * Handle the case when the date range is 18:00 to 8:00 (say)
 */
                q->time_cnt++;
                NEW (AVAIL_TIME, split_ptr);
                *split_ptr = *a;
                split_ptr->resource_limits.high_time = 86400;
                a->resource_limits.low_time = 0;
                a->next_time = split_ptr;
                a = split_ptr;
            }
/*
 * Now get the time details in time order, prior to checking them
 */
        if ((q->time_order = (AVAIL_TIME **)
            malloc ((q->time_cnt)*sizeof(AVAIL_TIME *)))
                     == (AVAIL_TIME **) NULL)
        {
            perror("Failed to allocate space for queue sort");
            return 0;
        }
       
        for ( q->cur_time_ind = q->time_order,
              a = q->first_time;
                    a != (AVAIL_TIME *) NULL;
                         a = a->next_time)
            *(q->cur_time_ind++) = a;
/*
 * Generate the desired order for the availability details
 */
        (void) qsort((char *) q->time_order,
                        q->time_cnt,
                        sizeof(AVAIL_TIME *),vcomp);
        for ( q->cur_time_ind = q->time_order,
              a = *(q->cur_time_ind++),
              i = q->time_cnt -1;
                  i > 0;
                    i --,
                    a = *(q->cur_time_ind++))
        {
            AVAIL_TIME * x_ptr;
            x_ptr = *(q->cur_time_ind);
            if (x_ptr->resource_limits.low_time <
                a->resource_limits.high_time)
            {
                char * x, *x1, *x2, *x3, *x4;
                x = to_char("HH24:MI",(double) a->resource_limits.low_time);
                x1 = strnsave(x,strlen(x));
                x = to_char("HH24:MI",(double) a->resource_limits.high_time);
                x2 = strnsave(x,strlen(x));
                x = to_char("HH24:MI",(double)
                                             x_ptr->resource_limits.low_time);
                x3 = strnsave(x,strlen(x));
                x = to_char("HH24:MI",(double)
                                             x_ptr->resource_limits.high_time);
                x4 = strnsave(x,strlen(x));
                ERROR(__FILE__, __LINE__, 0,
                  "Over-lapping Queue Availabilities. %s - %s Overlaps", x1, x2);
                ERROR(__FILE__, __LINE__, 0,
                  "With Time Period %s - %s", x3, x4);
                free(x1);
                free(x2);
                free(x3);
                free(x4);
                return 0;
            }
        }
        q->cur_time_ind = q->time_order;
    }
    free(tbuf);
    free(tlook);
    return 1;
}
/*
 * Print out a single queue definition
 */
void queue_print(fp,q)
FILE * fp;
QUEUE * q;
{
int i;
ENV  *e;
AVAIL_TIME * a;

    (void) fprintf(fp,"DEFINE=%s\n",q->queue_name);
    (void) fprintf(fp,"DIRECTORY=%s\n",q->queue_dir);
    if (q->q_use != (QUEUE *) NULL)
        (void) fprintf(fp,"USE=%s\n",q->q_use->queue_name);
    if (q->q_submitted != (ENV *) NULL)
    {
        (void) fprintf(fp,"SUBMITTED=");
        print_env(fp,q->q_submitted);
                                    /* the command executed at submission */
    }
    if (q->q_held != (ENV *) NULL)
    {
        (void) fprintf(fp,"HELD=");
        print_env(fp,q->q_held);      /* the command executed on hold */
    }
    if (q->q_released != (ENV *) NULL)
    {
        (void) fprintf(fp,"RELEASED=");
        print_env(fp,q->q_released);  /* the command executed at release */
    }
    if (q->q_started != (ENV *) NULL)
    {
        (void) fprintf(fp,"STARTED=");
        print_env(fp,q->q_started);   /* the command executed on startup */
    }
    if (q->q_succeeded != (ENV *) NULL)
    {
        (void) fprintf(fp,"SUCCEEDED=");
        print_env(fp,q->q_succeeded); /* the command executed on success */
    }
    if (q->q_failed != (ENV *) NULL)
    {
        (void) fprintf(fp,"FAILED=");
        print_env(fp,q->q_failed);    /* the command executed on failure */
    }
/*
 * Print the environment
 */
    for (e = q->q_local_env;
            e != (ENV *) NULL;
                e = e->e_next)
         print_env(fp,e);
    (void) fprintf(fp,"END\n");
/*
 * print the Availability details
 */
    if ( i = q->time_cnt )
    {
        for ( q->cur_time_ind = q->time_order,
              a = *(q->cur_time_ind++);
                  i > 0;
                    i --,
                    a = *(q->cur_time_ind++))
        {
            (void) fprintf(fp,"AVAILABILITY=\n");
            (void) fprintf(fp,"PRIORITY=%d\n",a->prior_weight);
            (void) fprintf(fp,"ALGORITHM=%d\n",a->selection_method);
            (void) fprintf(fp,"COUNT=%d\n",a->max_simul);
            (void) fprintf(fp,"CPU=%.0f\n",a->resource_limits.sigma_cpu);
            (void) fprintf(fp,"IO=%.0f\n",a->resource_limits.sigma_io);
            (void) fprintf(fp,"MEMORY=%.0f\n",a->resource_limits.sigma_memory);
            (void) fprintf(fp,"START_TIME=%s\n",
                          to_char("HH24:MI",
                                  (double) a->resource_limits.low_time));
            (void) fprintf(fp,"END_TIME=%s\n",
                          (a->resource_limits.high_time == 86400) ?
                          "24:00" :
                          to_char("HH24:MI",
                                  (double) a->resource_limits.high_time));
            (void) fprintf(fp,"END\n");
        }
    }
    return;
}
/*
 * Write out the queue details to a given file
 *
 * Passed the anchor
 *
 * Two loops to put the triggers first; quick fix. Should really sort them
 * so that there are no forward references.
 */
void queue_file_write(fp,q)
FILE * fp;
QUEUE * q;
{
QUEUE * sav_q = q;

    while (q != (QUEUE *) NULL)
    {
        if (!strlen(q->queue_dir))
            queue_print(fp,q);
        q = q->q_next;
    }
    q=sav_q;
    while (q != (QUEUE *) NULL)
    {
        if (strlen(q->queue_dir))
            queue_print(fp,q);
        q = q->q_next;
    }
    return;
}
/*
 * Get rid of all the memory stored with a Queue
 * NB. Does not junk the job reference memory.
 * Do not call this on an open queue, unless the purpose is
 * alteration of queue details (in which case, the the job details
 * will be assumed by the replacement).
 */
void
queue_destroy(q)
QUEUE * q;
{
int i;
ENV *f, *e;
JOB * j;
AVAIL_TIME * a;

    if (q->q_machine_queue != (QUEUE_MACHINE *) NULL)
        machine_queue_destroy(q->q_machine_queue);
/*
 * Junk the Availability details
 */
    if (i = q->time_cnt)
    {
        for ( q->cur_time_ind = q->time_order,
              a = *(q->cur_time_ind++);
                  i > 0;
                    i --,
                    a = *(q->cur_time_ind++))
            free(a);
        free(q->time_order);
    }
/*
 * Junk the default actions
 */
    if (q->q_submitted != (ENV *) NULL)
        destroy_env(q->q_submitted); /* the command executed at submission */
    if (q->q_held != (ENV *) NULL)
        destroy_env(q->q_held);      /* the command executed on hold */
    if (q->q_released != (ENV *) NULL)
        destroy_env(q->q_released);  /* the command executed at release */
    if (q->q_started != (ENV *) NULL)
        destroy_env(q->q_started);   /* the command executed on startup */
    if (q->q_succeeded != (ENV *) NULL)
        destroy_env(q->q_succeeded); /* the command executed on success */
    if (q->q_failed != (ENV *) NULL)
        destroy_env(q->q_failed);    /* the command executed on failure */
 
/*
 *  DO NOT junk the queue job list
 *
 *  if (i = q->job_cnt)
 *  {
 *      for ( q->cur_job_ind = q->job_order,
 *            j = *(q->cur_job_ind++);
 *                i > 0;
 *                    i --,
 *                    j = *(q->cur_job_ind++))
 *          job_destroy(j);
 *          free(j);
 *  }
 *  if (q->job_order)
 *      free(q->job_order);
 */
/*
 * Junk the environment
 */
    for (e = q->q_local_env;
            e != (ENV *) NULL;
                f = e->e_next,
                destroy_env(e),
                e = f);
/*
 * Free the Queue structure
 */
    free(q);
    return;
}
/*
 * save an environment command
 */
ENV * save_env(combine_lines)
enum combine_lines combine_lines;
{
ENV * e;
ENV * cont;

    NEW(ENV,e);
    if (combine_lines == DO_COMBINE)
    {
        e->e_env = strnsave(tbuf,strlen(tbuf));
        e->e_cont = (ENV *) NULL;
        e->e_next = (ENV *) NULL;
    } 
    else
    {
        char * x;
        for ( x = strtok(tbuf,"\n"), cont = e;;)
        {
            if (x == (char *) NULL)
                 x = tbuf;
            cont->e_cont = (ENV *) NULL;
            cont->e_next = (ENV *) NULL;
            cont->e_env = strnsave(x,strlen(x));
            if ((x= strtok(NULL,"\n")) == (char *) NULL)
                 break;
            NEW (ENV,cont->e_cont);
            cont = cont->e_cont;
        }
    }
    return e;
}
/*
 * Output an environment command
 */
void print_env(fp,e)
FILE *fp;
ENV * e;
{
    while (e != (ENV *) NULL)
    {
        if (e->e_cont == (ENV *) NULL)
            (void) fprintf(fp,"%s\n",e->e_env);
        else
            (void) fprintf(fp,"%s\\\n",e->e_env);
        e = e->e_cont;
    }
    return;
}
/*
 * Clone an environment command chain.
 */
ENV * clone_env(e)
ENV * e;
{
ENV * cont;
ENV * ret;
ENV * next;

    if (e == (ENV *) NULL)
        return e;
    NEW (ENV, ret);
    *ret = *e;
    for (cont = e,
         next = ret;
               cont != (ENV *) NULL;)
    {
        *next = *cont;
        if (cont->e_env != (char *) NULL)
            next->e_env = strnsave(cont->e_env, strlen(cont->e_env));
        cont = cont->e_cont;
        if (cont != (ENV *) NULL)
            NEW (ENV, next->e_cont);
        next = next->e_cont;
    }
    return ret;
}
/*
 * destroy an environment command
 */
void
destroy_env(e)
ENV * e;
{
ENV * cont;

    for (cont = e; cont != (ENV *) NULL;)
    {
        ENV * next;
        if (cont->e_env != (char *) NULL)
            free(cont->e_env);
        next = cont->e_cont;
        free(cont);
        cont = next;
    }
    return;
}
/*
 * Throw away a chain of Queue structures
 * N.B. Memory leakage will occur if any of the queues have open jobs
 * that are not being executed.
 */
void destroy_queue_list(anchor)
QUEUE * anchor;
{
QUEUE * q;

    for (q = anchor;
             q != (QUEUE *) NULL;
                 anchor = q->q_next,
                 queue_destroy(q), 
                 q = anchor); 
    return;
}
/*
 * Process a Queue definition
 */
void do_define (fp,queue_anchor,queue_last)
FILE * fp;
QUEUE ** queue_anchor;
QUEUE ** queue_last;
{
enum tok_id tok_id;
QUEUE * q;

    tok_id = get_tok(fp);               /* get the definition name */
    NEW (QUEUE,q);
    if (*queue_anchor == (QUEUE *) NULL)  /* chain it to the list    */
    {
        *queue_anchor = q;
    }
    else
    {
        (*queue_last)->q_next = q;
    }
    *queue_last = q;
    
    STRCPY(q->queue_name,tbuf);          /* save the name           */
/*
 * Now, look to see if we have a suffix type animal
 */
    if (strchr(q->queue_name,':') != (char *) NULL)
        q->q_machine_queue = machine_queue_ini();
    else q->q_machine_queue = (QUEUE_MACHINE *) NULL;
/*
 * Now process the options
 */
    tok_id = get_tok(fp);
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
            ENV * top_env;
        case END:
            return;                          /* Finished this one */
        case USE:
            tok_id = get_tok(fp);
            q->q_use = find_def(tbuf,*queue_anchor);
            break;
        case DIRECTORY:
            tok_id = get_tok(fp);
            STRCPY(q->queue_dir,tbuf);
            break;
        case SUBMITTED:
            tok_id = get_tok(fp);
            q->q_submitted = save_env(DONT_COMBINE);
            break;
        case HELD:
            tok_id = get_tok(fp);
            q->q_held = save_env(DONT_COMBINE);
            break;
        case RELEASED:
            tok_id = get_tok(fp);
            q->q_released = save_env(DONT_COMBINE);
            break;
        case SUCCEEDED:
            tok_id = get_tok(fp);
            q->q_succeeded = save_env(DONT_COMBINE);
            break;
        case STARTED:
            tok_id = get_tok(fp);
            q->q_started = save_env(DONT_COMBINE);
            break;
        case FAILED:
            tok_id = get_tok(fp);
            q->q_failed = save_env(DONT_COMBINE);
            break;
        default:
            top_env= save_env(DO_COMBINE);
            if (q->q_last_env == (ENV *) NULL)
            {
                q->q_local_env = top_env; 
            }
            else q->q_last_env->e_next = top_env;
            q->q_last_env = top_env;
            break;
        }
        tok_id = get_tok(fp);
    }
    return;
}
/*
 * Process a Time Period definition
 */
void do_availability (fp,queue_anchor,queue)
FILE * fp;
QUEUE * queue_anchor;
QUEUE * queue;
{
enum tok_id tok_id;
AVAIL_TIME * a;

    QUEUE_MACHINE * machine_queue;
    tok_id = get_tok(fp);                 /* get the definition name */
    if (strlen(tbuf) != 0 )              /* Specific Queue */
    {
        if (strchr(tbuf,':') != (char *) NULL)
        {
            machine_queue = machine_queue_ini();
            queue = find_ft(queue_anchor,machine_queue->queue_name,
                     machine_queue->host_name,1);
            machine_queue_destroy(machine_queue);
        }
        else
            queue = find_ft(queue_anchor,tbuf, (char *) NULL,1);
    }
    if (queue == (QUEUE *) NULL)
        return;                        /* Don't want problems */ 
/*
 * Initialise the AVAIL_TIME with the defaults
 */
    NEW (AVAIL_TIME,a);
    a->own_queue = queue;
    a->max_simul = 1;
    a->resource_limits.sigma_memory = 1000000000.0;
    a->resource_limits.sigma_memory_2 = 0.0;
    a->selection_method = 0;
    a->resource_limits.sigma_cpu = 1000000000.0;
    a->resource_limits.sigma_cpu_2 = 0.0;
    a->max_simul = 1;
    a->resource_limits.sigma_io = 1000000000.0;
    a->resource_limits.sigma_io_2 = 0.0;
    a->resource_limits.sigma_elapsed_time = 0.0;
    a->resource_limits.sigma_elapsed_time_2 = 0.0;
    a->prior_weight = 1;
    a->resource_limits.low_time = 0;
    a->resource_limits.high_time = 86400;
    queue->time_cnt++;
    if (queue->first_time == (AVAIL_TIME *) NULL)
        a->next_time = (AVAIL_TIME *) NULL;
    else
        a->next_time = queue->first_time;
    queue->first_time = a;
/*
 * Now process the options
 */
    tok_id = get_tok(fp);
    sg.chew_glob = tbuf;
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
        case END:
            return;                          /* Finished this one */
        case COUNT:
            tok_id = get_tok(fp);
            a->max_simul = chew_sym(1,CHEW_INT);
            break;
        case MEMORY:
            tok_id = get_tok(fp);
            a->resource_limits.sigma_memory = (double) 
                                              chew_sym(1000000000,CHEW_INT);
            break;
        case ALGORITHM:
            tok_id = get_tok(fp);
            a->selection_method = chew_sym(0,CHEW_INT);
            break;
        case CPU:
            tok_id = get_tok(fp);
            a->resource_limits.sigma_cpu = (double) 
                                              chew_sym(1000000000,CHEW_INT);
            break;
        case IO:
            tok_id = get_tok(fp);
            a->resource_limits.sigma_io = (double) 
                                              chew_sym(1000000000,CHEW_INT);
            break;
        case PRIORITY:
            tok_id = get_tok(fp);
            a->prior_weight = chew_sym(1,CHEW_INT);
            if (strcmp(sg.username,sg.adminname) && strcmp(sg.username,"root"))
                a->prior_weight = 1;
            break;
        case START_TIME:
            tok_id = get_tok(fp);
            a->resource_limits.low_time = chew_sym(0,CHEW_TIME);
            break;
        case END_TIME:
            tok_id = get_tok(fp);
            a->resource_limits.high_time = chew_sym(86400,CHEW_TIME);
            break;
        default:
            return;          /* An error! */
        }
        tok_id = get_tok(fp);
    }
    return;
}
/*
 * Search for a defname
 */
QUEUE * find_def(defname,queue_anchor)
char * defname;
QUEUE * queue_anchor;
{
register QUEUE * ds;

    for (ds = queue_anchor; ds != (QUEUE *) NULL; ds = ds->q_next)
    {
        if (!strcmp(defname,ds->queue_name))
            break;
    }
    return ds;
}
/*
 * Search for a token value
 */
ENV * find_tok(q,tok_id)
QUEUE * q;
enum tok_id tok_id;
{
    if (q == (QUEUE *) NULL)
        return (ENV *) NULL;
    else
    switch(tok_id)
    {
    case SUBMITTED:
        if (q->q_submitted == (ENV *) NULL) 
            return find_tok(q->q_use,tok_id);
        else
            return q->q_submitted;
        break;
    case HELD:
        if (q->q_held == (ENV *) NULL) 
            return find_tok(q->q_use,tok_id);
        else
            return q->q_held;
        break;
    case RELEASED:
        if (q->q_released == (ENV *) NULL) 
            return find_tok(q->q_use,tok_id);
        else
            return q->q_held;
        break;
    case SUCCEEDED:
        if (q->q_succeeded == (ENV *) NULL) 
            return find_tok(q->q_use,tok_id);
        else
            return q->q_succeeded;
        break;
    case STARTED:
        if (q->q_started == (ENV *) NULL) 
            return find_tok(q->q_use,tok_id);
        else
            return q->q_started;
        break;
    case FAILED:
        if (q->q_failed == (ENV *) NULL) 
            return find_tok(q->q_use,tok_id);
        else
            return q->q_failed;
        break;
    case E2STR:
        if (q->q_local_env == (ENV *) NULL) 
            return find_tok(q->q_use,tok_id);
        else
            return q->q_local_env;
        break;
    default:
        ERROR(__FILE__, __LINE__, 0, "Illegal token value %d", (int) tok_id,NULL);
        return (ENV *) NULL;
    }
}
/*
 * Routine to fill the environment as per specification
 */
void fill_env(q)
QUEUE * q;
{
ENV * e;

    if (q->q_use != (QUEUE *) NULL)
        fill_env(q->q_use);
    for (e = q->q_local_env;
             e != (ENV *) NULL;
                  e = e->e_next)
        if (e->e_env != (char *) NULL && ((int) strlen(e->e_env)) > 0)
            putenv(e->e_env);
    return;
}
/*
 * Read the next token
 * -  There are at most two tokens a line
 * -  Read a full line, taking care of escape characters
 * -  Search to see what the cat brought in
 * -  Be very careful with respect to empty second tokens
 * -  Return  
 */
static enum tok_id get_tok(fp)
FILE * fp;
{
int p;
int str_length;
int res;
char * cur_pos;
struct named_token * cur_tok;
/*
 * If look-ahead present, return it
 */
    if (look_status == PRESENT)
    {
        register char * x;
        register char * x1;
        enum tok_id ret_tok;
        ret_tok = E2STR;
        for (x = tlook, x1 = tbuf; *x != '\0'; )
        {
            if (*x == ':')
                ret_tok = QM;
            *x1++ = *x++;
        }
        *x1 = '\0';
        look_status = CLEAR;
        return ret_tok;
    }
    else cur_pos = tbuf; 
    p = getc(fp);
/*
 * Scarper if all done
 */
    if ( p == EOF )
        return PEOF;
    else
/*
 * Pick up the next line, stripping out escapes
 */
    for(;;)
    {
        if (p == (int) '\\')
        {
            p = getc(fp);
            if ( p == EOF )
                p = '\n';
        }
        else if (p == (int) '\n')
            break;
        *cur_pos++ = p;
        p = getc(fp);
        if ( p == EOF )
            p = '\n';
    }
    *cur_pos = '\0';
/*
 * See if we have a token
 */
    if (*tbuf == '#')
        return get_tok(fp);          /* recurse if we meet a comment line */
    else
    for ( cur_tok = known_toks,
         str_length = strlen(cur_tok->tok_str);
             (str_length > 0);
                  cur_tok++,
                  str_length = strlen(cur_tok->tok_str))
    {
        if (*(cur_tok->tok_str + str_length - 1) == '=')
            res = strncmp(tbuf,cur_tok->tok_str,str_length);
        else
            res = strcmp(tbuf,cur_tok->tok_str);
        if (res == 0)
            break;
    }
    if (res != 0)
        return E2STR;
    else
    {
        if (*(cur_tok->tok_str + str_length - 1) == '=')
        {
            look_status = PRESENT;
            strcpy(tlook,tbuf+str_length);
        }
        return cur_tok->tok_id;
    }
}
/*
 * Initialise details of machine and queue
 */
QUEUE_MACHINE * machine_queue_ini()
{
register char * x;
QUEUE_MACHINE * ds;
NEW(QUEUE_MACHINE, ds);

    if (*tbuf == ':')
    {                        /* Null suffix */
        ds->queue_name = strnsave("",0);
        if ((x= strtok(tbuf,":")) == (char *) NULL)
            return (QUEUE_MACHINE *) NULL;          /* skip invalid entries */
    }
    else
    {
        if ((x= strtok(tbuf,":")) == (char *) NULL)
            return (QUEUE_MACHINE *) NULL;          /* skip invalid entries */
        ds->queue_name = strnsave(x,strlen(x));
        if ((x= strtok(NULL,":")) == (char *) NULL)
            return (QUEUE_MACHINE *) NULL;          /* skip invalid entries */
    }
    ds->host_name = strnsave(x,strlen(x));
    if ((x= strtok(NULL,":")) != (char *) NULL)
        ds->description = strnsave(x,strlen(x));
    else ds->description = strnsave("",0);
    return ds;
}
/*
 * Initialise details of machine and queue
 */
void machine_queue_destroy(ds)
QUEUE_MACHINE * ds;
{
    free(ds->queue_name);
    free(ds->host_name);
    free(ds->description);
    free(ds);
    return;
}
/*
 * routine to identify the applicable QUEUE given a name, host and occurrence
 * 
 */
QUEUE * find_ft(queue_anchor,queue_name,host,occurrence)
QUEUE * queue_anchor;
char * queue_name;
char * host;
int occurrence;
{
QUEUE_MACHINE * ds;
QUEUE * q;
int i;

    for (i = 0, q = queue_anchor;
            q != (QUEUE *) NULL;
                 q = q->q_next)
    {
        if (q->q_machine_queue != (QUEUE_MACHINE *) NULL)
        {
            ds = q->q_machine_queue;
            if ((queue_name == (char *) NULL ||
                 !strcmp(queue_name,ds->queue_name))
            && (host == (char *) NULL || !strcmp(host,ds->host_name)))
            {
                i++;
                if (i == occurrence)
                    return q;
            }
        }
        else
        {
            if (queue_name != (char *) NULL &&
                 !strcmp(queue_name,q->queue_name))
            {
                i++;
                if (i == occurrence)
                    return q;
            }
        }
    }
    return (QUEUE *) NULL;
}
