/***********************************************************************
 * nat; SUN/OS 3.5-compatible at program
 * SUN/OS 4.03 has got the System V at. This program mimics the 
 * SUN/OS 3.5.
 * see man 1 nat for syntax details.
 * Variations are:
 *  - Insists on three letters for days of week and months
 *  - Error messages
 *
 * History:
 *  24 October 1989 : Use $SHELL rather than sh if shell not specified
 *                    Make target directory /usr/spool/nat, as
 *                    requested by Salomon.
 *                    Avoid name collisions.
 *  2 July 1990     : Add execute immediate option
 *                    make sh5 rather than sh default on ULTRIX
 *                    fixed error in mode for file create
 *                    added -q queue option (default nat)
 *  31 July 1990    : Added -j jobname option
 *                          -x "command to execute"
 *  3 January 1991  : Added restart and resubmission options
 *
 *  8 January 1991  : Added Job change options
 *
 * 19 January 1992  : Added:
 *                    -  User Name Change
 *                    -  Group Change
 *                    -  Log File Keep
 *                    -  Conditional actions
 *                    -  Express jobs
 *                    -  User-settable nice
 *                    -  Queue environment variables
 *                    -  Single threaded database updates
 *                    Obsoleted -o flag (which is no longer needed)
 */ 
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1989";
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include "supernat.h"

FILE * fopen();
double floor();

static char buf[BUFLEN];  /* useful memory */
static void get_cal();
/******************************************
 * Main program starts here
 */
int nat_main(argc,argv,envp)
int argc;
char * argv[];
char * envp[];
{

    QUEUE * targ;
    int c;
    FILE * in_chan = stdin;
    time_t cur_time;
    enum shell_seen {NO,SH,CSH};
    enum shell_seen shell_seen;
    enum imm_seen {TIMED,IMMED};
    enum imm_seen imm_seen;
    enum usr_seen {NOUSER,YESUSER};
    enum usr_seen usr_seen;
    enum grp_seen {NOGRP,YESGRP};
    enum grp_seen grp_seen;
    char * jobname;
    char * command;
    struct group * g, *getgrgid();
    char * mail_string="no";
    char * log_keep="no";
    char * express="no";
    char * job_owner;
    char * job_group;
    char * real_group;
    double secs, begin_of_day, cur_double,week_time, 
           file_time;
    char * x;
    static char * end_com_str = SEND_COM_STR;
    char  * queue_name=NAT;
    char * date_string;
    char * restartable="no";
    char * resub="no";
    struct tm * tm_ptr;
    JOB job;
    JOB * cur_job;
    int old_job_no;
    QUEUE * anchor;
    QUEUE * calendar;
    int nice_level;

/****************************************************
 *    Initialise
 */
    anchor = (QUEUE *) NULL;
    calendar = (QUEUE *) NULL;
    shell_seen = NO;
    imm_seen = TIMED;
    usr_seen = NOUSER;
    grp_seen = NOGRP;
    optind=1;
    nice_level = 0;
    command = (char *) NULL;
    jobname = (char *) NULL;
    memset(&job,'\0',sizeof(job));
    old_job_no = 0;
#ifdef MINGW32
    job_group = "everyone";
    real_group = job_group;
#else
    if ((g = getgrgid(getgid())) != (struct group *) NULL)
    {
        job_group = strnsave(g->gr_name, strlen(g->gr_name));
        real_group = job_group;
    }
    else
    {
        static char num_group[11];
        sprintf(num_group,"%d",getgid());
        job_group = num_group;
        real_group = num_group;
    }
#endif
/*
 * Save the execution path, that will be screwed up by the security features
 */
    if ((x=getenv("PATH")) != (char *) NULL)
    {
        (void) sprintf(buf,"NATPATH=%s",x);
        x = strnsave(buf, strlen(buf));
        (void) putenv(x);
    }
    natinit(argc,argv);
    job_owner = sg.username;
/*
 * Process the arguments
 */
    while ( ( c = getopt ( argc, argv, "csmiq:j:x:rt:o:p:u:g:ken:" ) ) != EOF )
    {
        switch ( c )
        {
        case 'c' :
/*
 * Seen a C Shell request
 */
            if (shell_seen != NO)
            {
                (void) fprintf(stderr,
                               "Can only specify the shell once\n");
                exit(1);
            }
            else shell_seen = CSH;
            break;
        case 's' :
/*
 * Seen a Bourne Shell request
 */
            if (shell_seen != NO)
            {
                (void) fprintf(stderr,
                               "Can only specify the shell once\n");
                exit(1);
            }
            else shell_seen = SH;
            break;
        case 'k' :
/*
 * Seen a Request to Keep the log file
 */
            log_keep ="yes";
            break;
        case 'e' :
/*
 * Seen a Request to Express the job
 */
            express ="yes";
            break;
        case 'm' :
/*
 * Seen a Mail request
 */
            mail_string="yes";
            break;
        case 'r' :
/*
 * Seen an indication as to restartability
 */
            restartable="yes";
            break;
        case 'q' :
/*
 * Seen a Queue Name
 */
            queue_name=optarg;
            break;
        case 'o' :
/*
 * Seen an Old Queue Name
 */
            break;
        case 'j' :
/*
 * Seen a Job Name
 */
            jobname=optarg;
            break;
        case 'n' :
/*
 * Seen a Nice level
 */
            nice_level=atoi(optarg);
            break;
        case 'u' :
/*
 * Seen a User Name
 */
            job_owner=optarg;
            usr_seen = YESUSER;
            break;
        case 'g' :
/*
 * Seen a Group Name
 */
            job_group=optarg;
            grp_seen = YESGRP;
            break;
        case 't' :
/*
 * Seen Resubmission details
 */
            resub=optarg;
            break;
        case 'x' :
/*
 * Seen a Single Command with arguments
 */
            command=optarg;
            break;
        case 'p' :
/*
 * Seen an old job number
 */
            old_job_no=atoi(optarg);
            break;
         case 'i' :
/*
 * Seen an instant request
 */
            imm_seen = IMMED;
            break;

        default:
        case '?' : /* Default - invalid opt.*/
               (void) fprintf(stderr,"Invalid argument; try man 1 nat\n");
               exit(1);
        break;
        } 
    }
    NEW(QUEUE,targ);
    STRCPY(targ->queue_name,queue_name);
    if ((queuetodir(targ,&anchor)) == (char *) NULL)
    {
        (void) fprintf(stderr,"Cannot access queue %s\n",queue_name);
        exit(1);
    }
    if (*resub != 'd' && *resub != 'h' && *resub != 'p' &&
        *resub != 'w' && strcmp(resub,"no")
        && (calendar = find_ft(anchor,resub,(char *) NULL,1))
                == (QUEUE *) NULL)
    {
        (void) fprintf(stderr,
"Resubmission option must be hourly, daily, weekly, periodic or a calendar\n");
        exit(1);
    }
    checkperm(U_WRITE,targ);       /* does not return if failure */
    if (old_job_no != 0)
    {
        if ((cur_job = findjobbynumber(&anchor,old_job_no)) == (JOB *) NULL)
        {
            (void) fprintf(stderr,"Cannot access job %d\n",old_job_no);
            exit(1);
        }
/*
 * Check the permissions if the user changing is not the job owner
 */
        if (strcmp(cur_job->job_owner,sg.username))
            checkperm(U_EXEC,cur_job->job_queue);
        job = *(job_clone(cur_job));
/*
 * At this point, the job is definitely a candidate for changing
 */
    }
/*
 * Look for the time of day
 * At the end of this stretch of code, the time is in secs
 */
    if (optind >= argc && imm_seen != IMMED)
    {
        if ( old_job_no == 0)
        {
           (void) fprintf(stderr,"You must provide a time\n");
           exit(1);
        }
    }
    else
    {
    cur_double = local_secs(time(0));
    cur_time = (time_t)  cur_double;
                      /* current time, seconds, local time zone */
    date_string = ctime(&cur_time);
    begin_of_day = floor(cur_double/ (double) 86400.0) * 86400.0;
    week_time = 0.0;
    if (imm_seen == TIMED)
    {
        time_t file_dst_time;
        if (!strcmp("M",argv[optind]) ||
            !strcmp("m",argv[optind]) ||
            !strcmp("1200M",argv[optind]) ||
            !strcmp("1200m",argv[optind]) ||
            !strcmp("12:00M",argv[optind]) ||
            !strcmp("12:00m",argv[optind]) ||
            !strcmp("12M",argv[optind]) ||
            !strcmp("12m",argv[optind]))
            secs = (double) 86400.0;
        else if (!strcmp("N",argv[optind]) ||
            !strcmp("n",argv[optind]) ||
            !strcmp("1200N",argv[optind]) ||
            !strcmp("1200n",argv[optind]) ||
            !strcmp("12:00N",argv[optind]) ||
            !strcmp("12:00n",argv[optind]) ||
            !strcmp("12N",argv[optind]) ||
            !strcmp("12n",argv[optind]))
            secs = (double) 43200.0;
        else if (date_val(argv[optind],"HHMIAM", &x, &secs) == 0)
            if (date_val(argv[optind],"HHAM", &x, &secs) == 0)
                if (date_val(argv[optind],"HH24MI", &x, &secs) == 0)
                    if (date_val(argv[optind],"HH24",& x, &secs) == 0)
                    {
                        (void) fprintf(stderr,"Invalid Time\n");
                        exit(1);
                    }
        optind++;
        if (optind < argc)
            if (date_val(argv[optind],"DY", &x, &week_time) != 0)
                optind++;
            else if (optind < (argc -1))
            {
                (void) sprintf(buf,"%4.4s %s %s",date_string+20,
                   argv[optind],argv[optind+1]);
                if (date_val(buf,"YYYY MON DD", &x, &week_time) !=0)
                {
                    if (week_time < begin_of_day)
                    {
                        (void) sprintf(buf,"%d %s %s",atoi(date_string+20)+1,
                           argv[optind],argv[optind+1]);
                        (void) date_val(buf,"YYYY MON DD",&x,&week_time);
                    }
                    optind++;
                    optind++;
                }
            }
            if (optind < argc && (0 == strcmp("week",argv[optind])))
            {
                secs += (double) 7.0 * (double) 86400.0;
                optind++;
            }
            if (week_time > (double) 0.0)
                file_time = week_time + secs;
            else
                file_time = begin_of_day + secs;
            if (cur_double > file_time)
                file_time += (double) 86400.0;
            file_time = gm_secs((time_t) file_time);
        }
        else     /* Immediate execution */
        {
            file_time = gm_secs((time_t) begin_of_day);
        }
        job.job_execute_time = (time_t) file_time;
    }
/*
 * Set up the channel to read in
 */
    if (command != (char *) NULL)
    {
        if (jobname == (char *) NULL)
            jobname = command;
    }
    if (optind < argc)
    {
        if ((in_chan = fopen(argv[optind],"r")) == (FILE *) NULL)
        {
            (void) perror("Script file open failed");
            exit(1);
        }
        if (jobname == (char *) NULL)
            jobname = argv[optind];
    }
    else if (jobname == (char *) NULL)
       if (old_job_no == 0)
           jobname = "stdin";
       else
           jobname = cur_job->job_name;
    job.job_name = jobname;
    if ((int) strlen(job.job_name) > MAXUSERNAME)
         *(job.job_name + MAXUSERNAME - 1) = '\0';
    job.job_queue = targ;
    job.job_mail = mail_string;
    job.log_keep = log_keep;
    job.nice = nice_level;
    if (command == (char *) NULL)
    {
        if (old_job_no == 0)
            job.job_parms = "";
    }
    else job.job_parms = command;
    job.job_restart = restartable;
    job.job_resubmit = resub;
    job.express = express;
    if (shell_seen ==CSH)
        job.job_shell = "csh";
    else if (shell_seen == NO && old_job_no != 0)
        job.job_shell = cur_job->job_shell;
    else
#ifdef ULTRIX
        job.job_shell = "sh5";
#else
        job.job_shell = "sh";
#endif
    if (old_job_no == 0)
    {
        job.status = JOB_SUBMITTED;
        if (getuid() == 0)
        {
            job.job_owner = job_owner;
            job.job_group = job_group;
        }
        else
        {
            job.job_owner = sg.username;
            job.job_group = real_group;
        }
        if ((int) strlen(job.job_owner) > MAXUSERNAME)
             *(job.job_owner + MAXUSERNAME - 1) = '\0';
        if ((int) strlen(job.job_group) > MAXUSERNAME)
             *(job.job_group + MAXUSERNAME - 1) = '\0';
        if (calendar != (QUEUE *) NULL)
            get_cal(calendar,&job);
        else
            get_cal(targ,&job);
        if (createjob(&job) == 0)
        {
            perror("Job create failed");
            exit(1);
        }
        if (shell_seen == CSH)
            (void) fprintf(job.job_chan,"csh %s",end_com_str);
        else if (shell_seen == SH)
#ifdef ULTRIX
            (void) fprintf(job.job_chan,"sh5 %s",end_com_str);
#else
            (void) fprintf(job.job_chan,"sh %s",end_com_str);
#endif
        else if (getenv("SHELL") == NULL)
#ifdef ULTRIX
            (void) fprintf(job.job_chan,"sh5 %s",end_com_str);
#else
            (void) fprintf(job.job_chan,"sh %s",end_com_str);
#endif
        else
            (void) fprintf(job.job_chan,"$SHELL %s",end_com_str);
        if (command != (char *) NULL)
            (void) fprintf(job.job_chan,"%s\n",command);
        else
        {
            if (in_chan == stdin)
            {
                (void) printf("nat> ");
                (void) fflush(stdout);
            }
            while((fgets(buf,BUFLEN,in_chan)) != NULL)
            {
                (void) fprintf(job.job_chan,"%s",buf);
                if (in_chan == stdin)
                {
                    (void) printf("nat> ");
                    (void) fflush(stdout);
                }
            }
            if (in_chan == stdin)
                (void) printf("<EOT>\n");
            (void) fclose(in_chan);
        }
        (void) fclose(job.job_chan);
        job.job_chan = (FILE *) NULL;
        (void) sprintf(buf,"nat add %s %s\n",targ->queue_name,job.job_file);
        if (! wakeup_natrun(buf))
        {
            sg.read_write = 1;
            createjob_db(&job);
            (void) fprintf(sg.errout,"Accepted Job No:%d\n",
                                              job.job_no);
        }
    }
    else   /* changing an existing job */
    {
        if (getuid() == 0)
        {
            if (usr_seen == YESUSER)
                job.job_owner = job_owner;
            if (grp_seen == YESGRP)
                job.job_group = job_group;
        }
        if ((int) strlen(job.job_name) > MAXUSERNAME)
             *(job.job_name + MAXUSERNAME - 1) = '\0';
        if ((int) strlen(job.job_owner) > MAXUSERNAME)
             *(job.job_owner + MAXUSERNAME - 1) = '\0';
        if ((int) strlen(job.job_group) > MAXUSERNAME)
             *(job.job_group + MAXUSERNAME - 1) = '\0';
        if (calendar != (QUEUE *) NULL)
            get_cal(calendar,&job);
        if (!renamejob(cur_job,&job))
        {
            perror("Job change failed");
            exit(1);
        }
        (void) sprintf(buf,"nat change %s %s %d\n",
                       targ->queue_name,job.job_file,old_job_no);
        if (! wakeup_natrun(buf))
        {
            sg.read_write = 1;
            renamejob_db(&job);
            (void) fprintf(sg.errout,"Changed Job No:%d\n",
                                              job.job_no);
        }
    }
    exit(0);
}
/*
 * Fill in the control information
 */
static void get_cal(calendar,job)
QUEUE * calendar;
JOB * job;
{
    job->j_submitted = find_tok(calendar,SUBMITTED);
                      /* the command executed at submission */
    job->j_held = find_tok(calendar,HELD);
                      /* the command executed on hold */
    job->j_released = find_tok(calendar,RELEASED);
                      /* the command executed at release */
    job->j_started = find_tok(calendar,STARTED);
                      /* the command executed on startup */
    job->j_succeeded = find_tok(calendar,SUCCEEDED);
                      /* the command executed on success */
    job->j_failed = find_tok(calendar,FAILED);
                      /* the command executed on failure */
    job->j_local_env = find_tok(calendar,E2STR);
                      /* local environment definitions */
    return;
}
