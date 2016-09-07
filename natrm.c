/***********************************************************************
 * natrm; SUPERNAT job killer.
 * Program replaces atrm for network queue
 *
 * 2 July 1990 : Added functionality to:
 *                - handle multiple queues
 * 8 May 1992  : Upgraded to version 3.                   
 */ 
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2systems 1989";
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include "supernat.h"

long atol();
/***********************************************************************
 *
 * Functions in this file
 */
short int yyval();

char buf[BUFLEN];  /* useful memory */

/**********************************************************************
 * Main program starts here
 */
int natrm_main(argc,argv,envp)
int argc;
char * argv[];
char * envp[];
{
QUEUE * queue_anchor, *last_queue;
enum no_conf {CN, CY};
enum no_conf no_conf;
enum int_prompt {IN,IY};
enum int_prompt int_prompt;
enum am_daemon {DN,DY};
enum am_daemon am_daemon;
int c;
char * queue_name = NAT;
short int i;
QUEUE * targ;
JOB * job;
/****************************************************
 *    Initialise
 */
    am_daemon = DN;
    if (argc == 1)
    {
(void) printf(
"usage: natrm [-f] [-q queue] [-i] [-] [[job #] [user] ...]\n");
        exit(0);
    }
    natinit(argc,argv);
    queue_anchor = (QUEUE *) NULL;
    last_queue = (QUEUE *) NULL;
    (void) queue_file_open(0,(FILE *) NULL,
                                  QUEUELIST,&queue_anchor,&last_queue);
/*
 * Process the arguments
 */
    int_prompt = IN;
    no_conf = CN;
    while ( ( c = getopt ( argc, argv, "ifq:" ) ) != EOF )
    {
        switch ( c )
        {
        case 'i' :
            int_prompt = IY;
            break;
        case 'f' :
            no_conf = CY;
            break;
        case 'q' :
            queue_name = optarg;
            break;
        case '?' : /* Default - invalid opt.*/
        default:
               (void) fprintf(stderr,"Invalid argument; try man natrm\n");
               exit(1);
        break;
        } 
    }   
    if (optind >= argc)
    {
(void) printf(
"usage: natrm [-f] [-q queue] [-i] [-] [[job #] [user] ...]\n");
        exit(0);
    }
    for (i=optind; i < argc; i++)
       if (!strcmp(argv[i],"-"))
           argv[i] = sg.username;
    
    NEW( QUEUE, targ);

    strcpy(targ->queue_name,queue_name);
    if ((queuetodir(targ,&queue_anchor)) == (char *) NULL)
    {
        (void) fprintf(stderr,"Cannot access queue %s\n",queue_name);
        exit(1);
    }
    checkperm(U_WRITE,targ);       /* does not return if failure */
    if ((!queue_open(targ)))
    {
        perror("cannot open the queue");
        exit (1);
    }
/*
 * Loop through the files in the directory
 */
    while ((job = queue_fetch(targ)) != (JOB *) NULL)
    {
        
        for (i=optind; i < argc; i++)
        {
           if ((strcmp(argv[i],job->job_owner) == 0 ||
               atoi(argv[i]) == job->job_no))
           {
               if (strcmp(job->job_owner,sg.username))
                   if (am_daemon == DN)
                   {
                       checkperm(U_EXEC,targ);
                       am_daemon = DY;
                   }
               break;
           }
        }
        if (i == argc)
            continue;
/*
 * At this point, the job is definitely a candidate for deletion
 */
        if (int_prompt == IY)
        {
            (void) sprintf(buf,"(owned by %s) remove it?",job->job_owner);
            (void) printf(" %5.ld:%36.36s ",job->job_no,buf);
            if ((fgets(buf,BUFLEN,stdin)) == NULL)
                break;
            if (buf[0] != 'y') continue;
        }
        if ( e2unlink(job->job_file))
            perror("remove failed");
        else
        {
            (void) sprintf(buf,"-k %5.ld\n",job->job_no);
            if (!wakeup_natrun(buf))
            {
                sg.read_write = 1;
                deletejob_db(job->job_no);
                if (int_prompt != IY && no_conf != CY)
                    (void) printf(" %5.ld: removed\n",job->job_no);
            }
        }
    }           /* end of loop through the files in the directory */
    queue_close(targ);
    exit(0);
}
