/*
 * natllib.c - Contains a function to write out a log record in the format of
 * a UNIX accounting record
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1992";
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>

#ifdef NT4_CYGWIN
/*
 * Accounting structures
 */
/* SVR4 acct strucutre */
struct    acct
{
    char    ac_flag;        /* Accounting flag */
    char    ac_stat;        /* Exit status */
    uid_t    ac_uid;            /* Accounting user ID */
    gid_t    ac_gid;            /* Accounting group ID */
    dev_t    ac_tty;            /* control typewriter */
    time_t    ac_btime;        /* Beginning time */
    comp_t    ac_utime;        /* acctng user time in clock ticks */
    comp_t    ac_stime;        /* acctng system time in clock ticks */
    comp_t    ac_etime;        /* acctng elapsed time in clock ticks */
    comp_t    ac_mem;            /* memory usage */
    comp_t    ac_io;            /* chars transferred */
    comp_t    ac_rw;            /* blocks read or written */
    char    ac_comm[8];        /* command name */
};
#else
#include <sys/acct.h>
#endif
#ifndef comp_t
#ifndef MINGW32
#ifndef AIX
typedef    ushort comp_t;        /* "floating point" */
        /* 13-bit fraction, 3-bit exponent  */
#endif
#endif
#endif
#include <pwd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct passwd * getpwnam();
double log();
double floor();

#ifndef AHZ
#ifdef OSF
#define AHZ 100
#else
#ifdef V4
#define AHZ 100
#else
#define AHZ 60
#endif
#endif
#endif
#include "supernat.h"
/*
 * Macro to convert comp_t to double
 */
#define comp_t_double(x) ((double) ((((double)(((comp_t) (x)) & 0x1fff)) *\
 ((double) ((((((comp_t) x)&0x2000)?8:1))))* \
 ((double) ((((((comp_t) x)&0x4000)?64:1))))* \
 ((double) ((((((comp_t) x)&0x8000)?4096:1)))))))
/*
 * Function to convert double to comp_t
 * A comp_t is a 13 bit integer (range 0-8191) multiplied by a power
 * of 8.
 *
 */
#ifndef MINGW32
static comp_t double_comp_t (x)
double x;
{
double xlog8;

    if (x < 8192.0)
        return (short int) x;
    xlog8 = ceil(((log(x + 1.0)/log(2.0) - 13.0)/3.0));
    if (xlog8 < 0.0)
        xlog8 = 0.0;
    return (comp_t) ((((short int) xlog8) & 0x07) << 13) |
             (((short int) ((x)/pow(8.0,xlog8))) & 0x1fff);
 
}
#endif
/*
 * Function to write out a UNIX accounting record. This will
 * eventually cater for the vagaries of the record format across
 * UNIX variants
 */
void create_unix_acct(job_acc)
JOB_ACC * job_acc;
{
#ifndef MINGW32
struct passwd * upwd;
static char * sacct;
static FILE * sacct_file;
struct acct pacct;
char name_buf[MAXUSERNAME+12];
int i;

    memset (&pacct,0,sizeof(pacct));
    if (sacct == (char *) NULL)
    {
        sacct = filename("%s/%s",sg.supernat,UNIXACCT);
        sacct_file = fopen(sacct, "a");
        (void) free(sacct);
    }
    if (sacct_file == (FILE *) NULL)
        return;
#ifdef SCO
    else
        setbuf(sacct_file,(char *) NULL);
#endif
    if ((upwd = getpwnam(job_acc->user_name)) == (struct passwd *) NULL)
    {
        pacct.ac_uid = -1;
        pacct.ac_gid = -1;
    }
    else
    {
        pacct.ac_uid = upwd->pw_uid;
        pacct.ac_gid = upwd->pw_gid;
    }
/*
 * The command name field. Fill with, initially, the job number, and
 * for the rest the job name
 */
    sprintf(name_buf,"%-*.*s",
                 sizeof(pacct.ac_comm),
                 sizeof(pacct.ac_comm),
                  job_acc->job_name);
    memcpy(pacct.ac_comm,name_buf, sizeof(pacct.ac_comm));
#ifdef SCO
    pacct.ac_utime = double_comp_t(job_acc->used.sigma_cpu );
#else
    pacct.ac_utime = double_comp_t(job_acc->used.sigma_cpu * (double) AHZ);
#endif
    pacct.ac_stime = (comp_t) 0;
    pacct.ac_btime = (time_t) job_acc->used.low_time;
#ifndef LINUX
    pacct.ac_mem = double_comp_t(job_acc->used.sigma_memory);
    pacct.ac_io = double_comp_t(job_acc->used.sigma_io);
#endif
    pacct.ac_tty = -1;
#ifdef AXSIG
    if (WIFEXITED(job_acc->execution_status))
        pacct.ac_flag = 0;
    else
        pacct.ac_flag = AXSIG;
#else
    pacct.ac_flag = 0;
#endif
       
#ifdef AIX
    pacct.ac_etime = double_comp_t((
                     job_acc->used.sigma_elapsed_time)*
                     ((double) AHZ) * 100.0);
#ifndef LINUX
    pacct.ac_stat = job_acc->execution_status;
    pacct.ac_rw = 0;
#endif
#else
#ifdef ULTRIX
    pacct.ac_etime = double_comp_t((
                     job_acc->used.sigma_elapsed_time));
#else
#ifdef BSD
    pacct.ac_etime = double_comp_t((
                     job_acc->used.sigma_elapsed_time)*
                     ((double) AHZ) );
    pacct.ac_stat = job_acc->execution_status;
    pacct.ac_rw = 0;
#else
    pacct.ac_etime = double_comp_t((
                     job_acc->used.sigma_elapsed_time)*
                     ((double) AHZ) );
#endif
#endif
#endif
    (void) fwrite(&pacct,1, sizeof(pacct), sacct_file);
    (void) fflush(sacct_file);
#endif
    return;
}
