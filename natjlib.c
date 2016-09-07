/*
 *    natjlib.c
 *
 *    Copyright (c) E2 Systems, UK, 1990. All rights reserved.
 *
 *    Job-related routines for nat system.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 1990\n";
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#ifdef AIX
#include <fcntl.h>
#define d_fileno d_ino
#else
#ifndef ULTRIX
#include <sys/fcntl.h>
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#ifdef SCO
#ifndef V4
#define SHORT_NAME 1
#endif
#endif
#ifndef SHORT_NAME
double floor();
#endif

#include "supernat.h"

/*
 * routine to copy a job structure
 */
JOB *
job_clone(old_job)
JOB * old_job;
{
JOB * new_job;
ENV * e, *f;

    NEW(JOB,new_job);
    *new_job = *old_job;
    new_job->job_file = strnsave(old_job->job_file,
                                 strlen(old_job->job_file));
    new_job->job_name = strnsave(old_job->job_name,
                                 strlen(old_job->job_name));
    new_job->job_owner = strnsave(old_job->job_owner,
                                 strlen(old_job->job_owner));
    new_job->job_shell = strnsave(old_job->job_shell,
                                 strlen(old_job->job_shell));
    new_job->job_mail = strnsave(old_job->job_mail,
                                 strlen(old_job->job_mail));
    new_job->job_parms = strnsave(old_job->job_parms,
                                 strlen(old_job->job_parms));
    new_job->job_restart = strnsave(old_job->job_restart,
                                 strlen(old_job->job_restart));
    new_job->job_resubmit = strnsave(old_job->job_resubmit,
                                 strlen(old_job->job_resubmit));
    new_job->job_group = strnsave(old_job->job_group,
                                 strlen(old_job->job_group));
    new_job->log_keep = strnsave(old_job->log_keep,
                                 strlen(old_job->log_keep));
    new_job->express = strnsave(old_job->express,
                                 strlen(old_job->express));
    new_job->j_submitted = clone_env(old_job->j_submitted);
                                 /* the command executed at submission */
    new_job->j_held = clone_env(old_job->j_held);
                                 /* the command executed on hold */
    new_job->j_released = clone_env(old_job->j_released);
                                 /* the command executed at release */
    new_job->j_started = clone_env(old_job->j_started);
                                 /* the command executed on startup */
    new_job->j_succeeded = clone_env(old_job->j_succeeded);
                                 /* the command executed on success */
    new_job->j_failed = clone_env(old_job->j_failed);
                                 /* the command executed on failure */
 
/*
 * The local environment stuff
 */
    for (e = old_job->j_local_env,
         new_job->j_local_env = clone_env(e),
         f = new_job->j_local_env;
            e != (ENV *) NULL;)
    {
         e = e->e_next;
         if (e != (ENV *) NULL)
         {
             f->e_next = clone_env(e);
             f = f->e_next;
         }
    }
    return new_job;
}
/*
 * job_destroy - return malloc'ed space
 *             - do not bin the job itself, in case it is static.
 */
void
job_destroy(job)
JOB * job;
{
ENV * e, *f;

    if (job->job_file != (char *) NULL)
         (void) free(job->job_file);
    if (job->job_name != (char *) NULL)
         (void) free(job->job_name);
    if (job->job_owner != (char *) NULL)
         (void) free(job->job_owner);
    if (job->job_shell != (char *) NULL)
         (void) free(job->job_shell);
    if (job->job_mail != (char *) NULL)
         (void) free(job->job_mail);
    if (job->job_parms != (char *) NULL)
         (void) free(job->job_parms);
    if (job->job_restart != (char *) NULL)
         (void) free(job->job_restart);
    if (job->job_resubmit != (char *) NULL)
         (void) free(job->job_resubmit);
    if (job->job_group != (char *) NULL)
         (void) free(job->job_group);
    if (job->log_keep != (char *) NULL)
         (void) free(job->log_keep);
    if (job->express != (char *) NULL)
         (void) free(job->express);
    job->job_no = 0;
    if (job->j_submitted != (ENV *) NULL)
        destroy_env(job->j_submitted);
                                 /* the command executed at submission */
    if (job->j_held != (ENV *) NULL)
        destroy_env(job->j_held);
                                 /* the command executed on hold */
    if (job->j_released != (ENV *) NULL)
        destroy_env(job->j_released);
                                 /* the command executed at release */
    if (job->j_started != (ENV *) NULL)
        destroy_env(job->j_started);
                                 /* the command executed on startup */
    if (job->j_succeeded != (ENV *) NULL)
        destroy_env(job->j_succeeded);
                                 /* the command executed on success */
    if (job->j_failed != (ENV *) NULL)
        destroy_env(job->j_failed);
                                 /* the command executed on failure */
 
/*
 * The local environment stuff
 */
    for (e = job->j_local_env;
            e != (ENV *) NULL;)
    {
         f = e->e_next;
         destroy_env(e);
         e = f;
    }
    return;
}
/*
 * Function to read in a status code trigger
 */
static int trig_read(job, env, search_text, tok_id,combine_lines)
JOB * job;
ENV ** env;
char * search_text;
enum tok_id tok_id;
enum combine_lines combine_lines;
{
char buf[BUFSIZ];

    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strcmp(search_text,buf))
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected %s\n",search_text);
#endif
        return 0;
    }
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: unexpected %s EOF\n",
                       search_text);
#endif
        return 0;
    }
    if (!strcmp("##\n",buf))
    {
        if ((*env = find_tok(job->job_queue,tok_id)) != (ENV *) NULL)
            *env = clone_env(*env);
    }
    else
    {
        ENV  *cont;
        int len;

        NEW (ENV, *env);
        cont = *env;
        len = strlen(buf+2);
        cont->e_cont = (ENV *) NULL;
        cont->e_next = (ENV *) NULL;
        cont->e_env = strnsave(buf+2,len);
        while (fgets(buf,BUFSIZ,job->job_chan) != NULL &&
               strcmp("##\n",buf))
        {
            int i;
            NEW (ENV,cont->e_cont);
            cont = cont->e_cont;
            i = strlen(buf+2);
            len += i;
            cont->e_env = strnsave(buf+2,i);
            cont->e_cont = (ENV *) NULL;
            cont->e_next = (ENV *) NULL;
        }
        if (combine_lines == DO_COMBINE)
        {
            register char * x1, *x2;
            char *x3;
            x2 = (char *) malloc(len + 1);
            cont = *env;
            x1 = cont->e_env; 
            x3 = x2;
            for (;;)
            {
                ENV * prev;
                prev = cont;
                while (*x1 != '\0')
                   *x2 ++ = *x1++;
                cont = cont->e_cont;
                (void) free(prev->e_env);
                if (prev != *env)
                    (void) free(prev);
                if (cont == (ENV *) NULL)
                    break;
                x1 =  cont->e_env;
            }
            *x2 = '\0';
            (*env)->e_env = x3;
            (*env)->e_cont = (ENV *) NULL;
        }
    }
    return 1;
}
/*
 * Function to write out a status code trigger
 */
static void trig_write(job, env, search_text, combine_lines)
JOB * job;
ENV ** env;
char * search_text;
enum combine_lines combine_lines;
{
    (void) fprintf(job->job_chan,"%s",search_text);
    if (*env != (ENV *) NULL)
    {
        ENV * next;
        for( next = *env;
                 next != (ENV *) NULL;
                     next = next->e_next)
        {
            ENV  *cont;
            for( cont = next;
                     cont != (ENV *) NULL;
                         cont = cont->e_cont)
                if (combine_lines == DO_COMBINE)
                {
                    char * x1, *x2;
                    x1 = strnsave(cont->e_env,strlen(cont->e_env));
                    for ( x2 = strtok(x1,"\n");
                              x2 != (char *) NULL;
                                  x2 = strtok(NULL,"\n"))
                        (void) fprintf(job->job_chan,"##%s\n",x2);
                    (void) free(x1);
                }
                else
                {
                    (void) fprintf(job->job_chan,"##%s\n",cont->e_env);
                }
        }
    }
    (void) fprintf(job->job_chan,"##\n");
    return;
}
/*
 * Pick up the job details from the header
 */
static int job_head_read(job)
JOB * job;
{
char * x;
char buf[BUFSIZ];
char job_name[MAXUSERNAME];
char job_owner[MAXUSERNAME];
char job_shell[MAXUSERNAME];
char job_mail[MAXUSERNAME];
char job_parms[BUFSIZ];
char job_group[MAXUSERNAME];
char job_restart[MAXUSERNAME];
char job_resubmit[MAXUSERNAME];
char log_keep[MAXUSERNAME];
char express[MAXUSERNAME];
int xnice;

   if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# owner: ",buf,9)
        || (x = strtok(&buf[9]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected owner\n");
#endif
        return 0;
    }
    STRCPY(job_owner,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# jobname: ",buf,11)
        || (x = strtok(&buf[11]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected jobname\n");
#endif
        return 0;
    }
    STRCPY(job_name,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# shell: ",buf,9)
        || (x = strtok(&buf[9]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected shell\n");
#endif
        return 0;
    }
    STRCPY(job_shell,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# notify by mail: ",buf,18)
        || (x = strtok(&buf[18]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected notify by mail\n");
#endif
        return 0;
    }
    STRCPY(job_mail,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# parameters: ",buf,14))
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected parameters\n");
#endif
        return 0;
    }
    if ((x = strtok(&buf[14],"\r\n")) == NULL)
        job_parms[0]='\0';
    else STRCPY(job_parms,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# restartable: ",buf,15)
        || (x = strtok(&buf[15]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected restartable\n");
#endif
    
        return 0;
    }
    STRCPY(job_restart,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# resubmission: ",buf,16)
        || (x = strtok(&buf[16]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected resubmission\n");
#endif
        return 0;
    }
    STRCPY(job_resubmit,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# group: ",buf,9)
        || (x = strtok(&buf[9]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected group\n");
#endif
        return 0;
    }
    STRCPY(job_group,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# keep: ",buf,8)
        || (x = strtok(&buf[8]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected keep\n");
#endif
        return 0;
    }
    STRCPY(log_keep,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# express: ",buf,11)
        || (x = strtok(&buf[11]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected express\n");
#endif
        return 0;
    }
    STRCPY(express,x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# nice: ",buf,8)
        || (x = strtok(&buf[8]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected nice\n");
#endif
        return 0;
    }
    xnice = atoi(x);
    if (fgets(buf,BUFSIZ,job->job_chan) == NULL
        || strncmp("# number: ",buf,10)
        || (x = strtok(&buf[10]," \t\r\n")) == NULL)
    {
#ifdef DEBUG
        (void) fprintf(stderr,"Format error: expected job number\n");
#endif
        return 0;
    }
    job->job_no = atoi(x);

    if (!trig_read(job, &job->j_submitted, "# submitted:\n", 
                    SUBMITTED, DO_COMBINE))
        return 0;
    if (!trig_read(job, &job->j_held, "# held:\n", 
                    HELD, DO_COMBINE))
        return 0;
    if (!trig_read(job, &job->j_released, "# released:\n", 
                    RELEASED, DO_COMBINE))
        return 0;
    if (!trig_read(job, &job->j_started, "# started:\n", 
                    STARTED, DO_COMBINE))
        return 0;
    if (!trig_read(job, &job->j_succeeded, "# succeeded:\n", 
                    SUCCEEDED, DO_COMBINE))
        return 0;
    if (!trig_read(job, &job->j_failed, "# failed:\n", 
                    FAILED, DO_COMBINE))
        return 0;
    if (!trig_read(job, &job->j_local_env, "# environment:\n", 
                    E2STR, DONT_COMBINE))
        return 0;
    job->job_name = strnsave(job_name,strlen(job_name));
    job->job_owner = strnsave(job_owner,strlen(job_owner));
    job->job_shell = strnsave(job_shell,strlen(job_shell));
    job->job_mail = strnsave(job_mail,strlen(job_mail));
    job->job_parms = strnsave(job_parms,strlen(job_parms));
    job->job_restart = strnsave(job_restart,strlen(job_restart));
    job->job_resubmit = strnsave(job_resubmit,strlen(job_resubmit));
    job->job_group = strnsave(job_group,strlen(job_group));
    job->log_keep = strnsave(log_keep,strlen(log_keep));
    job->express = strnsave(express,strlen(express));
    job->nice = xnice;
    return 1;
}
/*
 * getnextjob()
 * - Function reads the next job record
 * - Current directory must be the queue
 */
JOB *
getnextjob(job_chan,queue)
DIR * job_chan;
QUEUE * queue;
{
JOB * ret_job;
#ifdef POSIX
struct dirent * job_dir;
#else
struct direct * job_dir;
#endif
#ifdef POSIX
    while ((job_dir = readdir(job_chan)) != (struct dirent *) NULL)
#else
    while ((job_dir = readdir(job_chan)) != (struct direct *) NULL)
#endif
    {
        if ((ret_job = getjobbyfile(queue,job_dir->d_name)) != (JOB *)
                 NULL)
        {
            if (ret_job->job_no == 0)
            {
                job_destroy(ret_job);
                free(ret_job);
            }
            else
                return ret_job;
        }
    }           /* end of loop through the files in the directory */
    return (JOB *) NULL;
}
/*
 * getjobbyfile()
 * Routine to read a file as a job, and set up the JOB record.
 */
JOB * getjobbyfile(queue,file_name)
QUEUE * queue;
char * file_name;
{
struct stat job_stat;
JOB * ret_job;
FILE * com_chan;
JOB_ACC job_acc;
double secs;
time_t jtime;
long year;
long jday;
char * x;
char restart_char;
#ifdef SHORT_NAME
int rec;
#endif
static char buf[BUFSIZ];
time_t job_execute_time;
int dummy;
char * path_to_open;
        
/*
 * Re-initialise the static structure
 * It is the responsibility of the calling routine to return malloc'ed space
 */
    restart_char = '\0';
    NEW(JOB,ret_job);
    ret_job->job_no = 0;
    ret_job->job_queue = queue;
    ret_job->job_file = (char *) NULL;
    ret_job->job_chan = (FILE *) NULL;
    ret_job->job_create_time = 0;
    ret_job->job_execute_time = 0;
    ret_job->job_name = (char *) 0;
    ret_job->job_owner = (char *) 0;
    ret_job->job_group = (char *) 0;
    ret_job->job_shell = (char *) 0;
    ret_job->job_mail = (char *) 0;
    ret_job->job_parms = (char *) 0;
    ret_job->job_restart = (char *) 0;
    ret_job->job_resubmit = (char *) 0;

#ifdef SHORT_NAME
    if ((rec = sscanf(file_name,
            "%lu.%d",&job_execute_time,&dummy)) != 2)
    {
        if (sscanf(file_name,
            "X%c%lu.%u",&restart_char,&job_execute_time,&dummy) != 3)
        {
             (void) free(ret_job);
             return (JOB *) NULL;     /* not a job file */
        }
    }
#else
    if (sscanf(file_name,
            "%lu.%lu.%lu.%u",&year,&jday,&jtime,&dummy) != 4)
    {
        if (sscanf(file_name,
            "X%c%lu.%lu.%lu.%u",&restart_char,&year,&jday,&jtime,&dummy) != 5)
        {
             (void) free(ret_job);
             return (JOB *) NULL;     /* not a job file */
        }
        else if (!yyyy_val(file_name+2,&x,&secs))
        {
             (void) free(ret_job);
             return (JOB *) NULL;     /* not a job file */
        }
    }
    else if (!yyyy_val(file_name,&x,&secs))
    {
        (void) free(ret_job);
        return (JOB *) NULL;     /* not a job file */
    }
/*
 * Compute the time from the year, julian and time (use yyyy_val() on d_name;
 * value should match year; 
 */
    job_execute_time = (time_t) ( secs + 86400.0*((double) jday) +
                     60.0*(60.0*((double) (jtime/100)) +
                    (double) ((jtime % 100))));
#endif
    path_to_open = filename("%s/%s",queue->queue_dir,file_name);
    if ((com_chan = fopen(path_to_open,"r")) == (FILE *) NULL)
    {
        (void) free(path_to_open);
        (void) free(ret_job);
        return (JOB *) NULL;     /* not a job file */
    }
    else
        setbuf(com_chan,buf);
/*
 * Get the job details off the spool file and from elsewhere
 * Assume errors are due to files still being created.
 */
    if (stat(path_to_open,&job_stat) < 0)
    {
#ifdef DEBUG
        perror("Stat on job file failed");
#endif
        (void) free(path_to_open);
        (void) free(ret_job);
        (void) fclose(com_chan);
        return (JOB *) NULL;     /* not a job file */
    }
    ret_job->job_chan = com_chan;
    if (job_head_read(ret_job))
    {                         /* job_no is zero for jobs still being created */
        if (ret_job->job_no != 0)
        {
            JOB_ACC * found_acc;
            memset((char *)&job_acc,'\0',sizeof(job_acc));
            job_acc.job_no = ret_job->job_no;
            if (findjobacc_db(&job_acc,0, &found_acc))
            {
                ret_job->status = found_acc->status;
                (void) free(found_acc);
            }
        }
    }
    else
    {
        (void) fclose(ret_job->job_chan);
        (void) free(path_to_open);
        (void) job_destroy(ret_job);
        (void) free(ret_job);
        return (JOB *) NULL;     /* not a job file */
    }
    (void) fclose(ret_job->job_chan);
    ret_job->job_chan = (FILE *) NULL;
    ret_job->job_file = strnsave(file_name,strlen(file_name));
    ret_job->job_create_time = job_stat.st_ctime;
    ret_job->job_execute_time = job_execute_time;
    (void) free(path_to_open);
    return ret_job;
}
/*
 * routine to fill in the job file name, given the execution time
 */
char * job_name_skel(job)
JOB * job;
{
static char new_name_skel[MAXFILENAME];
#ifdef SHORT_NAME
    (void) sprintf(new_name_skel,"%u.%%d", job->job_execute_time);
#else
    char * x;
    char buf[BUFLEN];
    double file_time,begin_of_year;
    file_time = (double) job->job_execute_time;
    (void) date_val(to_char("YYYY",file_time),"YYYY",&x,&begin_of_year);
    (void) sprintf(buf,"%s.%g.", 
                   to_char("YYYY",file_time),
                    floor((file_time-begin_of_year)/86400.0));
    (void) sprintf(new_name_skel,"%s%s.%%d",buf,
                   to_char("HH24MI",file_time));
#endif 
    return new_name_skel;    
}
/*
 * Routine to create job files.
 * WARNING:
 * - There is a potential problem if a new job is created that has
 *   the same name as a potentially executing job.
 */
static int job_file_create(job)
JOB * job;
{
char new_path[BUFLEN];
char new_name[MAXFILENAME];
char * new_name_skel;
int i;
int wfd;

    new_name_skel = job_name_skel(job);
#ifdef SHORT_NAME
/*
 * Problems keeping within the 14 character file name length maximum
 */
     for ( i = rand16() % 97,
           (void) sprintf(new_name,new_name_skel,i++ % 97),
           (void) sprintf(new_path,"%s/%s",job->job_queue->queue_dir, new_name);
               (wfd=open(new_path,O_CREAT | O_WRONLY | O_EXCL,0x30L)) < 0;
                   (void) sprintf(new_name,new_name_skel,i++ % 97),
                   (void) sprintf(new_path,"%s/%s",
                                      job->job_queue->queue_dir, new_name))
#else
     for (i = rand16(),
          (void) sprintf(new_name,new_name_skel,i++),
          (void) sprintf(new_path,"%s/%s", job->job_queue->queue_dir, new_name);
#ifdef ULTRIX
          (wfd=open(new_path,O_CREAT | O_WRONLY | O_EXCL | O_BLKANDSET,0x30L))
                          < 0;
#else
              (wfd=open(new_path,O_CREAT | O_WRONLY | O_EXCL ,0x30L)) < 0;
#endif
                  (void) sprintf(new_name,new_name_skel,i++),
                  (void) sprintf(new_path,"%s/%s",
                                      job->job_queue->queue_dir, new_name))
#endif
        if (errno != EEXIST)
        {
            perror("Job file create failed");
            return -1;
        }
    if (job->job_file != (char *) NULL)
        (void) free(job->job_file);
    job->job_file = strnsave(new_name,strlen(new_name));
    (void) chown(new_path,getadminuid(), getadmingid());
    return wfd;
}
/*
 * Routine to stuff environment variables, so that the shell syntax is valid
 */
char * envstuff(in)
char * in;
{
char buf[BUFSIZ];
register char *x1=in, *x2=buf;

    while (*x1 != '\0')
    {
        if (*x1 == '\'')
        {
            *x2++ = '\'';
            *x2++ = '\\';
            *x2++ = '\'';
        }
        *x2++ = *x1++;
    }
    *x2 = '\0';
    return strnsave(buf,x2 - buf);
}
/*
 * The job header
 */
static void job_head_write(job)
JOB* job;
{
    (void) fprintf(job->job_chan,"# owner: %s\n",job->job_owner);
    (void) fprintf(job->job_chan,"# jobname: %s\n",job->job_name);
    (void) fprintf(job->job_chan,"# shell: %s\n",job->job_shell);
    (void) fprintf(job->job_chan,"# notify by mail: %s\n",job->job_mail);
    (void) fprintf(job->job_chan,"# parameters: %s\n",job->job_parms);
    (void) fprintf(job->job_chan,"# restartable: %s\n",job->job_restart);
    (void) fprintf(job->job_chan,"# resubmission: %s\n",job->job_resubmit);
    (void) fprintf(job->job_chan,"# group: %s\n",job->job_group);
    (void) fprintf(job->job_chan,"# keep: %s\n",job->log_keep);
    (void) fprintf(job->job_chan,"# express: %s\n",job->express);
    (void) fprintf(job->job_chan,"# nice: %d\n",job->nice);
    (void) fprintf(job->job_chan,"# number: %d\n",job->job_no);
    trig_write(job, &job->j_submitted, "# submitted:\n", DO_COMBINE);
    trig_write(job, &job->j_held, "# held:\n", DO_COMBINE);
    trig_write(job, &job->j_released, "# released:\n", DO_COMBINE);
    trig_write(job, &job->j_started, "# started:\n", DO_COMBINE);
    trig_write(job, &job->j_succeeded, "# succeeded:\n", DO_COMBINE);
    trig_write(job, &job->j_failed, "# failed:\n", DO_COMBINE);
    trig_write(job, &job->j_local_env, "# environment:\n", DONT_COMBINE);
    return;
}
/*
 * Routine to start the creation of a new job file; returns the channel
 * it is open on in the JOB pointed to by the passing routine
 * This must be executed by a user rather than a server process,
 * because of the way that it picks up the environment stuff.
 */
extern char ** environ;
#ifdef SCO
extern char * getcwd();
#endif
#ifdef PTX
extern char * getcwd();
#endif
int createjob(job)
JOB * job;
{
#ifdef SCO
static char wbuf[BUFSIZ];
#endif
char buf[BUFLEN];
char * x;
char ** envp;
int wfd;
int oumask,numask=0x7; 

    oumask = umask(numask);     /* set the protections for the job */
    job->job_no = 0;            /* Job files are created with job number
                                 * zero initially
                                 */
    if ((wfd = job_file_create(job)) < 0)
        return 0;
    if ((job->job_chan = fdopen(wfd,"w")) == (FILE *) NULL)
    {
        (void) perror("Job File open failed");
        return 0;
    }
#ifdef SCO
    else
        setbuf(job->job_chan, wbuf);
#endif
    job_head_write(job);
    (void) fprintf(job->job_chan,"umask %#o\n",oumask);

#ifdef SCO
    if (getcwd(buf,sizeof(buf)) == (char *) NULL)
#else
#ifdef PTX
    if (getcwd(buf,sizeof(buf)) == 0)
#else
    if (getwd(buf) == 0)
#endif
#endif
    {
    (void) fprintf(stderr,"getwd() failed to find your current directory\n");
        (void) fprintf(stderr,"Error:%d :%s\n",errno,buf);
        perror("getwd()");
        (void) fclose(job->job_chan);
        job->job_chan = (FILE *) NULL;
        return 0;
    }
    (void) fprintf(job->job_chan,"cd %s\n",buf);
/*
 * Loop through the strings in the environment
 */
    for (envp = environ; *envp != (char *) NULL; envp++)
    {
/*
 * TERM and TERMCAP are not meaningful
 * PATH has been corrupted by the security features
 */
        if (strncmp("TERM=",*envp,5) && strncmp("PATH=",*envp,5) &&
            strncmp("TERMCAP=",*envp,8))
        {
            if ((x = strchr(*envp,'=')) != (char *) NULL)
            {
                char * stuffed_val, * save_env;
                stuffed_val = envstuff(x+1);
                if (strncmp(*envp,"NATPATH=",8))
                    save_env = strnsave(*envp, (x - *envp));
                else
                    save_env = strnsave(*envp + 3, (x - *envp - 3));
                                                 /* restore the original PATH */
                (void) fprintf(job->job_chan,"%s='%s'\n",save_env,stuffed_val);
                (void) fprintf(job->job_chan,"export %s\n",save_env);
                (void) free(stuffed_val);
                (void) free(save_env);
            }
        }
    }
    return 1;
}
/*
 * Routine for changing job attributes
 * Uses a link if possible, otherwise re-writes and changes
 *
 * This routine has a few problems
 * - The job file name includes the time of execution and the
 *   current execution/restart status
 * - The job structure includes the execution time separately
 * - The routine has to make up its mind as to what is what
 * - What actually happens is that if the times are different, it
 *   takes it upon itself to redo the name
 * - There is a possible race condition with the rename of jobs;
 *   the new job name may come into existence between checking
 *   and actually creating 
 */
int renamejob(old_job,new_job)
JOB * old_job;
JOB * new_job;
{
char rbuf[BUFSIZ];
char wbuf[BUFSIZ];
char name1[BUFLEN];
char name2[BUFLEN];
char * tmp;
int oumask,numask=0x7; /* Make sure the mode flags on creation
                                are the ones that we want */

    oumask = umask(numask);
    (void) sprintf(name1,"%s/%s",
      old_job->job_queue->queue_dir,old_job->job_file);
    if (old_job->job_execute_time != new_job->job_execute_time)
    {          /* if the times are different, need a new name2 */
        int wfd;
        if ((wfd = job_file_create(new_job)) < 0)
        {
            ERROR(__FILE__,__LINE__,0,"Job File Create in renamejob() failed\n",
                     0,0);
            (void) umask(oumask);
            return 0;
        }
        (void) close(wfd);
    }
    (void) sprintf(name2,"%s/%s",
      new_job->job_queue->queue_dir,new_job->job_file);
/*
 * At this point, if name1 and name2 are different, kill off name2
 */
    if (strcmp(name1,name2))
        (void) e2unlink(name2);
    if ( strcmp(new_job->job_name, old_job->job_name) ||
          strcmp(new_job->job_owner , old_job->job_owner) ||
          strcmp(new_job->job_shell , old_job->job_shell) ||
          strcmp(new_job->job_mail , old_job->job_mail) ||
          strcmp(new_job->job_parms , old_job->job_parms) ||
          strcmp(new_job->job_restart , old_job->job_restart) ||
          strcmp(new_job->job_resubmit , old_job->job_resubmit) ||
          strcmp(new_job->job_group , old_job->job_group) ||
          strcmp(new_job->log_keep , old_job->log_keep) ||
          strcmp(new_job->express , old_job->express) ||
                (new_job->job_no != old_job->job_no) ||
                (new_job->nice != old_job->nice) ||
          (strcmp(name1,name2) && lrename(name1,name2)))
    {
/*
 * Come here if the header needs to be rewritten, or the rename
 * failed
 */
        short int i;
        int com_fd;
    char buf[BUFSIZ];
        if (old_job->job_chan != (FILE *) NULL)
            (void) fseek(old_job->job_chan,0,0);
        else
        {
#ifdef MINGW32
            if ((com_fd = open(name1,
                 O_RDONLY | O_EXCL, 0)) < 0)
#else
#ifndef ULTRIX
            if ((com_fd = open(name1,
                 O_RDONLY | O_NDELAY | O_EXCL, 0)) < 0)
#else
            if ((com_fd = open(name1,
                 O_RDONLY | O_NDELAY | O_BLKINUSE, 0)) < 0)
#endif
#endif
            {
                ERROR(__FILE__,__LINE__,0,
                  "Job file open in renamejob() failed, errno %d name %s\n",
                     errno,name1);
                return 0;                   /* Still being created */
            }
            if ((old_job->job_chan = fdopen(com_fd,"r")) == (FILE *) NULL)
            {
                ERROR(__FILE__,__LINE__,0,
                  "Job file fdopen in renamejob() failed, errno %d name %s\n",
                     errno,name1);
                return 0;                   /* someone else has got it? */
            }
            else
                setbuf(old_job->job_chan,rbuf);
        }
        if (!strcmp(name1,name2))
        {  /* if the names are the same, need a temporary file */
            tmp = getnewfile(sg.supernat,"tmp");
            new_job->job_chan =fopen(tmp,"w");
        }
        else new_job->job_chan =fopen(name2,"w");
        (void) umask(oumask);
        if (new_job->job_chan  == (FILE *) NULL)
        {
            ERROR(__FILE__,__LINE__,0,
                 "New Job file open in renamejob() failed, errno %d, name %s\n",
                     errno,(char *)((strcmp(name1,name2))
                                     ?((int) name2)
                                     :((int) tmp)));
            (void) fclose(old_job->job_chan);
            if (!strcmp(name1,name2))
                (void) free(tmp);
            old_job->job_chan = (FILE *) NULL;
            return 0; 
        }
        else
            setbuf(new_job->job_chan,wbuf);
        if (!job_head_read(old_job))
        {
            ERROR(__FILE__,__LINE__,0,
                  "Job head read in renamejob() failed, errno %d name %s\n",
                     errno,name1);
            (void) fclose(old_job->job_chan);
            old_job->job_chan = (FILE *) NULL;
            (void) fclose(new_job->job_chan);
            new_job->job_chan = (FILE *) NULL;
            if (!strcmp(name1,name2))
                (void) free(tmp);
            return 0; 
        }
        job_head_write(new_job);
            
        while((i=fread(buf,sizeof(char),BUFSIZ,old_job->job_chan)) > 0)
            (void) fwrite(buf,sizeof(char),i,new_job->job_chan);
        (void) fclose(old_job->job_chan);
        old_job->job_chan = (FILE *) NULL;
        (void) fclose(new_job->job_chan);
        new_job->job_chan = (FILE *) NULL;
        (void) e2unlink(name1);        /* kill off the old name */
        if (!strcmp(name1,name2))
        {
           int r;
           if ((r = lrename(tmp,name2)) <0)
           {
               ERROR(__FILE__,__LINE__,0,
                  "File rename in renamejob() failed, errno %d name %s\n",
                     errno,tmp);
           }
           (void) free(tmp);
           if (r < 0)
               return 0;
        }
    }
    else
        (void) umask(oumask);
    return new_job->job_no;
}
/************************************************************
 * Functions that do the database updates associated with a job
 * - createjob_db()
 * - renamejob_db()
 *
 * The question arises, just what are we storing in this database?
 * - Queue definitions are still in clear text in a file; we are reading
 *   all these in together. There shouldn't be many of them.
 * - Variables. Essentially, name-value pairs.
 * - Do we need:
 *   Job Number/Execution Time?
 *   Execution Time/Job Number (B*Tree for this!)
 *   Job Number/Status
 *   Job Number/Filename
 *   Job Name/Job Number (and therefore, status)
 *   Job Number/Resource
 *   Job Name/Resource
 *   User/Resource?
 *   Two possible approaches; multi-indexed records, or multiple
 *   pairs?
 *   What do we really need?
 *   -  Job Accounting Records? Yes.
 *   -  Job Number/Status? Yes.
 *   -  Job Name/Status? Yes.
 *   Variable is single valued. Job Name Status is the status of the
 *   most recent job with that name. We use lower case characters
 *   to distinguish ours from user-defined variables. 
 *
 * We run with natcalc() for the moment, pending integration of
 * Alistairs DB and interpreter.
 */
DH_CON * dh_open()
{
    if (sg.cur_db == (DH_CON *) NULL)
        sg.cur_db = fhopen(sg.supernatdb,string_hash);
    return sg.cur_db;
}
long findoveralljob_db(jobname,results)
char * jobname;
OVERALL_JOB ** results;
{
OVERALL_JOB overall_job, *ojp;
DH_CON * one_shot;
union hash_entry hash_entry;
long hoffset;

    one_shot = sg.cur_db;
    if (one_shot == (DH_CON *) NULL)
        (void) dh_open();
    for ( hash_entry.hi.in_use = 0,
          hoffset = fhlookup(sg.cur_db,jobname,strlen(jobname),
                             &hash_entry);
              hoffset > 0;
          hoffset = fhlookup(sg.cur_db,jobname,strlen(jobname),
                             &hash_entry))
    {
                            /* read the hash entry, see if O.K. */
        int rl;
        int rt;
        (void) fh_get_to_mem(sg.cur_db,hash_entry.hi.body,
                             &rt,(char **) &ojp,&rl); 
        if (rt == OVERALL_JOB_TYPE)
        {
            (void) memcpy(&overall_job,ojp,rl);
            (void) free(ojp);
            break;
        }
        (void) free(ojp);
    }
    if (one_shot == (DH_CON *) NULL)
    {
        (void) fhclose(sg.cur_db);
        sg.cur_db = (DH_CON *) NULL;
    }
    if (hoffset == 0)       /* No Overall Job */
        return 0;
    else
    {
        NEW(OVERALL_JOB,*results);
        **results = overall_job;
        return hash_entry.hi.body;
    }
}
/* createjob_db()
 * - Open the database if not already open
 * - Allocate a new job number
 * - Fill in the accounting record
 * - Call job_status_chg() to perform the necessary actions
 * - Call the job rename function to fix the job number
 * - Add the accounting record
 * - Add the index entries
 * - Add a new job name (overall job record) if necessary
 * - Close the database if it had to be opened especially.
 */
void createjob_db(job)
JOB * job;
{
JOB_ACC job_acc;
OVERALL_JOB overall_job, *oj_ptr;
DH_CON * one_shot;
JOB * old_job;
union hash_entry hash_entry;
int rl;
long hoffset;

    one_shot = sg.cur_db;
    if (one_shot == (DH_CON *) NULL)
        (void) dh_open();
    old_job = job_clone(job);
    if (job->job_no == 0)
        job->job_no = fh_next_job_id(sg.cur_db);
    memset((char *)&job_acc,'\0',sizeof(job_acc));
    job_acc.job_no = job->job_no;
    job_acc.planned_time = job->job_execute_time;
    job_acc.execution_status = 0;
    STRCPY(job_acc.queue_name,job->job_queue->queue_name);
    STRCPY(job_acc.user_name,job->job_owner);
    STRCPY(job_acc.job_name,job->job_name);
/*
 * Perform the actions indicated by the job status
 */
    (void) job_status_chg(job);
    (void) renamejob(old_job,job);
    job_destroy(old_job);
    (void) free(old_job);
    job_acc.status = job->status;
    createjobacc_db(&job_acc);
    if (!findoveralljob_db(job_acc.job_name,&oj_ptr))
    {               /* Need to add an Overall Job record */
        memset((char *)&overall_job,'\0',sizeof(overall_job));
        STRCPY(overall_job.job_name,job_acc.job_name);
        overall_job.res_used = job_acc.used;
        hash_entry.hi.in_use = 0;
        if (!fhinsert(sg.cur_db,overall_job.job_name,
                    strlen(overall_job.job_name),OVERALL_JOB_TYPE,
                    (char *) &overall_job, sizeof(overall_job)))
        {
            fprintf(sg.errout,
                     "createjob_db() - Failed to create OVERALL_JOB\n");
            return;
        }
    }
    else
        (void) free(oj_ptr);
/*
 * Insert the accounting record indexed on the job number
 */
    if (one_shot == (DH_CON *) NULL)
    {
        (void) fhclose(sg.cur_db);
        sg.cur_db = (DH_CON *) NULL;
    }
    return;
}
/*
 * findjobacc_db()
 * - Open the database if not already open
 * - Search for a match; and-ing, not or-ing:
 * - Choose search key in order of:
 *   - job_no
 *   - job_name
 *   - user-name
 *   - queue_name
 * - Check returned value for match on other fields.
 * - Close the database if it had to be opened especially.
 * - occurrence == 0 is look for the current record; logic relies on
 *   times being much larger than occurrence values.
 */
long findjobacc_db(search_job, occurrence, results)
JOB_ACC * search_job;
int occurrence;
JOB_ACC ** results;
{
union hash_entry hash_entry;
long hoffset;
JOB_ACC job_acc,*jap;
DH_CON * one_shot;
char * search_mem;
int search_len;
int found_occ;
int rl;
int rt;

    one_shot = sg.cur_db;
    if (one_shot == (DH_CON *) NULL)
        (void) dh_open();
    if (search_job->job_no != 0)
    {
        search_mem = (char *) &search_job->job_no;
        search_len = sizeof(int);
    }
    else
    {
        if (strcmp(search_job->job_name,""))
            search_mem = search_job->job_name;
        else
        if (strcmp(search_job->user_name,""))
            search_mem = search_job->user_name;
        else
            search_mem = search_job->queue_name;
        search_len = strlen(search_mem);
    }

    for ( hash_entry.hi.in_use = 0,
          found_occ =0,
          hoffset = fhlookup(sg.cur_db,search_mem,search_len, &hash_entry);
              hoffset > 0;
          hoffset = fhlookup(sg.cur_db,search_mem,search_len, &hash_entry))
    {
        (void) fh_get_to_mem(sg.cur_db,hash_entry.hi.body,
                             &rt,(char **) &jap,&rl); 
        (void) memcpy(&job_acc,jap,sizeof(job_acc));
        (void) free(jap);
/*
 * Match if:
 * - right type
 * - and search keys are null, or match
 */
        if (rt == JOB_ACC_TYPE &&
            (!strcmp(search_job->job_name,"") ||
            !strcmp(search_job->job_name,job_acc.job_name)) &&
            (search_job->job_no == 0 ||
             search_job->job_no == job_acc.job_no) &&
            ( !strcmp(search_job->queue_name,"") ||
            !strcmp(search_job->queue_name,job_acc.queue_name)) &&
            (!strcmp(search_job->user_name,"") ||
            !strcmp(search_job->user_name,job_acc.user_name)))
        {
            found_occ++;
            if (found_occ == occurrence || occurrence == job_acc.used.low_time)
            {
                hoffset = hash_entry.hi.body;
                break;
            }
        }
    }
    if (one_shot == (DH_CON *) NULL)
    {
        (void) fhclose(sg.cur_db);
        sg.cur_db = (DH_CON *) NULL;
    }
    if (hoffset)
    {
        NEW(JOB_ACC,*results);
        **results = job_acc;
        return hoffset;
    }
    else
        return 0L;
}
/*
 * updatejob_db()
 * - Open the database if not already open
 * - Find the corresponding job accounting record
 * - If called with a resource picture
 *   - Update with resource picture
 *   - Update overall job resource picture
 * - if it is now submitted, held or released
 *   - Construct a new job accounting record
 *   - Write out a new record.
 *   - Add the index entries
 * - Close the database if it had to be opened especially.
 */
void updatejob_db(job, used)
JOB * job;
RESOURCE_PICTURE * used;
{
int rl;
long hoffset,  soffset;
JOB_ACC job_acc, search_job_acc, * job_acc_ptr;
DH_CON * one_shot;

    one_shot = sg.cur_db;
    if (one_shot == (DH_CON *) NULL)
        (void) dh_open();
    memset((char *)&job_acc,'\0',sizeof(job_acc));
    job_acc.job_no = job->job_no;
    (void) memset((char *) &search_job_acc,0,sizeof(search_job_acc));
    search_job_acc.job_no = job->job_no;
/*
 * We find the current one by looking for a zero occurrence value
 */
    if (!(hoffset = findjobacc_db(&search_job_acc, 0,&job_acc_ptr)))
    {
        (void) fprintf(stderr,
             "updatejob_db() Job No: %d  Cannot find Job Accounting Template\n",
                job->job_no);
        return;              /* Should never happen; how about an error! */
    }
    if (used != (RESOURCE_PICTURE *) NULL)
    {
        OVERALL_JOB  *oj_ptr;
        job_acc_ptr->used = *used;
        if ((soffset = findoveralljob_db(job->job_name,&oj_ptr)) == 0)
        {
            (void) fprintf(stderr,
            "updatejob_db() Job Name: %s No: %d  Cannot find Overall Job\n",
               job->job_name,
               job->job_no);
            fhdomain_check(sg.cur_db,1,rec_dump);
            (void) free(job_acc_ptr);
            return;      /* Should never happen; how about an error! */
        }
        oj_ptr->res_used.no_of_samples += job_acc_ptr->used.no_of_samples;
        oj_ptr->res_used.low_time = job_acc_ptr->used.low_time;
        oj_ptr->res_used.high_time = job_acc_ptr->used.high_time;
        oj_ptr->res_used.sigma_elapsed_time +=(job_acc_ptr->used.high_time -
                                            job_acc_ptr->used.low_time);
        oj_ptr->res_used.sigma_elapsed_time_2+=(job_acc_ptr->used.high_time-
                                            job_acc_ptr->used.low_time) *
                                           (job_acc_ptr->used.high_time -
                                            job_acc_ptr->used.low_time);
        oj_ptr->res_used.sigma_cpu += job_acc_ptr->used.sigma_cpu;
        oj_ptr->res_used.sigma_cpu_2 += job_acc_ptr->used.sigma_cpu_2;
        oj_ptr->res_used.sigma_io += job_acc_ptr->used.sigma_io;
        oj_ptr->res_used.sigma_io_2 += job_acc_ptr->used.sigma_io_2;
        oj_ptr->res_used.sigma_memory += job_acc_ptr->used.sigma_memory;
        oj_ptr->res_used.sigma_memory_2 += job_acc_ptr->used.sigma_memory_2;
        if (fseek(sg.cur_db->fp,soffset,0) < 0)
        {
            fprintf(sg.errout,
             "updateejob_db() - failed to position to the overall job entry\n");
            (void) free(job_acc_ptr);
            return;
        }
        if (!fh_put_from_mem(sg.cur_db,OVERALL_JOB_TYPE,(char *) oj_ptr,
                               sizeof(struct _overall_job)))
        {
            fprintf(sg.errout,
             "updateejob_db() - failed to update the overall job entry\n");
            (void) free(job_acc_ptr);
            return;
        }
        (void) free(oj_ptr);
/*
 * Write out a log record for the job
 */
        job_acc = *job_acc_ptr;
        job_acc.execution_status = job->execution_status;
        job_acc.status = job->status;
        STRCPY(job_acc.queue_name,job->job_queue->queue_name);
        STRCPY(job_acc.user_name,job->job_owner);
        STRCPY(job_acc.job_name,job->job_name);
        create_unix_acct(&job_acc);
    }
/*
 * Update the record read
 */
    job_acc = *job_acc_ptr;
    job_acc.status = job->status;
    job_acc.execution_status = job->execution_status;
    job_acc.used.low_time = 0;     /* Make sure this stays current! */
    STRCPY(job_acc.queue_name,job->job_queue->queue_name);
    STRCPY(job_acc.user_name,job->job_owner);
    STRCPY(job_acc.job_name,job->job_name);
    if (fseek(sg.cur_db->fp,hoffset,0) < 0)
    {
        fprintf(sg.errout,
         "updateejob_db() - failed to re-position to the job record\n");
        (void) free(job_acc_ptr);
        return;
    }
    if (!fh_put_from_mem(sg.cur_db,JOB_ACC_TYPE,
               (char *) &job_acc,sizeof(job_acc)))
    {
        fprintf(sg.errout,
         "updateejob_db() - failed to update the job account record\n");
        (void) free(job_acc_ptr);
        return;
    }
    if (one_shot == (DH_CON *) NULL)
    {
        (void) fhclose(sg.cur_db);
        sg.cur_db = (DH_CON *) NULL;
    }
    (void) free(job_acc_ptr);
    return;
}
/*
 * Insert a fresh Job Accounting Record
 */
void createjobacc_db(ja_ptr)
JOB_ACC * ja_ptr;
{
long hoffset;
union hash_entry hash_entry;

    ja_ptr->used.no_of_samples = 0;
    ja_ptr->used.low_time = 0;
    ja_ptr->used.high_time = 0;
    ja_ptr->used.sigma_elapsed_time = 0.0;
    ja_ptr->used.sigma_elapsed_time_2 = 0.0;
    ja_ptr->used.sigma_cpu = 0.0;
    ja_ptr->used.sigma_cpu_2 = 0.0;
    ja_ptr->used.sigma_io = 0.0;
    ja_ptr->used.sigma_io_2 = 0.0;
    ja_ptr->used.sigma_memory = 0.0;
    ja_ptr->used.sigma_memory_2 = 0.0;
    hoffset = fhinsert(sg.cur_db,
              (char *) &ja_ptr->job_no,sizeof(ja_ptr->job_no),
                         JOB_ACC_TYPE,
                        (char *) ja_ptr, sizeof(*ja_ptr));
    if (hoffset < sg.cur_db->hash_preamble.hash_header.preamble_size)
    {
        fprintf(sg.errout,"createjobacc_db() - Failed to create record\n");
        return;
    }
    if ( fseek(sg.cur_db->fp, hoffset, 0) < 0)
    {
        fprintf(sg.errout,"createjobacc_db() - Failed to re-read hash entry\n");
        return;
    }
    (void) fread((char *) &hash_entry,
                     sizeof(char),sizeof(hash_entry),sg.cur_db->fp);
    if (!fhinsert(sg.cur_db,ja_ptr->job_name,
                        strlen(ja_ptr->job_name),JOB_ACC_TYPE,
                        hash_entry.hi.body,0))
    {
        fprintf(sg.errout,
               "createjobacc_db() - Failed to index job name entry\n");
        return;
    }
    if (!fhinsert(sg.cur_db,ja_ptr->queue_name,
                        strlen(ja_ptr->queue_name),JOB_ACC_TYPE,
                       hash_entry.hi.body,0))
    {
        fprintf(sg.errout,"createjobacc_db() - Failed to index queue entry\n");
        return;
    }
    if (!fhinsert(sg.cur_db,ja_ptr->user_name,
                        strlen(ja_ptr->user_name),JOB_ACC_TYPE,
                       hash_entry.hi.body,0))
    {
        fprintf(sg.errout,"createjobacc_db() - Failed to index user entry\n");
        return;
    }
    return;
}
/*
 * renamejob_db()
 * - Open the database if not already open
 * - Find the corresponding job accounting record
 * - call job_status_chg()
 * - Update the index entries as necessary for the existing job
 *   (remove/insert)
 * - Close the database if it had to be opened especially.
 * BEWARE: natcalc() can itself call this routine. Therefore, the
 * the code must be prepared to handle recursive calls!
 */
void renamejob_db(job)
JOB * job;
{
union hash_entry hash_entry;
int rl;
int rt;
long hoffset, noffset;
JOB_ACC job_acc, search_job_acc, * job_acc_ptr;
DH_CON * one_shot;

    one_shot = sg.cur_db;
    if (one_shot == (DH_CON *) NULL)
        (void) dh_open();
    memset((char *)&job_acc,'\0',sizeof(job_acc));
    job_acc.job_no = job->job_no;
    (void) memset((char *) &search_job_acc,0,sizeof(search_job_acc));
    search_job_acc.job_no = job->job_no;
/*
 * We find the current one by looking for a zero occurrence value
 */
    if (!(hoffset = findjobacc_db(&search_job_acc, 0,&job_acc_ptr)))
    {
/*
 * Happens when creating a job with a submission trigger
 *
        (void) fprintf(stderr,
             "renamejob_db() Job No: %d  Cannot find Job Accounting Template\n",
                job->job_no);
        fhdomain_check(sg.cur_db,1,rec_dump);
 */
        return;
    }
    (void) free(job_acc_ptr);
    if (job->status != JOB_STARTED)
    (void) job_status_chg(job);
/*
 * Re-read and update the record read
 */
    if (!fh_get_to_mem(sg.cur_db,hoffset,&rt,(char **) &job_acc_ptr, &rl))
    {
        fprintf(sg.errout,
                "renamejob_db() - failed to read the job record\n");
        return;
    }
    job_acc = *job_acc_ptr;
    job_acc.status = job->status;
    job_acc.execution_status = job->execution_status;
    job_acc.planned_time = job->job_execute_time;
    STRCPY(job_acc.queue_name,job->job_queue->queue_name);
    STRCPY(job_acc.user_name,job->job_owner);
    STRCPY(job_acc.job_name,job->job_name);
    if (fseek(sg.cur_db->fp,hoffset,0) < 0)
    {
        fprintf(sg.errout,
                "renamejob_db() - failed to re-position to the job record\n");
        (void) free(job_acc_ptr);
        return;
    }
    if (!fh_put_from_mem(sg.cur_db,JOB_ACC_TYPE,
                         (char *) &job_acc,sizeof(job_acc)))
    {
        fprintf(sg.errout,
                "renamejob_db() - failed to re-write to the job record\n");
        (void) free(job_acc_ptr);
        return;
    }
/*
 * Adjust the index entries if appropriate
 */
    if (strcmp(job_acc.job_name,job_acc_ptr->job_name))
    {
        OVERALL_JOB overall_job, *oj_ptr;
        for ( hash_entry.hi.in_use = 0,
              noffset = fhlookup(sg.cur_db,job_acc_ptr->job_name,
                                 strlen(job_acc_ptr->job_name), &hash_entry);
                  noffset > 0;
              noffset = fhlookup(sg.cur_db,job_acc_ptr->job_name,
                                 strlen(job_acc_ptr->job_name), &hash_entry))
        {
            if (hash_entry.hi.body == hoffset)
            {
                hash_entry.hi.next = noffset;
                fhremove(sg.cur_db,hash_entry.hi.name,0,&hash_entry);
                break;
            }
        }
        if (noffset == 0)
        {
            fprintf(sg.errout,
                "renamejob_db() - failed to find the job name index entry\n");
            (void) free(job_acc_ptr);
            return;
        }
        if (!fhinsert(sg.cur_db,job_acc.job_name,
                        strlen(job_acc.job_name),JOB_ACC_TYPE,
                                                 hash_entry.hi.body,0))
        {
            fprintf(sg.errout,
              "renamejob_db() - failed to re-write the job name index entry\n");
            (void) free(job_acc_ptr);
            return;
        }
        if (!findoveralljob_db(job_acc.job_name,&oj_ptr))
        {               /* Need to add an Overall Job record */
            STRCPY(overall_job.job_name,job_acc.job_name);
            overall_job.res_used = job_acc.used;
            if (!fhinsert(sg.cur_db,overall_job.job_name,
                    strlen(overall_job.job_name),OVERALL_JOB_TYPE,
                    (char *) &overall_job, sizeof(overall_job)))
            {
                fprintf(sg.errout,
                  "renamejob_db() - failed to create new overall job record\n");
                (void) free(job_acc_ptr);
                return;
            }
        }
        else
            (void) free(oj_ptr);
    }
    if (strcmp(job_acc.queue_name,job_acc_ptr->queue_name))
    {
        for ( hash_entry.hi.in_use = 0,
              noffset = fhlookup(sg.cur_db,job_acc_ptr->queue_name,
                                 strlen(job_acc_ptr->queue_name), &hash_entry);
                  noffset > 0;
              noffset = fhlookup(sg.cur_db,job_acc_ptr->queue_name,
                                 strlen(job_acc_ptr->queue_name), &hash_entry))
        {
            if (hash_entry.hi.body == hoffset)
            {
                hash_entry.hi.next = noffset;
                fhremove(sg.cur_db,hash_entry.hi.name,0,&hash_entry);
                break;
            }
        }
        if (noffset == 0)
        {
            fprintf(sg.errout,
                "renamejob_db() - failed to find the job name index entry\n");
            (void) free(job_acc_ptr);
            return;
        }
        if (!fhinsert(sg.cur_db,job_acc.queue_name,
                        strlen(job_acc.queue_name),JOB_ACC_TYPE,
                                                 hash_entry.hi.body,0))
        {
            fprintf(sg.errout,
            "renamejob_db() - failed to re-write the queue name index entry\n");
            (void) free(job_acc_ptr);
            return;
        }
    }
    if (strcmp(job_acc.user_name,job_acc_ptr->user_name))
    {
        for ( hash_entry.hi.in_use = 0,
              noffset = fhlookup(sg.cur_db,job_acc_ptr->user_name,
                                 strlen(job_acc_ptr->user_name), &hash_entry);
                  noffset > 0;
              noffset = fhlookup(sg.cur_db,job_acc_ptr->user_name,
                                 strlen(job_acc_ptr->user_name), &hash_entry))
        {
            if (hash_entry.hi.body == hoffset)
            {
                hash_entry.hi.next = noffset;
                fhremove(sg.cur_db,hash_entry.hi.name,0,&hash_entry);
                break;
            }
        }
        if (noffset == 0)
        {
            fprintf(sg.errout,
                "renamejob_db() - failed to find the user name index entry\n");
            (void) free(job_acc_ptr);
            return;
        }
        if (!fhinsert(sg.cur_db,job_acc.user_name,
                        strlen(job_acc.user_name),JOB_ACC_TYPE,
                                                 hash_entry.hi.body,0))
        {
            fprintf(sg.errout,
             "renamejob_db() - failed to re-write the user name index entry\n");
            (void) free(job_acc_ptr);
            return;
        }
    }
    if (one_shot == (DH_CON *) NULL)
    {
        (void) fhclose(sg.cur_db);
        sg.cur_db = (DH_CON *) NULL;
    }
    (void) free(job_acc_ptr);
    return;
}
/*
 * Routine to zap a job accounting record
 */
void job_acc_hremove(hoffset,job_acc_ptr)
long hoffset;
JOB_ACC * job_acc_ptr;
{
union hash_entry hash_entry;
long noffset;

    for ( hash_entry.hi.in_use = 0,
          noffset = fhlookup(sg.cur_db,job_acc_ptr->job_name,
                             strlen(job_acc_ptr->job_name), &hash_entry);
              noffset > 0;
          noffset = fhlookup(sg.cur_db,job_acc_ptr->job_name,
                             strlen(job_acc_ptr->job_name), &hash_entry))
    {
        if (hash_entry.hi.body == hoffset)
        {
            hash_entry.hi.next = noffset;
            fhremove(sg.cur_db,hash_entry.hi.name,0,&hash_entry);
            break;
        }
    }
    if (noffset == 0)
    {
        fprintf(sg.errout,
               "job_acc_hremove() - failed to find the job name index entry\n");
        return;
    }
    for ( hash_entry.hi.in_use = 0,
          noffset = fhlookup(sg.cur_db,job_acc_ptr->queue_name,
                             strlen(job_acc_ptr->queue_name), &hash_entry);
              noffset > 0;
          noffset = fhlookup(sg.cur_db,job_acc_ptr->queue_name,
                             strlen(job_acc_ptr->queue_name), &hash_entry))
    {
        if (hash_entry.hi.body == hoffset)
        {
            hash_entry.hi.next = noffset;
            fhremove(sg.cur_db,hash_entry.hi.name,0,&hash_entry);
            break;
        }
    }
    if (noffset == 0)
    {
        fprintf(sg.errout,
                "deletejob_db() - failed to find the job name index entry\n");
        return;
    }
    for ( hash_entry.hi.in_use = 0,
          noffset = fhlookup(sg.cur_db,job_acc_ptr->user_name,
                             strlen(job_acc_ptr->user_name), &hash_entry);
              noffset > 0;
          noffset = fhlookup(sg.cur_db,job_acc_ptr->user_name,
                             strlen(job_acc_ptr->user_name), &hash_entry))
    {
        if (hash_entry.hi.body == hoffset)
        {
            hash_entry.hi.next = noffset;
            fhremove(sg.cur_db,hash_entry.hi.name,0,&hash_entry);
            break;
        }
    }
    if (noffset == 0)
    {
        fprintf(sg.errout,
                "deletejob_db() - failed to find the user name index entry\n");
        return;
    }
    fh_chg_ref(sg.cur_db,hoffset,-1);
    return;
}
/*
 * Zap the database record for a job
 */
void deletejob_db(job_no)
int job_no;
{
long hoffset;
JOB_ACC  * job_acc_ptr;
JOB_ACC search_job;
DH_CON * one_shot;

    one_shot = sg.cur_db;
    if (one_shot == (DH_CON *) NULL)
        (void) dh_open();
/*
 * We find the current one by looking for a zero occurrence value
 */
    memset((char *)&search_job,'\0',sizeof(search_job));
    search_job.job_no = job_no;
    if (!(hoffset = findjobacc_db(&search_job, 0,&job_acc_ptr)))
        return;              /* Possible with a race condition? */
    job_acc_hremove(hoffset,job_acc_ptr);
    (void) free(job_acc_ptr);
    if (one_shot == (DH_CON *) NULL)
    {
        (void) fhclose(sg.cur_db);
        sg.cur_db = (DH_CON *) NULL;
    }
    return;
}
/*
 * findjobbynumber()
 * - Search for the job in the database
 * - Search for the queue
 * - Scan the files in the directory for the right one
 * - return it
 */
JOB * findjobbynumber(queue_anchor,job_no)
QUEUE ** queue_anchor;
int job_no;
{
QUEUE * last_queue = (QUEUE *) NULL;
JOB  *cur_job, search_job;
JOB_ACC * job_acc_ptr, search_job_acc;

    if (job_no == 0)
        return (JOB *) NULL;
    search_job.job_no = job_no;
    search_job_acc.job_no = job_no;
    search_job_acc.job_name[0] = '\0';
    search_job_acc.user_name[0] = '\0';
    search_job_acc.queue_name[0] = '\0';
    if (!findjobacc_db(&search_job_acc,0,&job_acc_ptr))
    {
#ifdef DEBUG
        (void) fprintf(stderr,"No such job: %d\n",job_no);
#endif
        return (JOB *) NULL;
    } 
    if (*queue_anchor == (QUEUE *) NULL)
        (void) queue_file_open(0,(FILE **) NULL,
                                  QUEUELIST,queue_anchor,&last_queue); 
    search_job.job_queue = find_ft(*queue_anchor,job_acc_ptr->queue_name,                                           (char *) NULL, 1);
    checkperm(U_READ,search_job.job_queue);
    if (!queue_open(search_job.job_queue))
    {   
#ifdef DEBUG
        (void) fprintf(stderr,"findjobbynumber(): could not open queue: %s\n",
                              job_acc_ptr->queue_name);
#endif
    
        (void) free(job_acc_ptr);
        return (JOB *) NULL;
    }
/* 
 * Loop through the files in the directory
 */ 
    while ((cur_job = queue_fetch(search_job.job_queue)) != (JOB *) NULL)
        if (job_no == cur_job->job_no) 
        {
            cur_job = job_clone(cur_job);
            cur_job->status = job_acc_ptr->status;
            cur_job->execution_status = job_acc_ptr->execution_status;
            break;
        }
    queue_close(search_job.job_queue);
    (void) free(job_acc_ptr);
    return cur_job;
}
/*
 * job_status_chg()
 * - Function that carries out the necessary actions on a status
 *   change.
 * - natcalc() is called with the appropriate text, depending
 *   on the status.
 * - returns natcalc()'s return code.
 * - detects circularities in the rules.
 * - does not allow itself to be called recursively (because the yacc
 *   parser cannot be called recursively)
 * 
 */
int job_status_chg(job)
JOB * job;
{
static int rec_count = 0;
ENV * env_to_do = (ENV *) NULL;
int loop_detect = 0;
int ret =0;

    if (rec_count)
        return 1;
    else
        rec_count = 1;
    for(;;)
    {
        switch (job->status)
        {
        case JOB_SUBMITTED:
            if (loop_detect & (1 << JOB_SUBMITTED))
            {
                rec_count = 0;
                return ret;
            }
            env_to_do = job->j_submitted; 
            loop_detect |= (1 << JOB_SUBMITTED);
            break;
        case JOB_RELEASED:
            if (loop_detect & (1 << JOB_RELEASED))
            {
                rec_count = 0;
                return ret;
            }
            env_to_do = job->j_released; 
            loop_detect |= (1 << JOB_RELEASED);
            break;
        case JOB_HELD:
            if (loop_detect & (1 << JOB_HELD))
            {
                rec_count = 0;
                return ret;
            }
            env_to_do = job->j_held; 
            loop_detect |= (1 << JOB_HELD);
            break;
        case JOB_SUCCEEDED:
            if (loop_detect & (1 << JOB_SUCCEEDED))
            {
                rec_count = 0;
                return ret;
            }
            env_to_do = job->j_succeeded; 
            loop_detect |= (1 << JOB_SUCCEEDED);
            break;
        case JOB_FAILED:
            if (loop_detect & (1 << JOB_FAILED))
            {
                rec_count = 0;
                return ret;
            }
            env_to_do = job->j_failed; 
            loop_detect |= (1 << JOB_FAILED);
            break;
        case JOB_STARTED:
        default:
            if (loop_detect & (1 << JOB_STARTED))
            {
                rec_count = 0;
                return ret;
            }
            env_to_do = job->j_started; 
            loop_detect |= (1 << JOB_STARTED);
            break;
        }
        if (env_to_do != (ENV *) NULL)
        {
#ifdef DEBUG
            fprintf(sg.errout,"Job: %d Status: %d Trigger:\n%s\n",
                    job->job_no,job->status,env_to_do->e_env);
            fflush(sg.errout);
#endif
            ret = natcalc((FILE *) NULL,env_to_do->e_env, 1, job);
        } 
        else
            ret = 1;           /* Default is success */
        if (strcmp(job->job_resubmit,"no") &&
           (job->status == JOB_SUCCEEDED || job->status == JOB_FAILED))
        {
            if (job->status == JOB_SUCCEEDED)
                job->status = JOB_RELEASED;
            else
                job->status = JOB_HELD;
        }
    }
}
/*
 * Routine to open the log file for a job
 * Mode is read/write to allow seek back to the beginning for mail.
 */
FILE * log_file_open(job,fname)
JOB * job;
char ** fname;
{
static char buf[BUFSIZ];
FILE * fp;
int oumask,numask=0x7; /* Make sure the mode flags on creation
                                       are the ones that we want */

    *fname = filename("%s/%s/j%d.log",sg.supernat,SUPERNAT_LOG,job->job_no);
    oumask = umask(numask);
    if ((fp = fopen(*fname,"r+")) != (FILE *) NULL ||
        (fp = fopen(*fname,"w+")) != (FILE *) NULL)
    {
        setbuf(fp, buf);
        (void) fseek(fp,0L,2);
    }
    (void) umask(oumask);
    return fp;
}
