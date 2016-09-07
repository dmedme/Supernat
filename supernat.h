/************************************************************
 * supernat.h - SUPERNAT definitions
# @(#) $Name$ $Id$
 * Copyright (c) E2 Systems, 1992
 */
#ifndef SUPERNAT_H
#define SUPERNAT_H

#include <sys/types.h>
#include <sys/param.h>
#ifndef ANSIARGS
#define ANSIARGS(x) ()
#endif /* !ANSIARGS */
#ifdef MINGW32
/*
 * Need to provide an implementation of opendir(), readdir(), closedir()
 */
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifndef S_ISDIR
#define S_ISDIR(x) (x & S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(x) (x & S_IFREG)
#endif
struct dirent {
    struct _finddata_t f;
    int read_cnt;
    long handle;
};
typedef struct dirent DIR;
#define d_name f.name
struct dirent * readdir ANSIARGS((DIR));
DIR * opendir ANSIARGS((char*));
void closedir ANSIARGS((DIR));
#ifdef timezone
#undef timezone
#endif
struct timezone {
int tz_minuteswest;
};
struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};
struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;
        long    ru_ixrss;               /* XXX: 0 */
        long    ru_idrss;               /* XXX: sum of rm_asrss */
        long    ru_isrss;               /* XXX: 0 */
        long    ru_minflt;              /* any page faults not requiring I/O */
        long    ru_majflt;              /* any page faults requiring I/O */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcsw;              /* involuntary " */
};
#define SIGHUP 1
#define SIGQUIT 3
#define SIGBUS 10
#define SIGPIPE 13
#define SIGALRM 14
#define SIGUSR1 16
#define SIGCHLD 17
#define WNOHANG 1
#define MAXPATHLEN MAX_PATH
void (*sigset())();
void sighold();
void sigrelse();
#else
#ifdef POSIX
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#endif
#include "hashlib.h"
#define MAXUSERNAME 124
#define MAXFILENAME BUFSIZ

/* the userstr defines a user and the permissions */
typedef struct userstr {
 char u_name[MAXUSERNAME]; /* user name */
 int u_perm;   /* permissions */
#define U_READ 4
#define U_WRITE 2
#define U_EXEC 1
} USER;

#include "defs.h"

/***********************************************************************
 *
 * Co-ordination between nat (writing) and natrun (reading)
 * command files
 */

#define END_COM_STR "...the rest of this file is shell input\n"
#define SEND_LOOK_STR "st=$?;echo '### This is the end marker ###';exit $st\n"
#define LOOK_STR "### This is the end marker ###"
#define SEND_COM_STR " << '...the rest of this file is shell input'\n"
#define RESTART_STR "RESTART"

#define MAXJOBS 999
#define BUFLEN      2048
#define MAXQUEUES 200
#define DEFPRIOR 1
#define SIMUL 3
/**************************************************************************
 * Definitions for the Queues
 */
                   /* default number of children */
/* the envstr defines an Environment setting */
typedef struct envstr {
        char    *e_env;             /* the environment string */
        struct  envstr * e_cont;    /* the next line, for multi-line items */
        struct  envstr * e_next;    /* the next in the chain */
} ENV;

typedef struct pstr {
char * queue_name;
char * host_name;
char * description;
} QUEUE_MACHINE;

typedef struct queue_det {
    char queue_name[MAXUSERNAME];
    char queue_dir[MAXUSERNAME];
    int max_simul;       /* maximum simultaneous jobs */
    int cur_simul;       /* currently simultaneous jobs */
    int priority;        /* The prior_weight from the current AVAIL_TIME */
    char time_details[256];
/***************************************************************************
 * Fields to do with opening a Queue
 */
    int job_cnt;           /* number of jobs */
    struct job_det * first_job;
    struct job_det ** job_order;
                           /* a list of job pointers in the desired order */
    struct job_det ** cur_job_ind;
                           /* where we currently are in the list */
/***************************************************************************
 * Fields to do with time availabilities
 */
    int time_cnt;          /* number of time periods */
    struct time_det * first_time;
    struct time_det ** time_order;
                           /* a list of time pointers in the desired order */
    struct time_det ** cur_time_ind;
                           /* where we currently are in the list */
    QUEUE_MACHINE * q_machine_queue;
                           /* the queue/machine details (if applicable) */
    struct  queue_det * q_use;         /* the parent     */
    struct  queue_det * q_next;        /* the next in the chain */
    ENV     *q_submitted;           /* the command executed at submission */
    ENV     *q_held;                /* the command executed on hold */
    ENV     *q_released;            /* the command executed at release */
    ENV     *q_started;             /* the command executed on startup */
    ENV     *q_succeeded;           /* the command executed on success */
    ENV     *q_failed;              /* the command executed on failure */
    ENV     *q_local_env;           /* local environment definitions */
    ENV     *q_last_env;            /* local environment definitions */
} QUEUE;
/*
 * This structure is used:
 * - To store results of jobs
 * - To hold capabilities of machines
 * - To store limits as to resource that can be consumed by a
 *   queue, or the SUPERNAT system as a whole.
 */
typedef struct _resource_picture {
int no_of_samples;
long low_time;            /* seconds since midnight */
long high_time;           /* seconds since midnight */
double sigma_cpu;
double sigma_io;
double sigma_memory;
double sigma_elapsed_time;
double sigma_cpu_2;
double sigma_io_2;
double sigma_memory_2;
double sigma_elapsed_time_2;
} RESOURCE_PICTURE;
/*
 * For job history and scheduling purposes, information is collected together
 * under an overall heading of the Job Name.
 */
typedef struct _overall_job {
char job_name[MAXUSERNAME];
struct _resource_picture res_used;
} OVERALL_JOB;
/*
 * Possible Job Statuses
 */
#define JOB_SUBMITTED 0
#define JOB_RELEASED 1
#define JOB_HELD 2
#define JOB_STARTED 3
#define JOB_SUCCEEDED 4
#define JOB_FAILED 5
/*
 * In Addition, the integer values of the Restart Point letters;
 * - A (65)
 * - B (66)
 * - etc.
 */
/*
 * The following structure is an instance of a job.
 * The logic that applies per status is stored in the job file, and is read
 * and processed there. It is not stored in memory.
 * Automatically, some variables are created:
 * - A JOB_job_no_STATUS
 * - A JOB_job_name_STATUS
 * Since there may be several jobs with the same name, the latter is
 * multi-valued.
 *
 * At execution time, for maximum simplicity, we want to just evaluate
 * a statement that says something like:
 * - Time to execute is passed and previous job has started and
 *   global maximum simultaneous is less than limit
 *   and queue maximum simultaneous is less than limit
 * - Time to execute is passed and Current Loadings plus Job loadings
 *   are less than global limits and Current Queue Loadings plus Job loadings
 *   are less than queue limits.
 * We want to scan as few files as possible looking for such statements;
 * they should leap out of the database
 * This implies that we have lots of variables corresponding to
 * elements in the structures.
 * How do we stop it wasting time re-evaluating things?
 * -  Strategy is ideally to only inspect things that may now evaluate
 *    differently because of change
 * -  Problems are that
 *    -   Lots of things depend on time
 *    -   Everything in a queue depends on the queue limits.
 *
 * Can we gradually relax the conditions as events pass? Thus, take out
 * the time dependency when it is expired, take out the 'After this one'
 * when that has started? Probably doesn't help. However, can perhaps
 * remove the cross-references?
 *
 * Currently, job submission etc. works regardless of whether or not
 * the server is running. Perhaps what we should do is apply the updates
 * from within natrun if it is running, otherwise as at present. The
 * advantage of this is that the database will be single user, and the
 * queue change functions will update the internal tables as well.
 * What does 'HELD' mean? What does 'RELEASE' mean? How are the defaults
 * held? Initially, hard coded. Think about generalising later.
 *
 */
typedef struct job_det {
    int job_no;
    QUEUE * job_queue;
    char * job_file;
    FILE * job_chan;
    time_t job_create_time;
    time_t job_execute_time;
    char *job_name;
    char *job_owner;
    char *job_group;
    char *job_shell;
    char *job_mail;
    char *job_parms;
    char *job_restart;
    char *job_resubmit;
    char *log_keep;
    char *express;
    ENV  *j_submitted;           /* the command executed at submission */
    ENV  *j_held;                /* the command executed on hold */
    ENV  *j_released;            /* the command executed at release */
    ENV  *j_started;             /* the command executed on startup */
    ENV  *j_succeeded;           /* the command executed on success */
    ENV  *j_failed;              /* the command executed on failure */
    ENV  *j_local_env;           /* local environment definitions */
    int nice;
    int status;
    int execution_status;
    struct job_det * next_job;
}          JOB;
/*
 * Data stored in the database
 */
#define IND_TYPE 0
#define HASH_TYPE 1
#define VAR_TYPE 2
#define JOB_ACC_TYPE 3
#define OVERALL_JOB_TYPE 4

typedef struct _job_acc {
    int job_no;
    int status;
    int execution_status;
    time_t planned_time;
    char queue_name[MAXUSERNAME];
    char user_name[MAXUSERNAME];
    char job_name[MAXUSERNAME];
    struct _resource_picture used;
}   JOB_ACC;
/*
 * Possible methods of choosing a job
 */
#define CHOOSE_FIFO 0
#define CHOOSE_WAIT_RUN 1
#define CHOOSE_RESOURCE_LOAD 2
typedef struct time_det {
    QUEUE * own_queue;
    struct time_det * next_time;
    int prior_weight;        /* queue priority */
    int max_simul;           /* number allowed in this queue */
    int selection_method;    /* How to choose a job */
    struct _resource_picture resource_limits;
                             /* Limits as to what can go on here */
} AVAIL_TIME;

typedef struct child_det   {
    int child_pid;        /* = 0 for a free slot */
    JOB * own_job;        /* job running */
    time_t start_time;      /* when started */
    long kern_proc;       /* the  location of the kernel proc structure */
    struct proc * proc;   /* local copy thereof */
} RUN_CHILD;

/********************************************************************
 * Candidate hash functions (string_hash is the only one used)
 */
typedef unsigned (* HASH_FUNC) ANSIARGS((char*,int,int));
                                        /* routine to use for hashing */
                                        /* comparison routine is memcmp();
                                           -1 (first less than second)
                                           0 (match)
                                           +1 (second less than first)
                                        */
unsigned string_hash ANSIARGS((char *,int,int));
unsigned long_hash ANSIARGS((long *,int,int));

union hash_entry {
      char buf[16];
      struct hi {
          long name;
          long body;
          long next;
          short int in_use;
      } hi;
};

union hash_preamble {
    char buf[4096];                    /* Disk Block */
    struct hash_header {
        int preamble_size;             /*   - Its size */
        int last_backup;               /*   - Label for recovery */
        int next_job;                  /*   - A sequence generator,
                                        *     for unique job numbers
                                        *     (rather than having
                                        *     the rather tedious Inode number,
                                        *     which keeps changing) */
        int hash_size;                 /*   - The size and position of the
                                        *     hash table */
        long hash_offset;
        long first_free;               /*   - The start and end points for the
                                        *     free space chain */
        long last_free;
        long end_space;                /*   - The current limit of the space */
        int  extent;                   /*   - The amount of space to extend
                                        *     the heap at a time */
    } hash_header;
};
typedef struct dh_con {
    FILE * fp;
    HASH_FUNC hashfunc;
    union hash_preamble hash_preamble;
    char buf[BUFSIZ];
} DH_CON;

enum tok_id {DEFINE, QM, END, USE, DIRECTORY,
SUBMITTED, HELD, RELEASED, STARTED, SUCCEEDED, FAILED,
COUNT, AVAILABILITY, MEMORY, ALGORITHM, CPU, IO, PRIORITY, START_TIME,
END_TIME, E2STR,PEOF};
enum combine_lines {DO_COMBINE,DONT_COMBINE};

/*
 * Structure to indicate disposition of the different records
 * in the database under varying circumstances
 */
struct rec_handle {
    int rec_type;
    void (*rec_function)();
}; 
extern struct rec_handle rec_purge[];
extern struct rec_handle rec_dump[];
void ind_format();
void hash_format();
void var_format();
void job_acc_format();
void job_acc_purge();
void overall_job_format();
/*************************************************************************
 * prototypes for library functions
 */
char * envstuff ANSIARGS((char * thing_to_stuff));
ENV * find_tok ANSIARGS(( QUEUE *q,enum tok_id tok_id));
char *strnsave ANSIARGS((char *s, int n));
char *filename ANSIARGS((char *fmt, char * s1,char * s2, char * s3, char * s4, char * s5));
char *user2s ANSIARGS((USER *up));
USER *s2user ANSIARGS((char *s));
FILE *openfile ANSIARGS((char *d, char *f, char *mode));
char *fileof ANSIARGS((char *s));
unsigned short rand16 ANSIARGS((void));
char *mkuniq ANSIARGS((char *template));
char *getnewfile ANSIARGS((char *directory, char *family));
int lrename ANSIARGS((char *from, char * to));
void natinit  ANSIARGS((int argc, char **argv));
void natlog  ANSIARGS((int argc, char **argv));
void checkperm ANSIARGS((int need1, QUEUE * queue));
int doperms ANSIARGS((char * perm_string));
char *pperm ANSIARGS((int perms));
char *getnextqueue ANSIARGS((void));
char *queuetodir ANSIARGS((QUEUE * queue, QUEUE ** anchor));
QUEUE *dirtoqueue ANSIARGS((char *dir, QUEUE **anchor));
int chew_sym ANSIARGS((int deflt, int sym_type));
int addqueue ANSIARGS((char *queue_file, QUEUE ** anchor));
int chgqueue ANSIARGS((char *queue_file, QUEUE ** anchor));
int delqueue ANSIARGS((char *queue_file, QUEUE **queue));
QUEUE *getqueueent ANSIARGS((QUEUE *lp));
char *grantname ANSIARGS((QUEUE * queue));
USER *getuserent ANSIARGS((FILE *fp));
USER *getuserbyname ANSIARGS((char *file, char *uname));
char *getgroupforname ANSIARGS((char *uname));
int getadminuid ANSIARGS((void));
int getadmingid ANSIARGS((void));
int adduser  ANSIARGS((QUEUE *queue, USER *up));
int chguser  ANSIARGS((QUEUE *queue, USER *from, USER *to));
char    *job_name_skel ANSIARGS((JOB * job ));
int     createjob ANSIARGS((JOB * job ));
int     renamejob ANSIARGS((JOB *old_job, JOB *new_job));
JOB     *getnextjob ANSIARGS((DIR *job_chan, QUEUE *queue));
JOB     *getjobbyfile   ANSIARGS((QUEUE *queue, char *file_name));
int     queue_open ANSIARGS(( QUEUE * queue));
JOB     *queue_fetch ANSIARGS(( QUEUE * queue));
void     queue_close ANSIARGS(( QUEUE * queue));
void     queue_rewind ANSIARGS(( QUEUE * queue));
void     queue_reorder ANSIARGS(( QUEUE * queue, int (*comp)( JOB**,JOB**)));
JOB     *job_clone ANSIARGS((JOB * old_job));
void    job_destroy ANSIARGS((JOB * old_job));
int  wakeup_natrun ANSIARGS((char *s));
DH_CON * dh_open ANSIARGS((void));
long findoveralljob_db ANSIARGS((char *jobname,OVERALL_JOB **results));
void createjob_db ANSIARGS((JOB *job));
void createjobacc_db ANSIARGS((JOB_ACC * to_store));
void job_acc_remove ANSIARGS((long hoffset, JOB_ACC * job_acc_ptr));
long findjobacc_db ANSIARGS((JOB *search_job,
                            int occurrence,JOB_ACC ** results));
void renamejob_db ANSIARGS((JOB *job));
void updatejob_db ANSIARGS((JOB *job, RESOURCE_PICTURE *used));
void deletejob_db ANSIARGS((int job_no));
JOB * findjobbynumber ANSIARGS((QUEUE *queue_anchor,int job_no));
int job_status_chg ANSIARGS((JOB* job));
FILE * log_file_open ANSIARGS((JOB *job,char **fname));
QUEUE * queue_file_open ANSIARGS((int write_flag, FILE **ret_fp,
             char *file_to_read, QUEUE **queue_anchor, QUEUE **last_queue));
int queue_file_read ANSIARGS((FILE *fp,
                       QUEUE ** queue_anchor, QUEUE ** queue_last));
void create_unix_acct ANSIARGS((JOB_ACC * job_acc));
void queue_print ANSIARGS((FILE * fp,QUEUE *q));
void queue_file_write ANSIARGS((FILE *fp,QUEUE *q));
void queue_destroy ANSIARGS((QUEUE *q));
ENV *save_env ANSIARGS((enum combine_lines combine_lines));
void print_env ANSIARGS((FILE *fp,ENV *e));
ENV *clone_env ANSIARGS((ENV *e));
void destroy_env ANSIARGS((ENV *e));
void destroy_queue_list ANSIARGS((QUEUE *anchor));
void do_define  ANSIARGS((FILE *fp,QUEUE **queue_anchor,QUEUE **queue_last));
void do_availability  ANSIARGS((FILE *fp,QUEUE *queue_anchor,QUEUE *queue));
QUEUE *find_def ANSIARGS((char *defname,QUEUE *queue_anchor));
ENV * find_tok ANSIARGS((QUEUE *q,enum tok_id tok_id));
void fill_env ANSIARGS((QUEUE *q));
QUEUE_MACHINE * machine_queue_ini ANSIARGS((void));
void machine_queue_destroy ANSIARGS((QUEUE_MACHINE *ds));
QUEUE *find_ft ANSIARGS((QUEUE *queue_anchor,char *queue_name,char *host,
                         int occurrence));
short int date_val      ANSIARGS((char * to_be_val, char * form_val, char ** where_got_to, double * secs_since_1970));
char    *to_char        ANSIARGS((char * format,double date_secs));
double local_secs        ANSIARGS((time_t gm_secs));
void varerase        ANSIARGS((char * varname));
double gm_secs        ANSIARGS((time_t gm_secs));
short int    yy_val        ANSIARGS((char * date,char ** where_gotr, double * date_secs));
enum send_state {ORD,ENDSENT,EOFSENT,EOFSEEN,ALLDONE,KILLED,DIED};
/******************************************************************************
 * SUPERNAT Global Data - mainly initialised by natinit()
 */
struct supernat_glob {
char *supernat; /* supernat directory */
char *supernatdb; /* supernat database */
int read_write; /* supernat database read_write flag */
char *username; /* user's name */
char *groupname; /* user's login group */
char *adminname; /* admin's name */
char *admingroup; /* admin's group */
char *dirname; /* directory name */
char *lock_name; /* lock file name */
char *fifo_name; /* fifo file name */
char *dbname; /* db name */
enum send_state sstate;     /* Global so that die() can signal OK */
int fifo_fd;            /* connect (socket) fd */
int listen_fd;          /* listen socket fd */
FILE * fifo;
char    *chew_glob;     /* For recognition of time details */
FILE * errout;          /* If not null, use instead of stderr */
DH_CON * cur_db;        /* Current DB flag */
int stamp_out;          /* Flag to say whether or not the log should be
                         * time-stamped
                         */
HASH_CON * sym_hash;
} sg;
/***********************************************************************
 *
 * Getopt support
 */
extern int optind;           /* Current Argument counter.
  */
extern char *optarg;         /* Current Argument pointer.
  */
extern int opterr;           /* set the error behavior */
extern int errno;
extern char ** environ;
/* special definitions */
#ifndef ADMINNAME
#define ADMINNAME "natman"
#endif /* !ADMINNAME */
#ifndef ADMINGROUP
#ifdef AIX
#define ADMINGROUP "staff"
#else
#define ADMINGROUP "daemon"
#endif /* !ADMINGROUP */
#endif /* !ADMINGROUP */
#ifndef SUPERNAT_FIFO
#ifdef MINGW32
#define SUPERNAT_FIFO "\\\\.\\pipe\\supernat_fifo"
#else
#define SUPERNAT_FIFO "/usr/spool/nat/supernat_fifo"
#endif
#endif /* !SUPERNAT_FIFO */
#ifndef SUPERNAT_LOCK
#define SUPERNAT_LOCK "/usr/spool/nat/supernat_lock"
#endif /* !SUPERNAT_LOCK */

#ifndef SUPERNATSTRING
#define SUPERNATSTRING "SUPERNAT"
#endif /* !SUPERNATSTRING */
#ifndef NAT
#define NAT "nat"
#endif /* !NAT */
#ifndef SUPERNAT_LOG
#define SUPERNAT_LOG "log"
#endif /* !SUPERNAT_LOG */

#define CHEW_INT 0
#define CHEW_TIME 1

#ifndef DEFSUPERNAT
#define DEFSUPERNAT  "/usr/spool/nat"
#endif
#ifndef SUPERNATDB
#define SUPERNATDB "SUPERNATDB"
#endif /* !SUPERNATDB */
#ifndef DEFSUPERNATDB
#define DEFSUPERNATDB "supernatdb"
#endif /* !DEFSUPERNATDB */
int natcalc_fix_flag;         /* A flag for datlib.c and natcalc.y */

#define ACTLOG "Activity"
#define UNIXACCT "sacct"
#define USERLIST "Userlist"
#define QUEUELIST  "Queuelist"
#define PAST   "past"
#define LASTTIMEDONE  "lasttimedone"

/* So that malloc's aren't done all the time */
#define CHUNK  32

 
void fhcreate ANSIARGS((char * file_name,int hash_size, int extent, int last_j));
                                  /* create the DB */
DH_CON * fhopen ANSIARGS((char * file_name,HASH_FUNC hash_to_use));
                                  /* open the DB */
int fhclose ANSIARGS((DH_CON * dh_con));
                                  /* close the DB */
void fhdomain_check ANSIARGS((DH_CON * dh_con,int debug_flag,
                               struct rec_handle * rec_dispose));
                                  /* Sanity Check the Database */
long fhmalloc ANSIARGS((DH_CON * dh_con,int size));
                                  /* allocate space in the DB */
void fhfree ANSIARGS((DH_CON * dh_con,long where));
                                  /* release space in the DB */
void fh_chg_ref ANSIARGS((DH_CON * dh_con,long where, int delta));
                                  /* Change the space reference count */
int fh_next_job_id ANSIARGS((DH_CON * dh_con));
                                  /* Get the next job ID */
int fh_get_to_mem ANSIARGS((DH_CON * dh_con,long offset,int *read_type,
                                     char ** read_ptr, int * read_len));
                                  /* Read a record preceded by its length */
int fh_put_from_mem ANSIARGS((DH_CON * dh_con,int write_type,
                               char * write_ptr,int write_len));
                                  /* Write a record preceded by its length */
long fhinsert ANSIARGS((
DH_CON * dh_con,
char * item,
int item_len,
int data_type,
char * data,
int data_len
));                                /* Insert hash, and optionally key or data */
long fhlookup ANSIARGS((
DH_CON * dh_con,
char *  item,
int item_len,
union hash_entry * hash_entry
));                                /* Find a match */
long fhremove ANSIARGS((DH_CON * dh_con,
char * item,
int item_len,
union hash_entry * hash_entry
));                                /* Delete an item */
/**********************************************************************
 * Implementations required for reliable System V.3 signal functions?
 */
#ifdef ULTRIX
#define NO_SIGHOLD
#endif
#ifdef NO_SIGHOLD
void (*sigset())();
void sighold();
void sigrelse();
#endif
#ifdef ICL
#include <sys/time.h>
int e2gettimeofday ANSIARGS((struct timeval *tv, struct timezone * tz));
#ifndef OSXDC
#ifndef SOLAR
struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;
        long    ru_ixrss;               /* XXX: 0 */
        long    ru_idrss;               /* XXX: sum of rm_asrss */
        long    ru_isrss;               /* XXX: 0 */
        long    ru_minflt;              /* any page faults not requiring I/O */
        long    ru_majflt;              /* any page faults requiring I/O */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcsw;              /* involuntary " */
};
#endif
#endif
#else
#define e2gettimeofday(x,y) gettimeofday((x),(y))
#endif
#ifdef PTX
struct timezone {
int tz_minuteswest;
};
int gettimeofday ANSIARGS((struct timeval *tv, struct timezone * tz));
struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;
        long    ru_ixrss;               /* XXX: 0 */
        long    ru_idrss;               /* XXX: sum of rm_asrss */
        long    ru_isrss;               /* XXX: 0 */
        long    ru_minflt;              /* any page faults not requiring I/O */
        long    ru_majflt;              /* any page faults requiring I/O */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcsw;              /* involuntary " */
};
#endif
#ifdef PTX4
int gettimeofday ANSIARGS((struct timeval *tv, struct timezone * tz));
struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;
        long    ru_ixrss;               /* XXX: 0 */
        long    ru_idrss;               /* XXX: sum of rm_asrss */
        long    ru_isrss;               /* XXX: 0 */
        long    ru_minflt;              /* any page faults not requiring I/O */
        long    ru_majflt;              /* any page faults requiring I/O */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcsw;              /* involuntary " */
};
#endif
/****************************************************************
 * Single Image - saves space
 */
#ifndef SINGLE_IMAGE
#define nat_main main
#define natalist_main main
#define natalog_main main
#define nataperm_main main
#define natcalc_main main
#define natdump_main main
#define natglist_main main
#define natgperm_main main
#define natq_main main
#define natqadd_main main
#define natqdel_main main
#define natqlist_main main
#define natrebuild_main main
#define natrm_main main
#define natrun_main main
#endif
/***************************************************************
 * BP non-standard AIX unlink() problem
 */
#ifndef AIX
#define e2unlink unlink
#endif
#endif /* !SUPERNAT_H */
