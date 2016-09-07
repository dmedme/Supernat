/***********************************************************************
 * natrun; multiple queue despatcher.
 * Program replaces atrun as a batch execution process, and replicates
 * its functionality, with the following deviations.
 *   - there is a top-level loop; the program awaits events rather than dies
 *     when there is nothing to do
 *   - it works
 *   - it takes from the Queuelist file 
 *     a list of queue availabilities for batch work
 *   - it loops through the queue directories, looking for
 *     files past their 'sell-by' date
 *   - if none found, it awaits events
 *   - if any found:
 *      - the file is moved to /usr/spool/nat/past
 *      - if there are no eligible processors, it sleeps until there
 *        is one
 *      - it opens the file and unlinks it 
 *      - when executing the job, it finds the lightest loaded available
 *        processor (assuming that CPU load is the rate-determining
 *        step)
 *      - it then issues an rcmd() for
 *        the correct user for each processor, until success or all
 *        failed
 *      - if it fails, it outputs details to stderr (no valid processor)
 *      - it mails the user if the user asked for it
 *      - otherwise, it sends the file down the input port, and sends output
 *        to stdout and stderr, as necessary
 *      - if mail was requested, stdout and stderr also go to a temporary
 *        file
 *      - There is a problem; really want to use two channels, so that we
 *        can close the input one without closing the output. Since we
 *        cannot find a way of shutting down the socket in one direction
 *        only, output a dummy extra command to send back an unlikely string
 *        to recognise.
 *
 * If DEBUG is defined, the process is 'one shot'; no problems
 * with dbx and fork(). It also gives some extra diagnostics.
 *
 * History:
 *  2 July 1990 - hacked from ndes v.1.2; ultrix multi-queue despatcher
 *                   
 */ 
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 1990";
static char * release_level ="Supernat Version 3.3.7";
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef SOLAR
#include <sys/un.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#ifndef SCO
#ifndef PTX
#include <strings.h>
#endif
#endif
#ifdef PYR
#define WTERMSIG(x) x.w_T.w_Termsig
#define WEXITSTATUS(x) x.w_T.w_Retcode
#endif
#ifdef SCO
#include <sys/fcntl.h>
#endif
#ifdef PTX
#include <sys/fcntl.h>
#endif
#include <setjmp.h>
#include "supernat.h"
#include "natcexe.h"
#ifdef SOLAR
#include <sys/stropts.h>
#include <sys/stream.h>
#ifdef DEBUG
/*************************************************
 * Dump off the streams modules loaded. Only used to
 * find out what is there, during set up
 */
static void strmdump(fd)
int fd;
{
    struct str_mlist str_names[20];
    struct str_list s_feed;
    int i;
    s_feed.sl_nmods = sizeof(str_names)/sizeof(struct str_mlist);
    s_feed.sl_modlist = str_names;
    for ( i=ioctl(fd,I_LIST,&s_feed),
          ((i>-1)?(i=s_feed.sl_nmods):(-1)), i--; i > -1; i--)
    {
         fprintf(stderr,"%s\n",str_names[i].l_name);
    }
    return;
}
#endif
#endif
int icomp();                 /* hash comparison function                   */
/***********************************************************************
 *
 * Functions in this file
 */
static int lcomp();          /* compares processor loads                   */
void ret_mail();             /* returns mail to the user                   */
void ini_queue();            /* reads in queue array                       */
static void dump_eli();      /* lists out the eligible queues              */
static void dump_children(); /* lists out the currently executing jobs     */
void do_things();            /* process requests whilst things are still alive
                              */
void add_child();            /* administer the spawned processes
                              */
void rem_child();
RUN_CHILD * find_job();

FILE * in_chan;
enum poll {POLL,NOPOLL};
/*****************************************************************************
 *  Future event management
 */
static time_t go_time[MAXQUEUES];
static short int head=0, tail=0, clock_running=0, timer_exp=0;

void reset_time();           /* reset the clock system */
void add_time();             /* manage a circular buffer of alarm clock calls */
void rem_time();
void do_one_job();           /* child process handles one job */
static QUEUE **pick_queue(); /* identifies eligible queues; returns
                                a pointer to an array of queue names,
                                in descending order */
#define DEFPRIOR 1
#define SIMUL 3
                             /* default number of children */


static QUEUE * queue_anchor, *queue_last, * eli_queues[MAXQUEUES];
                             /* list of queues in the input file */
    

static HASH_CON * child_pid_tab;
static HASH_CON * child_job_tab;

static short int glob_simul = SIMUL;
 
#ifdef NETVERS
static     u_short inport;
#endif

static char buf[BUFSIZ];  /* useful memory */
void scarper();         /* exit, tidying up */
void die();             /* kill a single job */
void chld_sig();        /* catch the death of a child */
void chld_catcher();    /* reap children */
void proc_args();       /* process arguments */
int fifo_open();        /* open the fifo */
void fifo_check();      /* see what's doing */

static int child_cnt;
#ifdef NETVERS
   char  host_name[32]= "localhost"; 
#endif
                                   /* Global so that scarper() can remove it */
void crash_out();

int natrun_main(argc,argv,envp)
int argc;
char * argv[];
char * envp[];
{
#ifdef NETVERS
    struct servent *getservbyname(),
                       *rsh;
#endif

/****************************************************
 *    Initialise
 */
    (void) putenv("PATH=");               /* because this is run as root */
    (void) putenv("IFS=\r\n\t ");         /* likewise */
    child_cnt = 0;
    natinit(argc,argv);                   /* register ourselves */
#ifdef NETVERS
    if ((rsh=getservbyname("shell","tcp"))==(struct servent *) NULL)
    {
        char * x="rsh tcp service not defined; aborting";
        (void) fprintf(sg.errout,"rsh tcp service not defined\n");
        natlog(1,&x);
        exit(1);
    }
    inport = rsh->s_port;
#endif
    (void) sigset(SIGHUP,SIG_IGN);
    (void) sigset(SIGBUS,crash_out);           /* Try and close the files */
    (void) sigset(SIGSEGV,crash_out);          /* Try and close the files */
    (void) sigset(SIGTERM,scarper);
    (void) sigset(SIGCHLD,chld_sig);
/*
 * Allocate hash tables for the children tracking
 */
    child_pid_tab = hash(MAXQUEUES * SIMUL,long_hh,icomp);
    child_job_tab = hash(MAXQUEUES * SIMUL,long_hh,icomp);
    proc_args(argc,argv);
/*
 * Get the times off the Queue file.
 */
    if(!queue_file_open(0,(FILE **) NULL,QUEUELIST,&queue_anchor,&queue_last))
    {
       char * x ="Failed to open the Queue List; aborting";
       (void) fprintf(sg.errout,"Error: %d\n",errno);
       perror("Queuelist open");
       natlog(1,&x);
       exit(1);
    }
    sg.read_write = 1;                      /* We are going to do updates */
    (void) dh_open();                       /* Open the database */
    fhdomain_check(sg.cur_db,0,rec_purge);
/*
 * Main processing:
 *  - two modes, active and passive
 *  - whilst active:
 *    - scan queues in priority sequence looking for things to do
 *    - sleep if no queues are available
 *    - wait for a process termination if global maximum reached
 *    - move on to the next queue if a queue maximum reached
 *    - sleep if something is queued which is not yet due
 *    - wait if no global maximum, but the queues have met local maxima
 *    - leave active mode when
 *      - nothing in the queues at all
 *   - whilst passive:
 *      - wake up if prodded
 *      - prods can come from nat (submission) or natrm (remove)
 *        or natqadd or natqdel
 *  The design problem is to get the thing to:
 *   - respond quickly whether active or passive
 *   - be sensitive to the march of time (which will throw out calculations
 *     as to priorities etc.)
 *   - not to use too many resources looking for work to do
 *   - not to leave too many jobs waiting; want to reap all that are done
 *     at the points we are waiting for something to happen.
 *  The answer would seem to be a general event handler, where an event is:
 *   - the addition of a job to the queue system
 *   - the death of a child process
 *   - the expiry of some significant timer (either job related or priority
 *     related)
 *   - the removal of a job(?)
 *   - an instruction to reload the queue definitions; this needs careful
 *     handling, since the child processing also needs to be reset at this
 *     point
 *   - an instruction to shut down
 *  Wherever there is a call to wait, or sleep, the event handler should be
 *  called instead. Perhaps do a spot of multi-threading?
 */
    (void) umask(07);         /* allow group to submit */
#ifdef SOLAR
    if ((sg.listen_fd = fifo_listen(sg.fifo_name)) < 0)
    {
        char * x=
    "Failed to open the FIFO listen socket; aborting";
        (void) fprintf(sg.errout,"Error: %d\n",errno);
        perror("Cannot Open FIFO using File descriptor");
        natlog(1,&x);
        (void) e2unlink(sg.fifo_name);
        exit(1);
    }
#else
#ifdef MINGW32
    if ((sg.listen_fd = fifo_listen(sg.fifo_name)) < 0)
    {
        char * x=
    "Failed to open the FIFO listen socket; aborting";
        (void) fprintf(sg.errout,"Error: %d\n",errno);
        perror("Cannot Open FIFO using File descriptor");
        natlog(1,&x);
        (void) e2unlink(sg.fifo_name);
        exit(1);
    }
#else
    if (mkfifo(sg.fifo_name,0010660) < 0)
    { /* create the input FIFO */
        char * x ="Failed to create the FIFO; aborting";
        (void) fprintf(sg.errout,"Error: %d\n",errno);
        perror("Cannot Create FIFO");
        natlog(1,&x);
        exit(1);
    }
#endif
#endif
    (void) chown(sg.fifo_name,getadminuid(),getadmingid());
                                /* let nat wakeup */
    do_things();                /* process requests until nothing to do
                                 * DOES NOT RETURN
                                 */
    exit(0);                  /* Keeps lint happy */
}
/*
 * Exit, tidying up
 */
void scarper()
{
    char * x ="Termination Request Received; shutting down";
    (void) e2unlink(sg.fifo_name);
    (void) e2unlink(sg.lock_name);
    (void) fprintf(sg.errout,"Termination Request Received\n");
    natlog(1,&x);
    if (sg.cur_db != (DH_CON *) NULL)
        (void) fhclose(sg.cur_db);
    exit(0);
}
void abort();
/*
 * Exit, tidying up
 */
void crash_out()
{
    char * x ="Fatal Error; shutting down";
    sigset(SIGSEGV,abort);
    sigset(SIGBUS,abort);
    (void) e2unlink(sg.fifo_name);
    (void) e2unlink(sg.lock_name);
    (void) fprintf(sg.errout,"Killed by signal\n");
    natlog(1,&x);
    if (sg.cur_db != (DH_CON *) NULL)
        (void) fhclose(sg.cur_db);
    abort();
}
/*
 * Open the fifo, and get its file descriptor (used for select)
 */

#ifndef SOLAR
#ifndef MINGW32
int fifo_open()
{
    sg.fifo_fd=open(sg.fifo_name,O_RDONLY ,0 );
    if (sg.fifo_fd < 0)
    {
        if (errno != EINTR)
        {
            char * x = "Failed to open the FIFO; aborting";
            (void) fprintf(sg.errout,"Error: %d\n",errno);
            perror("Cannot Open FIFO");
            natlog(1,&x);
            (void) e2unlink(sg.fifo_name);
            exit(1);
        }
    }
    else
    if ((sg.fifo=fdopen(sg.fifo_fd,"r")) == (FILE *) NULL)
    {
        char * x="Failed to open the FIFO using the File descriptor; aborting";
        (void) fprintf(sg.errout,"Error: %d\n",errno);
        perror("Cannot Open FIFO using File descriptor");
        natlog(1,&x);
        (void) e2unlink(sg.fifo_name);
        exit(1);
    }
    return sg.fifo_fd;
}
#endif
#endif
/*
 * Process arguments, including initial ones
 */
void proc_args(argc,argv)
int argc;
char ** argv;
{
int c;
/*
 * Process the arguments
 */
    char err_buf[BUFSIZ];
    FILE * errout_save;
    errout_save =  sg.errout;

    while ( ( c = getopt ( argc, argv, "s:hdjk:e:t" ) ) != EOF )
    {
        switch ( c )
        {
        case 'h' :
            (void) fprintf(sg.errout,"natrun: job despatcher\n\
                You ought to put this program in /etc/rc\n\
                Options:\n -h prints this message and HALTS;\n\
 -t timestamp the log output\n\
 -s maximum simultaneous processes\n ");
            scarper();
            break;
        case 's':
        {
            char * x[2];
            x[0]="New Simultaneous Job Limit";
            x[1] = optarg;
            natlog(2,x);
            if ((glob_simul = atoi(optarg)) == 0)
                (void) fprintf(sg.errout, "Batch System Suspended");
            break;
        }
        case 'k':
            {
               int job_no;
               if ((job_no = atoi(optarg)) != 0)
               {      
                   RUN_CHILD * kill_job;
                   if ((kill_job = find_job(job_no))
                             != (RUN_CHILD *) NULL)
                   {
                       (void) kill(kill_job->child_pid,SIGUSR1);
                       fprintf(sg.errout,
                               "Running %s: removed\n",optarg);
                   }
                   else   /* Not running; finish cleanup */
                   {
                       deletejob_db(job_no);
                       fprintf(sg.errout,
                               " %s: removed\n",optarg);
                   }
               }
               else
               {
                   fprintf(sg.errout,"natrm: Invalid Job Number %s\n",optarg);
               }
            }
            break;
        case 'e':
/*
 * Change the disposition of error output
 */
               sighold(SIGALRM);
               sighold(SIGCHLD);
               sigset(SIGPIPE,chld_sig);
#ifdef SOLAR
               if ((sg.errout = fdopen(sg.fifo_fd,"w")) == (FILE *) NULL)
               {
                   perror("Response Connect() failed");
                   sg.errout = stderr;
               }
               else
                   setbuf(sg.errout,err_buf);
#else
#ifdef MINGW32
               if ((sg.errout = fdopen(sg.fifo_fd,"wb")) == (FILE *) NULL)
               {
                   perror("Response Connect() failed");
                   sg.errout = stderr;
               }
               else
                   setbuf(sg.errout,err_buf);
#else
               if ((sg.errout = fopen(optarg,"w")) == (FILE *) NULL)
               {
                   perror("Response FIFO open failed");
                   sg.errout = stderr;
               }
               else
                   setbuf(sg.errout,err_buf);
#endif
#endif
               sigrelse(SIGALRM);
               sigrelse(SIGCHLD);
            break;
        case 'd':
            queue_file_write(sg.errout,queue_anchor);
            dump_eli();
            dump_children();
            break;
        case 't':
            sg.stamp_out = 1;
            break;
        case 'j':
/*
 * Process the Job details, as per nat
 */
            timer_exp++;       /* tell the main control that something has
                                * happened
                                */
            break;
        default:
        case '?' : /* Default - invalid opt.*/
               (void) fprintf(sg.errout,"Invalid argument; try -h\n");
        break;
        } 
    }
/*
 * Process a nat, natcalc, naterase natqadd, natqchg or natqdel command
 */
    if (optind < argc - 2)
    {
        timer_exp++;       /* tell the main control that something has
                                * happened
                                */
        if ((optind < argc - 3) && !strcmp(argv[optind],"nat"))
        {
            QUEUE * q;
            JOB * job;
            if ((q = find_ft(queue_anchor,argv[optind+2],
                             (char *) NULL,1)) != (QUEUE *) NULL)
            {
                if ((job = getjobbyfile(q,argv[optind+3])) !=
                     (JOB *) NULL)
                {
                    if (!strcmp(argv[optind+1],"add"))
                    {
                        createjob_db(job);
                        (void) fprintf(sg.errout,"Accepted Job No:%d\n",
                                              job->job_no);
                    }
                    else
                    if (!strcmp(argv[optind +1],"change"))
                    {
                        RUN_CHILD * chg_job;
                        if ((chg_job = find_job(job->job_no))
                                  != (RUN_CHILD *) NULL)
                        {       /* Currently executing */
                            JOB *j;
                            j = chg_job->own_job;
                            chg_job->own_job = job;
                            renamejob_db(job);
                            job = j;
                        }
                        else
                            renamejob_db(job);
                        (void) fprintf(sg.errout,"Changed Job No:%d\n",
                                              job->job_no);
                    }
                    job_destroy(job); 
                    free(job);
                }
                else
                    (void) fprintf(sg.errout,
                             "Could not find Job File %s in Queue named:%s\n",
                                              argv[optind+3],
                                              argv[optind+2]);
            }
            else
                (void) fprintf(sg.errout,"Could not find Queue named:%s\n",
                                              argv[optind+2]);
        }
        else if (optind < argc - 2)
        {
            (void) free(sg.username);
            (void) free(sg.groupname);
            sg.username = strnsave(argv[optind+2],(int) strlen(argv[optind+2]));
            sg.groupname = getgroupforname(sg.username);
            if (!strcmp(argv[optind],"natqadd"))
            {
              if (addqueue(argv[optind+1], &queue_anchor ))
                 (void) fprintf(sg.errout, "Queue File Addition Succeeded\n");
              else
                 (void) fprintf(sg.errout, "Queue File Addition Failed\n");
            }
            else
            if (!strcmp(argv[optind],"natqchg"))
            {
                if (chgqueue(argv[optind+1], &queue_anchor  ))
                    (void) fprintf(sg.errout, "Queue File Update Succeeded\n");
                else
                    (void) fprintf(sg.errout, "Queue File Update Failed\n");
            }
            else
            if (!strcmp(argv[optind],"natqdel"))
            {
                QUEUE * q;
                NEW(QUEUE,q);
                strcpy(q->queue_name,argv[optind+1]);
                if ((queuetodir(q,&queue_anchor) == (char *) NULL) ||
                     !delqueue(q, &queue_anchor ))
                    (void) fprintf(sg.errout, "Queue Delete Failed\n");
                else
                    (void) fprintf(sg.errout, "Queue Delete Succeeded\n");
                (void) free(q);
            }
            else if (!strcmp(argv[optind],"natcalc"))
            {
                FILE * fp;
                if ((fp = fopen(argv[optind+1],"r")) != (FILE *) NULL)
                {
                    setbuf(fp, buf);
                    if ( natcalc(fp, (char *) NULL,1,(JOB *) NULL))
                        (void) fprintf(sg.errout, "SUCCESS\n");
                    else
                        (void) fprintf(sg.errout, "FAILURE\n");
                    (void) fclose(fp);
                    (void) e2unlink(argv[optind+1]);
                }
            }
            else if (!strcmp(argv[optind],"naterase"))
            {
                int i;
                natcalc_dh_con = sg.cur_db;
                for (i = optind + 2; i < argc; i++)
                     varerase(argv[i]);
            }
        }
    }
/*
 * Restore the error file pointer
 */
    if (sg.errout != errout_save)
        (void) fclose(sg.errout);
    sg.errout = errout_save;
    return;
}
/*
 * Check the FIFO for something to do
 */
void fifo_check(poll)
enum poll poll;
{
char * so_far;
char * fifo_args[14];                           /* Dummy arguments to process */
char fifo_line[BUFSIZ];                         /* Dummy arguments to process */
int read_cnt;                                 /* Number of bytes read */
        register char * x;
        short int i;
        struct stat stat_buf;
/*
 * Is there anything doing on the FIFO front?
 */
        chld_catcher(WNOHANG);                  /* pick up children */
        if (stat(sg.lock_name,&stat_buf) < 0 && (poll == POLL || timer_exp))
            return;                         /* still things to do */
#ifdef SOLAR
        if ((sg.fifo_fd = fifo_accept(sg.supernat, sg.listen_fd)) < 0)
            return;
#else
#ifdef MINGW32
        if ((sg.fifo_fd = fifo_accept(sg.supernat, sg.listen_fd)) < 0)
            return;
#else
        if (( sg.fifo_fd = fifo_open()) < 0)
            return;
                         /*  get back to a state of readiness */
#endif
#endif
/*
 * There is
 */
        sighold(SIGCHLD);
        sighold(SIGALRM);
        for (read_cnt = 0,
             x=fifo_line,
             so_far = x;
                (x < (fifo_line + sizeof(fifo_line) - 1)) &&
                ((read_cnt = read (sg.fifo_fd,x,
                   sizeof(fifo_line) - (x - fifo_line) -1))
                      > 0);
                        x += read_cnt)
#ifdef SOLAR
        {
            char * x1, *x2;
            x2 = x + read_cnt;
            *x2 = '\0';                  /* put an end marker */
            for (;so_far < x2; so_far = x1+1)
                if ((x1=strchr(so_far,(int) '\n')) == (char *) NULL)
                     break;
                else
                {
                    *x1='\0';
                    if (!strcmp(so_far,LOOK_STR))
                    {
                        *so_far = '\0';
                        x = so_far;
                        goto pseudoeof;
                    }
                    else
                    {
                        *x1='\n';
                         so_far = x1;
                    }
                }
        }
pseudoeof:
#ifndef PTX4
        if (errno)
            perror("User Request");
#endif
#ifdef DEBUG
        fprintf(stderr,"fifo_line:\n%s\n",fifo_line);
        fflush(stderr);
#endif
#else
           ;
#endif
        sigrelse(SIGCHLD);
        sigrelse(SIGALRM);
#ifndef SOLAR
#ifndef MINGW32
        (void) fclose(sg.fifo);
#endif
#endif
        *x = '\0';                             /* terminate the string */
        (void) e2unlink(sg.lock_name);

/*
 * Process the arguments in the string that has been read
 */
        if ((fifo_args[1]=strtok(fifo_line,"  \n"))==NULL) return;
/*
 * Generate an argument vector
 */
        for (i=2;
                (i < 14 && (fifo_args[i]=strtok(NULL," \n")) != (char *) NULL);
                      i++);
 
        fifo_args[0] = "";       /* Work-around for AIX V.4.3  */
        opterr=1;                /* turn off getopt() messages */
        optind=1;                /* reset to start of list     */
        proc_args(i,fifo_args);
        return;
}
/*
 * chld_sig(); interrupt a read or whatever.
 */
void chld_sig()
{
   timer_exp++;
   return;
}
/*
 * death_sig(); interrupt a read or whatever.
 */
void death_sig()
{
   sg.sstate = DIED;
   return;
}
/*
 * chld_catcher(); reap children as and when
 */
void chld_catcher(hang_state)
int hang_state;
{
   int pid;
#ifdef PYR
union wait
#else
int
#endif
       pidstatus;
   struct rusage res_used;
   (void) sigset(SIGCHLD,SIG_DFL); /* Avoid nasties with the SIGCHLD/wait3()
                                       interaction */
   while ((pid=wait3( &pidstatus, hang_state,&res_used)) > 0)
   {
       timer_exp++;
       rem_child(pid,pidstatus,&res_used);
   }
   (void) sigset(SIGCHLD,chld_sig); /* re-install */
   return;
}
/*
 * die(); terminate a sub-process fairly immediately; used to kill off
 * a job with SIGUSR1. 
 */
void die()
{
    sg.sstate = KILLED;    /* signal death to child */
    return;
}
static
#ifdef MINGW32
void
#else
int
#endif
spawn_child(run_job)
JOB * run_job;
{
FILE * out_file;
int pid;
#ifdef NETVERS
short int rem;
int fd2;                        /* remote file descriptors */
#else
int rem_out;
int rem_in;
int rem_err;
#endif

#ifndef MINGW32
    if ((pid=fork()) == 0)
/*
 * CHILD
 */
    {
#endif
    char * fname;
#ifdef NETVERS
    char ** rcmd_host_ptr;
    char * rcmd_host = host_name;
    rcmd_host_ptr = &rcmd_host;

        fd2 = -1;
        if((rem = rcmd(rcmd_host_ptr, inport, run_job->job_owner,
                       run_job->job_owner, "sh",
                           &fd2)) < 1)
        {    /* make the connexion */
            (void) fprintf(sg.errout,"Calling %s at host %s\n",
                           job_owner,host_name);
            perror("Failed to connect to the rsh service");
            exit(1);
        }
#else
/*
 * Call a routine to exec a sub-process owned by the person in question,
 * and connected to the passed file descriptors
 */
    static char *  penvp= (char *) NULL;
#ifdef MINGW32
#define COMMAND_PROCESSOR "C:\\WINNT\\SYSTEM32\\CMD.EXE"
    static char *  pargv[2];
    if (pargv[0] == (char *) NULL)
    {
        if ((pargv[0] = getenv("COMSPEC")) == (char *) NULL)
            pargv[0] = COMMAND_PROCESSOR;
    }
#else
#ifdef SCO
    static char *  pargv[2]={{"sh"}, { NULL }};
#else
    static char *  pargv[2]={"sh",  NULL };
#endif
#endif

        pid = lcmd(&rem_in,&rem_out,&rem_err,
                 run_job->job_owner,run_job->job_group,"/bin/sh",pargv,&penvp);
                 /* spawn the sub-shell */
        if ( rem_in == -1)
        {   
        char * x = "Failed to connect to the sub-shell";

            perror("Failed to connect to the sub-shell");
            natlog(1,&x);
            exit(1);
       }

#endif
        out_file = log_file_open(run_job,&fname);
        (void) free(fname);
        (void) sigset(SIGUSR1,die);
        (void) sigset(SIGTERM,die);
        (void) sigset(SIGCHLD,death_sig);
        do_one_job(out_file,
                   rem_in,
                   rem_out,
                   rem_err,
                   run_job,
                   pid); /* does not return */
#ifdef MINGW32
        return;
#else
    }
    else
        return pid;
#endif
}
/*
 * Function to handle requests, honouring simultaneity limits, until there
 * is absolutely nothing more to do for the moment
 */
void do_things()
{
QUEUE ** queue_list,** cur_queue;
static char new_name[MAXUSERNAME];

FILE * out_file;
time_t cur_time;
JOB * cur_job;

int i;
/*
 * Process forever (death is by signal SIGTERM)
 *
 * This is the look-for-work loop
 * - Should always look for the highest priority queue after an event.
 * - Need to find all submitted jobs for the queue being processed.
 */
    for (;;)
    {
/*
 *  Each pass, identify the eligible queues in priority sequence
 */
        queue_list = pick_queue();
        timer_exp = 0;               /* to see if anything has happened
                                      * whilst the loop is going on
                                      */
 /*
  * Loop - process the queues in turn
  */
        for ( cur_queue = queue_list;
                  *cur_queue != (QUEUE *) NULL && child_cnt < glob_simul;
                     cur_queue++)
        {
        int pid;
        JOB * run_job;

            if ((*cur_queue)->max_simul <= (*cur_queue)->cur_simul)
                continue;

            cur_time = 0;
            if ((!queue_open(*cur_queue)))
            {
            char * x[2];
            x[0] ="Queue open failed";
            x[1] = (*cur_queue)->queue_name;

                perror("cannot open the queue");
                natlog(2,x);
                continue;
            }
/*
 * Loop through the files in the directory
 */
            while ((cur_job = queue_fetch(*cur_queue)) != (JOB *) NULL)
            {
/*
 * Get the job details off the spool file and from elsewhere
 *
 * If successful, fork
 *
 * At start up:
 * - Find all jobs with STARTED status.
 * - Either zap them or resubmit
 * - Mail regardless. 
 */
                if (*(cur_job->job_file) == 'X')       /* already executing */
                {
                    if (find_job(cur_job->job_no) == (RUN_CHILD *) NULL)
                    {
                    char * fname;
                    int i;
                    struct stat stat_buf;

                        fname = filename("%s/%s",cur_job->job_queue->queue_dir,
                                         cur_job->job_file);
                        i = stat(fname,&stat_buf);
                        (void) free(fname);
                        if (i)
                        {  /* The job has finished between opening the directory
                              and fetching the file name */
                            continue;
                        }
                        if (*(cur_job->job_file+1) == '0')
                        {
                        char * x[5];
                        FILE * fp;
                        x[0] ="Cleaned up defunct job:";
                        x[1] = cur_job->job_queue->queue_name;
                        x[2] = cur_job->job_name;
                        x[3] = cur_job->job_owner;
                        x[4] = cur_job->job_file;

                            natlog(5,x);
                            if ((fp = log_file_open(cur_job,&fname))
                                        != (FILE *) NULL)
                            {
                                for (i = 0; i< 5; i++)
                                    (void) fprintf(fp,"%s\n",x[i]);
                            }
                            ret_mail(cur_job,fp);   /* always return mail if
                                                       crashed */
                            (void) fclose(fp);
                            cur_job->status = JOB_FAILED;
                            renamejob_db(cur_job); 
                                            /* Tidy Up */
                            if (cur_job->status != JOB_HELD &&
                                cur_job->status != JOB_RELEASED &&
                                cur_job->status != JOB_SUBMITTED)
                                (void) e2unlink (cur_job->job_file);
                            else
                            {
                                run_job = job_clone(cur_job);
                                (void) sprintf(new_name, "%s",
                                                  (run_job->job_file+2));
                                (void) free(run_job->job_file);
                                run_job->job_file = strnsave(new_name,
                                                     (int) strlen(new_name));
                                if (renamejob(cur_job,run_job) == 0)
                                {
                                char * x[5];
                                x[0]="Failed to allocate job for re-execution:";
                                x[1] = run_job->job_queue->queue_name;
                                x[2] = run_job->job_name;
                                x[3] = run_job->job_owner;
                                x[4] = cur_job->job_file;

                                    natlog(5,x);
                                }
                                job_destroy(run_job);
                                (void) free(run_job);
                                (void) free(fname);
                                continue;
                            }
                            if (!strcmp(cur_job->log_keep,"no"))
                                (void) e2unlink(fname);
                            (void) free(fname);
                            continue;
                        }
                    }
                    else
                    {
                        continue;   /* We are already executing this */
                    }
                }
                cur_time = time(0);
/*
 * Compute the time from the year, julian and time (use yyval() on d_name;
 * value should match year)
 */
                if (cur_job->status == JOB_SUCCEEDED ||
                    cur_job->status == JOB_FAILED ||
                    cur_job->status == JOB_HELD)
                    continue;                    /* Job on hold, succeeded or
                                                    failed, skip */
                if (cur_job->job_execute_time > cur_time)
                {
                    add_time(cur_job->job_execute_time);
                    continue;
                }
                else if ((*cur_queue)->cur_simul >= (*cur_queue)->max_simul ||
                     child_cnt >= glob_simul)
                {
                    break;
                }
                if (*(cur_job->job_file) == 'X')
                {
                    (void) strcpy(new_name, cur_job->job_file);
                    run_job = job_clone(cur_job);
                }
                else
                {
                    cur_job->status = JOB_STARTED;
                    if (!job_status_chg(cur_job)
                     || cur_job->status != JOB_STARTED )
                    {     /* Can we run this yet? */
                        continue;
                    }
                    run_job = job_clone(cur_job);
                    (void) free(run_job->job_file);
                    (void) sprintf(new_name,"X%c%s",
                           (*(cur_job->job_restart) != 'y') ? '0' : 'A',
                               cur_job->job_file);
                    run_job->job_file =
                             strnsave(new_name,(int) strlen(new_name));
                    if (renamejob(cur_job,run_job) == 0)
                    {
                    char * x[4];
                    x[0] ="Failed to allocate job for execution:";
                    x[1] = cur_job->job_queue->queue_name;
                    x[2] = cur_job->job_name;
                    x[3] = cur_job->job_owner;

                        natlog(4,x);
                        job_destroy(run_job);
                        (void) free(run_job);
                        continue;
                    }
                    renamejob_db(run_job);            /* Update the DB status */
                }
                if ((run_job->job_chan = fopen(new_name,"r")) == (FILE *) NULL)
                {       /* Result of race condition.  Not harmful */
                char * x[5];
                x[0] ="Cannot open job file:";
                x[1] = run_job->job_queue->queue_name;
                x[2] = run_job->job_name;
                x[3] = run_job->job_owner;
                x[4] = new_name;

                    natlog(5,x);
                    job_destroy(run_job);
                    (void) free(run_job);
                    continue;
                }
                setbuf(run_job->job_chan, buf);
#ifdef MINGW32
                i = (int) CreateThread(NULL, 0, spawn_child, (void *) run_job,
                                                 0, &pid);
                CloseHandle((HANDLE) pid);
#else
                i = spawn_child(run_job);
#endif
/*
 * PARENT
 */
                (void) fclose(run_job->job_chan);
                run_job->job_chan = (FILE *) NULL;
                if (i<0)
                {
                char * x[4];
                x[0]="Fork to execute job failed:";
                x[1] = run_job->job_queue->queue_name;
                x[2] = run_job->job_name;
                x[3] = run_job->job_owner;

                    perror("Fork to execute job failed");
                    natlog(4,x);
                    job_destroy(run_job);
                    free(run_job);
                    continue;
                }
                add_child(i,run_job,cur_time);
                if (child_cnt >= glob_simul)
/*
 * Pause if there are too many children
 */
                    fifo_check(NOPOLL);
                else
                    fifo_check(POLL);

            }           /* end of loop through the files in the directory */
            queue_close(*cur_queue); /* Bins all the jobs */
/*
 * Update time last done
 */
            if (cur_time > 0)         /* done something */
            {
                if ((out_file = fopen(LASTTIMEDONE,"w")) == (FILE *) NULL)
                    perror("Failed to update lasttimedone");
                else
                {
                    setbuf(out_file,(char *) NULL);
                    (void) fprintf(out_file,"%ld\n",cur_time);
                    (void) fclose(out_file);
                }
            }
        }       /* end of loop through the possible queues */
        if (timer_exp == 0)    /* Possible race condition here? */
        fifo_check(NOPOLL);    /* Nothing doing, go to sleep */
    }
}
/***************************************************************************
 * add a child process to the list;
 * overflow handled safely, if with degraded functionality
 */
void add_child(pid,job,start_time)
int pid;
JOB * job;
time_t start_time;
{
RUN_CHILD * cur_child_ptr;
    NEW (RUN_CHILD, cur_child_ptr);

    cur_child_ptr->child_pid = pid;
    cur_child_ptr->own_job = job;
    cur_child_ptr->start_time = start_time;
    (void) insert(child_pid_tab,(char *) pid,(char *) cur_child_ptr);
    (void) insert(child_job_tab,(char *) job->job_no,(char *) cur_child_ptr);
    child_cnt++;
    cur_child_ptr->own_job->job_queue->cur_simul++;
    return;
}
/*
 * Remove a child process from the list; perform the necessary
 * tidying up.
 */
void rem_child(pid,pidstatus,res_used)
int pid;
#ifdef PYR
union wait
#else
int
#endif
    pidstatus;
struct rusage * res_used;
{
register RUN_CHILD * cur_child_ptr;
HIPT * ind;
char new_name[MAXFILENAME];
    if ((ind = lookup(child_pid_tab,(char *) pid)) != (HIPT *) NULL)
    {                                 /* Found the child */
        struct stat stat_buf;
        char *x;
        char * mess;
        int y;
        char cbuf[BUFSIZ];
        FILE * fp;
        char * fname;
        JOB * cur_job, * old_job;
        RESOURCE_PICTURE res_pic;

        cur_child_ptr = (RUN_CHILD *) ind->body;
        (void) hremove(child_pid_tab,(char *) pid);
        cur_job = cur_child_ptr->own_job;
        (void) hremove(child_job_tab,(char *) cur_job->job_no);
        child_cnt--;
        res_pic.no_of_samples = 1;
        res_pic.low_time = cur_child_ptr->start_time;
        res_pic.high_time = time(0);   /* May be a little late! */
        res_pic.sigma_elapsed_time = (double) (res_pic.high_time
                                               - res_pic.low_time);
        res_pic.sigma_cpu = (double) res_used->ru_utime.tv_sec +
                            (double) res_used->ru_stime.tv_sec +
                            ((double) res_used->ru_utime.tv_usec +
                            (double) res_used->ru_stime.tv_usec)/1000000.0;
        res_pic.sigma_io = (double) res_used->ru_inblock +
                            (double) res_used->ru_oublock;
        res_pic.sigma_memory = (double) res_used->ru_idrss;
        res_pic.sigma_elapsed_time_2 = res_pic.sigma_elapsed_time *
                                       res_pic.sigma_elapsed_time;
        res_pic.sigma_cpu_2 = res_pic.sigma_cpu * res_pic.sigma_cpu;
        res_pic.sigma_io_2 = res_pic.sigma_io * res_pic.sigma_io;
        res_pic.sigma_memory_2 = res_pic.sigma_memory * res_pic.sigma_memory;
#ifdef PYR
        cur_job->execution_status = (int) pidstatus.w_status;
#else
        cur_job->execution_status = pidstatus;
#endif
        if (WIFEXITED(pidstatus))
        {
            mess = "exiting with status";
            y = WEXITSTATUS(pidstatus);
            if (!y)
                 cur_job->status =  JOB_SUCCEEDED;
            else
                 cur_job->status =  JOB_FAILED;
        }
        else /* Terminated by signal */
        {
            mess = "terminated by signal";
            y = WTERMSIG(pidstatus);
            cur_job->status =  JOB_FAILED;
        }
        (void) sprintf(cbuf,
              "Job no %d name %s for %s finished on queue %s %s %d",
              cur_job->job_no,
              cur_job->job_name,
              cur_job->job_owner,
              cur_job->job_queue->queue_name,
              mess, y);
        (void) fwrite(cbuf,sizeof(char),(int) strlen(cbuf),sg.errout);
        (void) fputc('\n', sg.errout);
        x=cbuf;
        natlog(1,&x);
        if ((fp = log_file_open(cur_job,&fname)) != (FILE *) NULL)
        {      /* Success trying to re-open the log file */
            (void) fwrite(cbuf,sizeof(char),(int) strlen(cbuf),fp);
            (void) fputc('\n', fp);
            (void) fprintf(fp,"User Time/Secs %g\n",(double)
                                  res_used->ru_utime.tv_sec +
                        ((double) res_used->ru_utime.tv_usec)/1000000.0);
                   /* user time used */
            (void) fprintf(fp,"System Time/Secs %g\n", (double)
                                  res_used->ru_stime.tv_sec +
                        ((double) res_used->ru_stime.tv_usec)/1000000.0);
                   /* system time used */
            (void) fprintf(fp,"Integral Memory Usage %ld\n",res_used->ru_idrss);
            (void) fprintf(fp,"Block Input Operations %ld\n",
                                                     res_used->ru_inblock);
                   /* block input operations */
            (void) fprintf(fp,"Block Output Operations %ld\n",
                                                     res_used->ru_oublock);
                                /* block output operations */
            if (cur_job->status == JOB_FAILED ||
                !strcmp(cur_job->job_mail,"yes"))
                ret_mail(cur_job,fp);   /* always return mail if
                                           crashed */
        }
        (void) fclose(fp);
        renamejob_db(cur_job);           /* Tidy Up */
        updatejob_db(cur_job,&res_pic);  /* Tidy Up */
        if (cur_job->status == JOB_SUCCEEDED || cur_job->status == JOB_FAILED)
        {
            sprintf(cbuf,"%s/%s",cur_job->job_queue->queue_dir,
                          cur_job->job_file);
            (void) e2unlink(cbuf);
                                               /* remove the job file */
        }
        else
        {   /* resubmit - hour, day or week or minutes later */
            old_job = job_clone(cur_job);
             
            switch (*(cur_job->job_resubmit))
            {
            case 'p':
                cur_job->job_execute_time += atoi(cur_job->job_resubmit+1)*60;
                break;
            case 'h':
                cur_job->job_execute_time += 3600 ;
                break;
            case 'd':
            case 'w':
            {
/**********************************************************************
 * Attempt to correct the time for DST.
 */
#ifndef MINGW32
            struct tm * tm_now, *tm_then;
#endif
            time_t this_time = cur_job->job_execute_time;
                cur_job->job_execute_time +=
                                  ((*(cur_job->job_resubmit)== 'd') ? 86400 :
                                  ((*(cur_job->job_resubmit) == 'w') ?
                                            7 * 86400 : 0));
#ifndef MINGW32
                tm_now = localtime(&this_time);
                tm_then = localtime(&(cur_job->job_execute_time));
                if (tm_now->tm_isdst != tm_then->tm_isdst)
                {
                    if (tm_now->tm_isdst)
                        cur_job->job_execute_time += 3600;
                    else
                        cur_job->job_execute_time -= 3600;
                }
#endif
                break;
            }
            default:
                if (*(old_job->job_file) == 'X')
                {     /* Trigger did not contain a RE_QUEUE() */
                    (void) free(cur_job->job_file);
                    (void) sprintf(new_name,"%s", (old_job->job_file+2));
                    cur_job->job_file = strnsave(new_name,
                                                 (int) strlen(new_name));
                }
                break;
            }
            if (renamejob(old_job,cur_job) == 0)
            {
                char * x[5];
                x[0] ="Failed to restore job after execution:";
                x[1] = cur_job->job_queue->queue_name;
                x[2] = cur_job->job_name;
                x[3] = cur_job->job_owner;
                x[4] = cur_job->job_file;
                natlog(5,x);
            }
            job_destroy(old_job);
            (void) free(old_job);
        }
        if (!strcmp(cur_job->log_keep,"no"))
            (void) e2unlink(fname);
        cur_job->job_queue->cur_simul--;
        job_destroy(cur_job);
        (void) free(cur_job);
        (void) free(cur_child_ptr);
        (void) free(fname);
    }
    return;
}
/*
 * Find the child running a job in the list
 */
RUN_CHILD * find_job(job_no)
int job_no;
{
HIPT * ind;
    if ((ind = lookup(child_job_tab,(char *) job_no)) != (HIPT *) NULL)
                                      /* Found the child */
        return (RUN_CHILD *) ind->body;
    else
        return (RUN_CHILD *) NULL;
}
/***************************************************************************
 * Clock functions
 *
 * add_time();  add a new time to the queue; start the clock if not running
 * This function moves the buffer head, but not its tail.
 *  - new_time is an absolute time in seconds since 1970.
 *  - do not add it if it's a duplicate
 */
void add_time(new_time)
time_t new_time;
{
    short int cur_ind;
    time_t sav_time;

    for (cur_ind = tail;
                    cur_ind !=head && go_time[cur_ind] < new_time;
                                 cur_ind = (cur_ind + 1) % MAXQUEUES);
    if (go_time[cur_ind] != new_time)
    {
        for (; cur_ind != head; cur_ind = (cur_ind + 1) % MAXQUEUES)
        {
            sav_time = go_time[cur_ind];
            go_time[cur_ind] = new_time;
            new_time = sav_time;
        }
        if (tail != (head + 1) % MAXQUEUES)
        {
            go_time[head] = new_time;
            head = (head + 1) % MAXQUEUES;
        }
    }
    if (clock_running != 0)
        alarm(0);
    clock_running = 0;
    rem_time();
    return;
} 
/*
 * rem_time(); tidy up the queue, removing times from the tail as they
 * expire. Start the clock if there is anything in the queue. Apart from
 * reset_time(), nothing else moves the tail.
 * This function is NEVER called if the clock is running.
 */
void rem_time()
{
    short int cur_ind;
    int sleep_int;
    time_t cur_time = time((time_t *) 0);
#ifdef DEBUG
    (void) fprintf(sg.errout,"rem_time(): Clock Running %d\n",clock_running);
#endif
    for (cur_ind = tail;
                    cur_ind !=head && go_time[cur_ind] <= cur_time;
                                 cur_ind = (cur_ind + 1) % MAXQUEUES,
                                 tail = cur_ind,
                                 timer_exp++);
    if (tail != head)
    {
        (void) sigset(SIGALRM,rem_time);
        sleep_int = go_time[tail] - cur_time;
        clock_running ++;
        (void) alarm(sleep_int);
    }
    return;
} 
/*
 * Reset the time buffers
 */
void reset_time()
{
    sigset(SIGALRM,SIG_IGN);
    tail = head;
    clock_running = 0;
    timer_exp = 0;
    return;
}
/*
 * Function to return mail to the user
 */
void ret_mail(cur_job,out_file)
JOB * cur_job;
FILE * out_file;
{
    int i;
#ifndef MINGW32
    (void) fseek(out_file,0L,0);
                   /* go back to the beginning of the temporary file */
    if ((i=fork()) == 0)
    {                 /* MAIL CHILD */
/* fork(), manipulate the file descriptors to get out_file as stdin,
 * and exec /usr/bin/mail to send it.
 */
        if (dup2(fileno(out_file),0))
        {
             perror("mail dup failed");
             exit(1);
        }
        (void) fclose(out_file);
#ifdef LINUX
        if (execl("/bin/mail","mail",cur_job->job_owner,0))
#else
#ifdef HP7
        if (execl("/bin/mail","mail",cur_job->job_owner,0))
#else
#ifdef PTX
        if (execl("/bin/mail","mail",cur_job->job_owner,0))
#else
#ifdef PTX4
        if (execl("/bin/mail","mail",cur_job->job_owner,0))
#else
        if (execl("/usr/bin/mail","mail",cur_job->job_owner,0))
#endif
#endif
#endif
#endif
        {
            perror("mail exec failed");
            exit(1);
        }
    }
    else
    if (i < 0)
        perror("mail fork failed");
#endif
    return;
}
/*
 * The following routine handles a possible problem with pipe buffering.
 * shells (if nothing else), by inserting padding to ensure that no command
 * straddles a 1024 byte boundary.
 */
static void pad_con(fd,buf_con,com_cnt, socket_flags)
int fd;
int * buf_con;
int com_cnt;
int socket_flags;
{
    char buf[1024];
    if ((com_cnt + *buf_con) > BUFSIZ)
    {
        int pad_len;
        pad_len = 1023 - *buf_con;
        sprintf(buf,"%*.*s\n",pad_len,pad_len,"");
#ifdef NETVERS
        (void) send(fd,buf,pad_len+1,socket_flags);
#else
        (void) write(fd,buf,pad_len+1);
#endif
        *buf_con = 0;
    }
    return;
}
static void pad_out(fd,buf,buf_con,socket_flags)
int fd;
char * buf;
int * buf_con;
int socket_flags;
{
    int out_len;
    out_len = (int) strlen(buf);
#ifdef NETVERS
    (void) send(fd,buf,out_len,socket_flags);
#else
    (void) write(fd,buf,out_len);
#endif
    *buf_con += out_len;
    return;
}
static void stamp_out(f,lab)
FILE *f;
char  lab;
{
time_t t;
char *x;
    t = time(0);
    x = ctime(&t);
    *(x + 9) = lab;
    fwrite(x+9, sizeof(char),11,f);
    return;
}
/*
 * CHILD
 * read from the file, output to link, receive from
 * link; when all done, search for end of job message before
 * shutting down.
 *
 * There are two distinctive flavours of this code.
 * - NETVERS is when the code is being executed potentially on a
 *   remote machine. In this case, the logic allows for an 'echo'
 *   command to be sent after the job, so that there will be a 
 *   positive acknowledgement of the fact that the processing has
 *   definitely reached the end.
 * - Ordinarily, this is not necessary, since the work is being
 *   executed by a child shell, which can be waited for.
 */
void do_one_job(out_file,
rem_in,
rem_out,
rem_err,
cur_job,
pid)
FILE * out_file;
int rem_in;
int rem_out;
int rem_err;
JOB * cur_job;
int pid;
{
ENV * e;
FILE * com_chan = cur_job->job_chan;
int buf_con;    /* Added to circumvent a problem with the Ultrix shells */
int disc_flag;
#ifdef PYR
union wait
#else
int
#endif
        pidstatus;
char err_buf[BUFSIZ];
char io_buf[BUFSIZ];
int read_count;
int read_check;
int write_check;
fd_set read_mask;
fd_set write_mask;
fd_set readymask;
fd_set writeymask;
#ifdef NETVERS
struct sockaddr_in calling_sock;
int socket_flags=0;
static int addr_size = sizeof(struct sockaddr_in);
#endif
char * x;
char * end_of_buf;

    FD_ZERO(&write_mask);
    FD_ZERO(&read_mask);
    FD_SET (rem_in, &read_mask);
    FD_SET (rem_err, &read_mask);
    FD_SET (rem_out, &write_mask);
    read_check = 2;
    write_check = 1;

    (void) sprintf(io_buf,
                   "Job no %d name %s for %s starting on queue %s",
                   cur_job->job_no,cur_job->job_name,cur_job->job_owner,
                   cur_job->job_queue->queue_name);
    (void) fprintf(sg.errout,"%s",io_buf);
    (void) fprintf(sg.errout,"\n");
    x=io_buf;
    natlog(1,&x);
    if (out_file != (FILE *) NULL)
    {
        (void) fprintf(out_file,"Subject:");
        (void) fprintf(out_file,"%s",io_buf);
        (void) fprintf(out_file,"\n");
        (void) fprintf(out_file,"=======================\n");
    }
    end_of_buf = io_buf;
    buf_con = 0;
    if (*(cur_job->job_file+1) != '0')
    {
        (void) sprintf(io_buf,"RESTART=%c; export RESTART\n",
                           *(cur_job->job_file+1));
        buf_con = (int) strlen(io_buf);
#ifdef NETVERS
        (void) send(rem_out,io_buf,buf_con,socket_flags);
#else
        (void) write(rem_out,io_buf,buf_con);
#endif
    }
#ifndef NETVERS
/*
 * Set the priority if it is legal and not defaulted
 */
    if (cur_job->nice != 0)
    {
        if (cur_job->nice > 0 || !strcmp(cur_job->job_owner,"root"))
#ifdef NT4
        (void) nice(cur_job->nice);
#else
#ifdef SCO
        (void) nice(cur_job->nice);
#else
#ifdef PTX
        (void) nice(cur_job->nice);
#else
        (void) setpriority(PRIO_PROCESS,pid,cur_job->nice);
#endif
#endif
#endif
    }
#endif
/*
 * Feed in the Job's inherited environment
 */
    for (e = cur_job->j_local_env;
             e != (ENV *) NULL;
                 e = e->e_next)
    {
    char * x;

        if (e->e_env != (char *) NULL && ((int) strlen(e->e_env)) > 0 &&
            (x = strchr(e->e_env,'=')) != (char *) NULL)
        {
        char * stuffed_val;

            *x ='\0';
            stuffed_val = envstuff(x+1);
            (void) sprintf(io_buf,"%s='%s'\n",e->e_env,stuffed_val);
#ifdef NETVERS
            pad_out(rem,io_buf,&buf_con, socket_flags);
#else
            pad_out(rem_out,io_buf,&buf_con,0);
#endif
            (void) sprintf(io_buf,"export %s\n",e->e_env);
#ifdef NETVERS
            pad_out(rem,io_buf,&buf_con, socket_flags);
#else
            pad_out(rem_out,io_buf,&buf_con,0);
#endif
            (void) free(stuffed_val);
            *x = '=';   /* Restore the corruption! */
        }
    }
    disc_flag = 0;
/*
 * Now feed the command file to the shell
 */
    for (sg.sstate = ORD;
            sg.sstate != ALLDONE && sg.sstate != KILLED && sg.sstate != DIED;)
    {
static char * end_com_str = END_COM_STR;
static char * send_look_str =SEND_LOOK_STR;
static char * look_str =LOOK_STR;
static char * restart_str =RESTART_STR;
static int restart_len;

/*
 * select for read on rem, rem_err;
 *        for write on rem.
 */
        restart_len = (int) strlen(restart_str);
        readymask = read_mask;
        writeymask = write_mask;
        if (select(20,&readymask,&writeymask,(int *) 0,0)<1)
        {
            if (errno == EINTR)
                continue;            /* Perhaps this is being killed? */
            perror("Select failed");
            exit(1);
        }
        if (sg.sstate == KILLED || sg.sstate == DIED)
            break;
        if (FD_ISSET(rem_out, &writeymask))
        {
/*
 * If write on rem
 * Read line from com_chan.
 * If EOF, output the << string, and then
 * output 'echo the search string...'
 */
            switch (sg.sstate)
            {
            case ORD:
                if (fgets(io_buf,BUFSIZ,com_chan) == NULL)
                {
                    if (sg.sstate == KILLED || sg.sstate == DIED)
                        break;
#ifdef NETVERS
                    sg.sstate = ENDSENT;
                    pad_out(rem_out,end_com_str,&buf_con, socket_flags);
#else
                    sg.sstate = EOFSEEN;
                    pad_out(rem_out,end_com_str,&buf_con,0);
                    FD_ZERO(&write_mask);  /* don't check for this any more */
                    write_check = 0;
                    (void) close(rem_out);
#endif
                }
                else
                {
                    if (io_buf[0] != '\0' && (disc_flag || io_buf[0] != '#'))
                    {
                        disc_flag = 1;
#ifdef NETVERS
                        pad_out(rem_out,io_buf,&buf_con, socket_flags);
#else
                        pad_out(rem_out,io_buf,&buf_con,0);
#endif
                    }
                }
                break;
            case DIED:
            case KILLED:
                continue;
                break;
#ifdef NETVERS
            case ENDSENT:
                sg.sstate = EOFSENT;
                pad_out(rem_out,send_look_str,&buf_con, socket_flags);
            case EOFSENT:
#endif
            default:
                FD_ZERO(&write_mask);  /* don't check for this any more */
                write_check = 0;
                break;
            }
        }
        if (FD_ISSET(rem_in, &readymask))
        {                  /* remote shell stdout something there */
/*
 * if read on rem, output to stdout, unless it matches the end
 * string after that has been sent, in which case break from the
 * loop. If MAIL, write to the out_file.
 */
#ifdef NETVERS
            read_count=recvfrom( rem_in,end_of_buf,BUFSIZ,
                   socket_flags, &calling_sock,&addr_size);
#else
            read_count=read( rem_in,end_of_buf,BUFSIZ);
#endif
            if (sg.sstate == KILLED || sg.sstate == DIED)
                break;
            if (read_count < 0)
                perror ("recv error");
            else
            if (read_count == 0)
            {
                if (sg.sstate != EOFSEEN )
                {
                    (void) fprintf (sg.errout,"EOF??!\n");
                    sg.sstate = EOFSEEN;
                }
                (void) close(rem_in); /* shutdown */
                FD_CLR(rem_in, &read_mask);
                read_check--;
            }
            else
            {
                if (sg.stamp_out)
                    stamp_out(sg.errout,'O');
                 (void) fwrite(end_of_buf,
                           sizeof(char),read_count, stdout);
                 if (out_file != (FILE *) NULL)
                 {
                     if (sg.stamp_out)
                         stamp_out(out_file, 'O');
                     (void) fwrite(end_of_buf,
                                sizeof(char),read_count,out_file);
                 }
            }
#ifdef NETVERS
            if (sg.sstate == EOFSENT)
            {
            char * x1;
/*
 * Have to scan the input stream. Since do not know whether can guarantee
 * that a line will be kept together, need to partition the line. Use the
 * 'io_buf' buffer.
 */
                *(end_of_buf + read_count) = '\0';
                                        /* put an end marker */
                for (x=io_buf;x < (end_of_buf + read_count);x=x1+1)
                    if ((x1=strchr(x,(int) '\n')) == (char *) NULL)
                    {
                        if (x < (end_of_buf + read_count - 1))
                        {
                            (void) memcpy(io_buf, x,
                                   read_count - (x - end_of_buf));
                            end_of_buf = io_buf + read_count -
                                         (x - end_of_buf);
                        }
                        else end_of_buf = io_buf;
                        break;
                    }
                    else
                    {
                        *x1='\0';
                        if (strcmp(x,look_str) == 0)
                        {
                            sg.sstate = EOFSEEN;
                            (void) close(rem); /* shutdown */
                            read_mask &= ~rem_in_check;
                        }
                    }
            }
#endif
        }
        if (FD_ISSET(rem_err, &readymask))
        {                            /* remote shell stderr */
/*
 * if read on fd2, output to stderr, unless it matches the end
 * string, after that has been sent, in which case break from the
 * loop. If MAIL, write to the out_file.
 */
#ifdef NETVERS
            read_count=recvfrom( rem_err,err_buf,BUFSIZ,
                   socket_flags, &calling_sock,&addr_size);
#else
            read_count=read( rem_err,err_buf,BUFSIZ);
#endif
            if (sg.sstate == KILLED || sg.sstate == DIED)
                break;
            if (read_count < 0)
                perror ("stderr read error");
            else
            if (read_count == 0)
            {
#ifdef DEBUG
                (void) fprintf (sg.errout,
                        "Secondary channel shut down\n");
#endif
                (void) close(rem_err);
                FD_CLR(rem_err, &read_mask);
                read_check--;
            }
            else
            {
                if (*(cur_job->job_file+1) != '0')
                {     /* restartable - scan stderr for information */
                    if (!strncmp(err_buf,restart_str,restart_len))
                    {
                        if (err_buf[restart_len] > 'A')
                        {
                        JOB * old_job;

                            old_job = job_clone(cur_job);
                            *(cur_job->job_file+1) =
                                err_buf[restart_len];
                            renamejob(old_job,cur_job);
                        }
                    }
                }
                if (sg.stamp_out)
                    stamp_out(sg.errout,'E');
                (void) fwrite(err_buf,
                         sizeof( char ), read_count, sg.errout);
                if (out_file != (FILE *) NULL)
                {
                    if (sg.stamp_out)
                        stamp_out(out_file, 'E');
                    (void) fwrite(err_buf,
                         sizeof( char ), read_count, out_file);
                }
            }
        }
        if (read_check == 0 && write_check == 0 && sg.sstate != KILLED)
           sg.sstate=ALLDONE;
     }    /*    End of ALLDONE loop */
     if (sg.sstate == KILLED)
     {
         (void) sprintf(io_buf,
                   "User Requested Termination of Job no %d name %s for %s\n",
                   cur_job->job_no,cur_job->job_name,cur_job->job_owner);
         (void) fprintf(sg.errout,"%s",io_buf);
         x=io_buf;
         natlog(1,&x);
         if (out_file != (FILE *) NULL)
         {
             (void) fprintf(out_file,"==========================\n");
             (void) fprintf(out_file,"%s",io_buf);
         }
#ifndef NETVERS
         (void) kill(pid,SIGINT);      /* hangup the shell */
         (void) kill(pid,SIGHUP);      /* hangup the shell */
#endif
     }
#ifndef NETVERS
     (void) sigset(SIGCHLD,SIG_DFL); /* Avoid nasties with the SIGCHLD/wait
                                       interaction on some systems */
     while (wait( &pidstatus) !=  pid);
#endif
     if (out_file != (FILE *) NULL)
     {
/*
 * Send mail if necessary
 */
         (void) fprintf(out_file,"==========================\n");
         if (WIFEXITED(pidstatus))
             (void) fprintf(out_file,"Job finished with exit status %d\n",
                         WEXITSTATUS(pidstatus));
         else
             (void) fprintf(out_file,"Job terminated by signal %d\n",
                         WTERMSIG(pidstatus));
         (void) fclose(out_file);
     }
/*
 * Exit child
 */
     if (sg.sstate == KILLED)
            exit(1);
     if (WIFEXITED(pidstatus))
         exit(WEXITSTATUS(pidstatus));
     else
         exit(WTERMSIG(pidstatus));
}
static int lcomp(el1,el2)
QUEUE ** el1, ** el2;
{
    if ((*el1)->priority == (*el2)->priority) return 0;
    else if ((*el1)->priority < (*el2)->priority) return 1;
    else return -1;
}
static QUEUE  ** pick_queue()
{
/*
 * This function knows where the array of time_det structures is.
 * This is terminated with a structure that does not point to a queue_det
 * structure, and is sorted in ascending order of low_time.
 * 
 * The function exits if there are no queues available
 *    - gets the current time of day, in seconds since midnight.
 *    - loops through the time_det structures
 *      - if the time is between low_time and high_time,
 *        add it to the queue list,
 *        and update the max_simul value and priority.
 *    - if none are found, sleep until the next start time, and start
 *      again
 *    - terminates the list with a null pointer
 *    - sorts the list into preference order.
 */ 
    long cur_secs;
    time_t next_look;
    int i;
    AVAIL_TIME * a;
    struct tm * tm_ptr,  * localtime();
    QUEUE * x_queue;
    QUEUE ** next_queue;
    for ( cur_secs = ( (long) local_secs(time(0))) % 86400,
          next_queue = eli_queues;
              next_queue == eli_queues;
                  cur_secs = ( (long) local_secs(time(0))) % 86400)
    {
        
        for (x_queue = queue_anchor;
                  x_queue != (QUEUE *) NULL;
                        x_queue=x_queue->q_next)
        {        /* Loop; for each queue */
/*
 * The array of structures is in ascending order of start time
 */

            if (!(i = x_queue->time_cnt))
                 continue; /* ignore if no availabilities */
            for ( x_queue->cur_time_ind = x_queue->time_order,
                  a = *(x_queue->cur_time_ind++);
                      i > 0;
                            i--,
                            a = *(x_queue->cur_time_ind++))
 
            {
                if (a->resource_limits.low_time > cur_secs)
                    break;
                if (a->resource_limits.high_time < cur_secs
                  || a->max_simul == 0)
                    continue;
/*
 * At this point, we have found an eligible one; the time is sandwiched
 */
                x_queue->priority= a->prior_weight;
                x_queue->max_simul= a->max_simul;
                *next_queue++ = x_queue;
                break;
            }

            next_look = (time_t) (86400.0* floor(local_secs(time(0))/86400.0));
                         /* the start of the current day in local time */
            if (i == 0)
            {             /* picked up everything; need to look again at
                           * midnight
                           */
                next_look += 86400;
            }
            else
            if (a->resource_limits.low_time > cur_secs)
                next_look = next_look + a->resource_limits.low_time;
            else
                next_look = next_look + a->resource_limits.high_time;
            add_time((time_t) gm_secs(next_look));
                                     /* must note the next status change in
                                        availability */
        }
        if (next_queue == eli_queues)
        {       /* nothing at all; await events */
            fifo_check(NOPOLL);
            timer_exp = 0;
        }
    }
    *next_queue = (QUEUE *) NULL; /* terminate the list */
    (void) qsort((char *) eli_queues,
    (next_queue - eli_queues),
    sizeof(QUEUE *),lcomp);
#ifdef DEBUG
    dump_eli();
#endif
    return eli_queues;
}
/*
 * Function to dump eligible queues
 */
static void dump_eli()
{
short int j;

    (void) fprintf(sg.errout,
             "Eligible Queues                  Priority Max. Sim. Cur. Sim.\n");
    for (j=0; eli_queues[j] != (QUEUE *) NULL; j++)
        (void) fprintf(sg.errout,"%-33.33s%6.5ld%9.1ld%9.1ld\n",
                 eli_queues[j]->queue_name,
                 eli_queues[j]->priority,
                 eli_queues[j]->max_simul,
                 eli_queues[j]->cur_simul);
}
/*
 * Function to dump running jobs
 */
static void dump_children()
{
int i,j;
    
    (void) fprintf(sg.errout,
"Queue                                Job Name          Owner             PID\n");
    for (i=0,j=0;j < child_pid_tab->tabsize; j++)
    {
         if (child_pid_tab->table[j].in_use)
         {
             register RUN_CHILD * cur_child_ptr;
             i++;
             cur_child_ptr = (RUN_CHILD *) child_pid_tab->table[j].body;
             (void) fprintf(sg.errout,"%-33.33s%7.1ld %-13.13s %-10.10s %10.1ld\n",
                   cur_child_ptr->own_job->job_queue->queue_name,
                   cur_child_ptr->own_job->job_no,
                   cur_child_ptr->own_job->job_name,
                   cur_child_ptr->own_job->job_owner,
                   cur_child_ptr->child_pid);
         }
    }
    if (child_cnt != i)
        (void) fprintf(sg.errout,
                       "Warning: Job children %d != internal count %d\n",
                          i, child_cnt);
    return;
}
