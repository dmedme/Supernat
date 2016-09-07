/***********************************************************************
 * natq; network queue lister
 * Program replaces atq for network queue
 *                   
 */ 
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2systems 1989";
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

long atol();
char * to_char();
/***********************************************************************
 *
 * Functions in this file
 */
static int ecomp();          /* Puts executing jobs at the head of the queue */
int ccomp();                 /* Puts jobs in creation order */

static char buf[BUFSIZ];     /* useful memory */
/************************************************************************
 * Main program starts here
 */
int natq_main(argc,argv,envp)
int argc;
char * argv[];
char * envp[];
{
int job_count;
FILE * in_chan;
enum all_queue {ONE, ALL};
enum all_queue all_queue;
enum count_only {CN, CY, CL};
enum count_only count_only;
enum sort_order {JX,JC};
enum sort_order sort_order;
enum user_only {U0,U1,UN};
enum user_only user_only;
int c;
char * queue_name=NAT;
QUEUE * queue_anchor, *last_queue;
double secs; 
short int i;
QUEUE * targ;
JOB * job;

/****************************************************
 *    Initialise
 */
    natinit(argc,argv);
    queue_anchor = (QUEUE *) NULL;
    last_queue = (QUEUE *) NULL;
    targ = (QUEUE *) NULL;
/*
 * Process the arguments
 */
    sort_order = JX;
    all_queue = ONE;
    count_only = CN;
    while ( ( c = getopt ( argc, argv, "acnq:l" ) ) != EOF )
    {
        switch ( c )
        {
        case 'a' :
            all_queue = ALL;
            break;
        case 'c' :
            sort_order = JC;
            break;
        case 'n' :
            count_only = CY;
            break;
        case 'l' :
            count_only = CL;
            break;
        case 'q' :
            queue_name = optarg;
            break;
        case '?' : /* Default - invalid opt.*/
        default:
               (void) fprintf(stderr,"Invalid argument; try man natq\n");
               exit(1);
        break;
        } 
    }
    if (optind >= argc)
        user_only=U0;
    else
        user_only= (optind == (argc-1)) ? U1 : UN;
    (void) queue_file_open(0,(FILE **) NULL,
                                  QUEUELIST,&queue_anchor,&last_queue);
 
    if (all_queue == ONE)
    {
        NEW( QUEUE, targ);
        strcpy(targ->queue_name,queue_name);
        if ((queuetodir(targ,&queue_anchor)) == (char *) NULL)
        {
            (void) fprintf(stderr,"Cannot access queue %s\n",queue_name);
            exit(1);
        }
    }
    else
        targ = queue_anchor;
/*
 * Main process - Loop through the identified queues
 */

    do
    {
        if (targ->queue_dir[0] == '\0')
            continue;                 /* Not a Queue as such */
        checkperm(U_READ,targ);       /* does not return if failure */
        if ((!queue_open(targ))) 
        {
            perror("cannot open the queue");
            exit (1);
        }
/*
 * Loop through the jobs
 */
        if (user_only == U0 && count_only == CY)
        {     /* Can use the count in the header */

            printf("%s %ld\n",targ->queue_name,targ->job_cnt);
            queue_close(targ);
            continue;
        }
        job_count = 0;
/*
 * Get into the correct sequence
 */
        if (sort_order == JX)
            queue_reorder(targ,ecomp);
        else
            queue_reorder(targ,ccomp);
/*
 * Loop - process the jobs in this queue
 */
        while ((job = queue_fetch(targ)) != (JOB *) NULL)
        {
            if (user_only != U0)
            {
               for (i=optind; i < argc; i++)
                   if (!strcmp(argv[i],job->job_owner))
                       break;
               if (i == argc)
                   continue;
            }
            job_count++;
            if (count_only != CY)
            {
/*
 * Actually want to print this
 */
                if (job_count == 1)
                {                   /* Headings once only */
                    if ((in_chan = fopen(LASTTIMEDONE,"r")) == (FILE *) NULL)
                    {
                        perror("Cannot open lasttimedone");
                        exit(1);
                    }
                    setbuf(in_chan, (char *) NULL);
                    if ((fgets(buf,BUFLEN,in_chan)) > (char*) NULL)
                    {
                        secs = local_secs((time_t) atol(buf));
                        (void) strcpy(buf, to_char("HH24:MI",secs));
                        (void) printf(
                                "\n Queue %s LAST EXECUTION TIME: %s at %s\n",
                                 targ->queue_name,
                                 to_char("Mon DD, YYYY",secs),buf);
                        if (count_only == CL)
                        {
                            (void) printf("\n Queue Definition:\n");
                            queue_print(stdout,targ);
                        }
                        (void) printf(
                                  "\n %-6.6s %-17.17s %-10.10s %-5.5s %-10.10s",
                          "Status","Execution Date","Owner","Job #","Job Name");
                        if (count_only == CL)
                            (void) printf(
      " %-10.10s %-5.5s %-4.4s %-11.11s %-11.11s %-17.17s %-4.4s %-4.4s %-3.3s",
                                          "Group","Shell","Mail","Restartable",
                                          "Periodicity","Create Time","Nice",
                                          "Keep","Exp");
                        (void) printf(" Parameters\n");
                   }
                   else
                   {
                       perror("Cannot read lasttimedone");
                       exit(1);
                   }
                   (void) fclose(in_chan);
                }
                (void) printf("%c%c%3.1d",
                        (*(job->job_file) == 'X') ? 
                        ((*(job->job_file+1) == '0') ? ' ' : 
                        *(job->job_file+1)) :
                            "WRHGSF"[job->status],
                        (*(job->job_file) == 'X') ? '*' : ' ', job_count);
                switch (job_count % 10)
                {
                case 1:
                    (void) printf("st");
                    break;
                case 2:
                    (void) printf("nd");
                    break;
                case 3:
                    (void) printf("rd");
                    break;
                default:
                    (void) printf("th");
                    break;
                }
                secs = local_secs(job->job_execute_time);
                (void) printf(" %-17.17s %-10.10s %5.ld %-10.10s",
                              to_char("DD Mon YYYY HH24:MI",secs),
                              job->job_owner,
                              job->job_no,
                              job->job_name);
                if (count_only == CL)
                {
                    char * x;
                    switch (*(job->job_resubmit))
                    {
                    case 'h':
                        x = "Hourly";
                        break;
                    case 'd':
                        x = "Daily";
                        break;
                    case 'w':
                        x = "Weekly";
                        break;
                    case 'n':
                        x = "No";
                        break;
                    default:
                        x = job->job_resubmit; /* ie. a calendar */
                        break;
                    }
                    secs = local_secs( job->job_create_time);
                   (void) printf(
" %-10.10s %-5.5s %-4.4s %-11.11s %-11.11s %-17.17s %4.1d %-4.4s %-3.3s",
                              job->job_group,
                              job->job_shell,
                              job->job_mail,
                              job->job_restart,
                              x,
                              to_char("DD Mon YYYY HH24:MI",secs),
                              job->nice,
                              job->log_keep,
                              job->express);
                }
                (void) printf(" %s\n", job->job_parms);
            }
        }           /* end of loop through the files in the directory */
        if (count_only == CY)
        {
            printf("%s %ld\n",targ->queue_name,job_count);
        }
        else if (job_count == 0)
        {
            switch (user_only)
            {
            case U0:
                (void) printf("no files in %s queue.\n",targ->queue_name);
                break;
            case U1:
                (void) printf("no files for %s in %s queue.\n",argv[optind],
                               targ->queue_name);
                break;
            case UN:
                (void) printf("no files for specified users in %s.\n",targ->queue_name);
                break;
            }
        }
        queue_close(targ);
        if (all_queue == ONE)
            break;
    } while ((targ = getqueueent(targ)) != (QUEUE *) NULL);
    return(0);
}
/*
 * Comparison routine that puts the currently executing items first
 */

static int ecomp(el1,el2)
JOB ** el1, ** el2;
{
    if ((*el1)->job_file == (*el2)->job_file) /* == (char *) NULL */
        return 0;
    if ((*el1)->job_file == (char *) NULL)
        return 1;
    if ((*el2)->job_file == (char *) NULL)
        return -1;
    if (*((*el1)->job_file) == 'X')
    {
       if (*((*el2)->job_file) != 'X')
          return -1;
       else return xcomp(el1,el2);
    }
    else if (*((*el2)->job_file) == 'X')
          return 1;
    else
        return xcomp(el1,el2);
}
