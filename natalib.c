/*
 *        natalib.c
 *
 *        Copyright (c) E2 Systems, UK, 1990. All rights reserved.
 *
 *        Common User-oriented routines for nat system.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 1992\n";
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifdef AIX
#include <fcntl.h>
#else
#ifndef ULTRIX
#include <sys/fcntl.h>
#endif
#endif
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "supernat.h"
#ifdef SOLAR
#include <sys/stropts.h>
#include <sys/stream.h>
#pragma ident	"@(#)natalib.c	%D% %T% %E% %U% Copyright (c) E2 Systems"
#endif
#ifdef LINUX
    int (*sigset())();
#endif
struct passwd * getpwnam(), * getpwuid();
struct group * getgrnam(), * getgrgid();
/*
 *        get the next user record from the user or grant list
 */
USER *
getuserent(fp)
FILE *fp;
{
char buf[BUFSIZ];

    if (fgets(buf, sizeof(buf), fp) == (char *) NULL)
        return((USER *) NULL);
    buf[strlen(buf) - 1] = 0;
    return(s2user(buf));
}
/*
 *        return the grant name for the queue queue
 */
char *
grantname(queue)
QUEUE *queue;
{
    return(filename("G.%s", fileof(queue->queue_dir)));
}

/*
 *        get the next queue name
 */
char *
getnextqueue()
{
    return(getnewfile(sg.supernat,"Loc"));
}
/*
 *        get the user record for user uname from file
 *      if there is no user name record, and but there is a global,
 *      return that instead.
 */
USER *
getuserbyname(file, uname)
char        *file;
char        *uname;
{
USER *up;
FILE *fp;
USER *gp;      /* Wild Card Value */


    if ((fp = openfile(sg.supernat, file, "r")) == (FILE *) NULL)
        return((USER *) NULL);
    gp = (USER *) NULL;
    while ((up = getuserent(fp)) != (USER *) NULL)
    {
        if (strcmp(up->u_name, uname) == 0)
            break;
        if (strcmp(up->u_name, "*") == 0)
            gp = up;
        else
            free(up);
    }
    (void) fclose(fp);
    if ( up != (USER *) NULL)
    {
        if (gp != (USER *) NULL)
            free(gp);          /* Need to throw away the global one */
    }
    else up = gp;
    return(up);
}

/*
 *        add the user record up to the queue queue. It's an error
 *        if up is already in the userlist.
 */
int
adduser(queue, up)
QUEUE        *queue;
USER        *up;
{
char    *grant;
FILE    *fp;
USER    *tp;

char buf[BUFSIZ];

    if (queue == (QUEUE *) NULL)
    {
        if ((tp = getuserbyname(USERLIST, up->u_name)) != (USER *) NULL &&
                 !strcmp(tp->u_name,up->u_name))
        {                              /* getuserbyname() may return the
                                              wild card value */
            ERROR(__FILE__, __LINE__, 0, "User %s already exists",
                                             up->u_name, NULL);
            free(tp);
            return(0);
        }
        checkperm(U_READ|U_WRITE|U_EXEC,(QUEUE *) NULL);
        chmod(buf,0770);
        fp = openfile(sg.supernat, USERLIST, "a");
    }
    else
    {
       grant = grantname(queue);
       if ((tp = getuserbyname(grant, up->u_name)) != (USER *) NULL &&
                 !strcmp(tp->u_name,up->u_name))
       {                              /* getuserbyname() may return the
                                              wild card value */
           ERROR(__FILE__, __LINE__, 0, "User %s already exists",
                                              up->u_name, NULL);
           (void) free(tp);
           (void) free(grant);
           return(0);
       }
       checkperm(U_READ|U_WRITE|U_EXEC,queue);
       fp = openfile(sg.supernat, grant, "a");
       (void) free(grant);
    }
    if (tp != (USER *) NULL)
        free(tp);
    if (fp == (FILE *) NULL)
        return(0);
    (void) fprintf(fp, "%s", user2s(up));
    (void) fclose(fp);
    return(1);
}

/*
 *        change the user record 'from' to the user record 'to' in the
 *        queue queue. If 'to' is NULL, or all permissions revoked,
 *      then delete the user record.
 */
int
chguser(queue, from, to)
QUEUE        *queue;
USER        *from;
USER        *to;
{
FILE    *fpw;
FILE    *fpr;
USER    *up;
char    *name;
char    *tmpname;
char    *grant;

    if (queue == (QUEUE *) NULL)
    {
        if ((up = getuserbyname(USERLIST, from->u_name))
                     == (USER *) NULL ||
                     strcmp(up->u_name,from->u_name))
        {
            ERROR(__FILE__, __LINE__, 0,
                              "User %s doesn't exist", from->u_name, NULL);
            if (up != (USER *) NULL)
                (void) free(up);
            return(0);
        }
        checkperm(U_READ|U_WRITE|U_EXEC,(QUEUE *) NULL);
        fpr = openfile(sg.supernat, USERLIST, "r+"); /* needed to Lock the file
                                                      */
    }
    else
    {
        grant = grantname(queue);
        if ((up = getuserbyname(grant, from->u_name))
                     == (USER *) NULL ||
                     strcmp(up->u_name,from->u_name))
                {
            ERROR(__FILE__, __LINE__, 0,
                              "User %s doesn't exist", from->u_name, NULL);
                        if (up != (USER *) NULL)
                            (void) free(up);
                        (void) free(grant);
            return(0);
        }
        checkperm(U_READ|U_WRITE|U_EXEC,queue);
        fpr = openfile(sg.supernat, grant, "r+"); /* needed to lock the file */
    }
    if (fpr == (FILE *) NULL)
    {
        if (queue != (QUEUE *) NULL)
            (void) free(grant);
        return(0);
    }
    tmpname = getnewfile(sg.supernat,"tmp");
    if ((fpw = openfile("/", tmpname, "w")) == (FILE *) NULL)
    {
        if (queue != (QUEUE *) NULL)
            (void) free(grant);
        (void) free(tmpname);
        return(0);
    }
    while ((up = getuserent(fpr)) != (USER *) NULL)
    {
        USER * tp;
        tp = up;
        if (strcmp(from->u_name, up->u_name) == 0)
        {
            if (to == (USER *) NULL || to->u_perm == 0)
            {
                (void) free(up);
                continue;
            }
            else
                up = to;
        }
        (void) fprintf(fpw, "%s", user2s(up));
        (void) free(tp);
    }
    name = filename("%s/%s", sg.supernat,
                        (queue == (QUEUE *) NULL) ? USERLIST : grant);
    if (queue != (QUEUE *) NULL)
        (void) free(grant);
    (void) fclose(fpw);
/*
 * Leave the fpr open in order to hold the lock until finished. Does this mean
 * that the lrename() will fail across file systems?
 */
    if (lrename(tmpname, name) < 0)
    {
        ERROR(__FILE__, __LINE__, 0,
                      "can't rename %s to %s", tmpname, name);
        (void) free(name);
        (void) free(tmpname);
        (void) fclose(fpr);
        return(0);
    }
    (void) free(tmpname);
    (void) free(name);
    (void) fclose(fpr);
    return(1);
}

/*
 *        get the next Queue record
 */
QUEUE *
getqueueent(q)
QUEUE        *q;
{
    return q->q_next;
}
/*
 *        Return the directory pertaining to the queue.
 *      Beware:
 *      -  This routine returns a hanging queue, not
 *         chained or pointed to by anything
 *      -  Space must have been allocated in the calling routine.
 */
char *
queuetodir(queue,anchor)
QUEUE        *queue;
QUEUE ** anchor;
{
QUEUE        *lp = (QUEUE *) NULL;

    if (*anchor == (QUEUE *) NULL)
        (void) queue_file_open(0,(FILE **) NULL,QUEUELIST,anchor,&lp);
    for ( lp = *anchor;
              lp != (QUEUE *) NULL;
                  lp = getqueueent(lp))
    {
        if (strcmp(lp->queue_name, queue->queue_name) == 0)
        {
            *queue = *lp;
                queue->q_next = (QUEUE *) NULL;
            break;
        }
    }
    return((lp == (QUEUE *) NULL) ? (char *) NULL : queue->queue_dir);
}

/*
 * Return the queue pertaining to the directory.
 *
 */
QUEUE * dirtoqueue(dir,anchor)
char    *dir;
QUEUE ** anchor;
{
QUEUE    *lp;

    if (*anchor == (QUEUE *) NULL)
        (void) queue_file_open(0,(FILE **) NULL,QUEUELIST,anchor,&lp);
    for (lp = *anchor;
             lp != (QUEUE *) NULL;
                 lp = getqueueent(lp))
    {
        if (strcmp(lp->queue_dir, dir) == 0)
            break;
    }
    return(lp);
}

/*
 * Set up the files that go with a queue
 */
void queue_os_setup(q)
QUEUE * q;
{
    FILE * fpw;
    char * grant;
    if ( mkdir(q->queue_dir, 0770))
            ERROR(__FILE__, __LINE__, 0, "%s directory creation",
                   q->queue_name, NULL);
    (void) chown(q->queue_dir,getadminuid(),getadmingid());
    grant = grantname(q);
    if ((fpw = openfile(sg.supernat, grant, "w")) == (FILE *) NULL)
            ERROR(__FILE__, __LINE__, 0, "%s grant creation",
                   q->queue_name, NULL);
    (void) fprintf(fpw, "rwx%s\n", sg.username);
    if (strcmp(sg.adminname,sg.username))
        (void) fprintf(fpw, "rwx%s\n", sg.adminname);
    (void) fclose(fpw);
    (void) free(grant);
    if ((fpw = openfile(q->queue_dir, LASTTIMEDONE,"w")) == (FILE *) NULL)
    ERROR(__FILE__, __LINE__, 0, "%s lasttimedone creation",
                                 q->queue_dir, NULL);
    (void) fwrite("\n",1,1,fpw);
    (void) fclose(fpw);
    return;
}
/*
 * Zap the files that go with a queue
 */
void queue_os_destroy(q)
QUEUE * q;
{
char * grant;
char    buf[BUFSIZ];
char * x;

    grant = grantname(q);
    x = filename("%s/%s",sg.supernat,grant);
    (void) e2unlink(x);
    (void) free(grant);
    (void) free(x);
    (void) chdir("..");
    (void) sprintf(buf,"rm -rf %s",q->queue_dir);
    (void) system(buf);
    return;
}
/*
 *  Delete the queue identified.
 *  - One shot at a time.
 *  - Need rwx permissions
 *  - race condition if queue_anchor pre-set
 *  - Cannot delete if jobs still exist
 */
int delqueue(queue,q_anchor)
QUEUE        *queue;
QUEUE ** q_anchor;
{
FILE    *fpr;
FILE    *fpw;
char    buf[BUFSIZ];
char    *tmp;
QUEUE    *lp;
QUEUE    *prev;
QUEUE * queue_anchor;
QUEUE * queue_last;

    if (queue->queue_name == (char *) NULL)
        return(0);
    checkperm(U_READ|U_WRITE,(QUEUE *) NULL);
    tmp = getnewfile(sg.supernat,"tmp");
    if ((fpw = openfile("/", tmp, "w")) == (FILE *) NULL)
    {
        (void) free(tmp);
        return(0);
    }
    if (q_anchor == (QUEUE **) NULL || *q_anchor == (QUEUE *) NULL) 
    {
        queue_anchor = (QUEUE *) NULL;
        queue_last = (QUEUE *) NULL;
        if (queue_file_open(1,&fpr, QUEUELIST,&queue_anchor,&queue_last)
                 == (QUEUE *) NULL)
        {
            (void) e2unlink(tmp);
            (void) free(tmp);
            (void) fclose(fpw);
            return(0);
        }
    }
    else
    {
        queue_anchor = *q_anchor;
        fpr = openfile(sg.supernat, QUEUELIST, "r+"); /* To lock it */
    }
    for (lp = queue_anchor,
         prev = (QUEUE *) NULL;
             lp != (QUEUE *) NULL;
                 lp = getqueueent(lp))
    {
        if (!strcmp(queue->queue_name, lp->queue_name))
        {
            if (lp->time_cnt != 0 &&
             (!queue_open(lp) || lp->job_cnt == 0))
            {        /* Cannot delete queue with jobs in it */
                queue_os_destroy(lp);
            }
/*
 * Do not recover the memory, to cater for things that are still running
 */
            if (lp->job_cnt == 0)
            {
                if (prev == (QUEUE *) NULL)
                    queue_anchor = lp->q_next;
                else
                    prev->q_next = lp->q_next;
                break;
            }
        }
        prev = lp;
    }
    if (lp == (QUEUE *) NULL)
    {            /* No valid match */
        (void) e2unlink(tmp);
        (void) free(tmp);
        if (fpr != (FILE *) NULL)
            (void) fclose(fpr);
        (void) fclose(fpw);
        return(0);
    }
    else
    {
        char * x;
        if (q_anchor != (QUEUE **) NULL)
            *q_anchor = queue_anchor;
        setbuf(fpw,buf);
        queue_file_write(fpw,queue_anchor);
        (void) fclose(fpw);
        (void) sprintf(buf, "%s/%s", sg.supernat, QUEUELIST);
        if (lrename(tmp, buf) < 0)
        {
            ERROR(__FILE__, __LINE__, 0, "can't rename %s to %s", tmp, buf);
            if (fpr != (FILE *) NULL)
                (void) fclose(fpr);
            (void) fclose(fpw);
            return(0);
        }
        if (fpr != (FILE *) NULL)
            (void) fclose(fpr);
        return(1);
    }
}
/*
 * Validate the natcalc() stuff
 */
int queue_val(q)
QUEUE * q;
{
DH_CON * one_shot;

    one_shot = sg.cur_db;
    if (one_shot == (DH_CON *) NULL)
        (void) dh_open();

    if ((q->q_submitted != (ENV *) NULL &&
       natcalc((FILE *) NULL,q->q_submitted->e_env, -1, (JOB *) NULL) < 0)
    || (q->q_released != (ENV *) NULL &&
       natcalc((FILE *) NULL,q->q_released->e_env, -1, (JOB *) NULL) < 0)
    || (q->q_held != (ENV *) NULL &&
       natcalc((FILE *) NULL,q->q_held->e_env, -1, (JOB *) NULL) < 0)
    || (q->q_started != (ENV *) NULL &&
       natcalc((FILE *) NULL,q->q_started->e_env, -1, (JOB *) NULL) < 0)
    || (q->q_succeeded != (ENV *) NULL &&
       natcalc((FILE *) NULL,q->q_succeeded->e_env, -1, (JOB *) NULL) < 0)
    || (q->q_failed != (ENV *) NULL &&
       natcalc((FILE *) NULL,q->q_failed->e_env, -1, (JOB *) NULL) < 0))
    {
        if (one_shot == (DH_CON *) NULL)
        {
            (void) fhclose(sg.cur_db);
            sg.cur_db = (DH_CON *) NULL;
        }
        return 0;
    }
    if (one_shot == (DH_CON *) NULL)
    {
        (void) fhclose(sg.cur_db);
        sg.cur_db = (DH_CON *) NULL;
    }
    return 1;
}
/*
 * Add/change common setup
 * Do not pass it null pointers.
 */
int queuecomm(fpp,q_anchor,q_last,n_last,queue_file)
FILE ** fpp;
QUEUE ** q_anchor;
QUEUE ** q_last;
QUEUE ** n_last;
char * queue_file;
{
QUEUE * exist_anchor;
QUEUE * exist_last;
QUEUE * new_last;

    if (*q_anchor == (QUEUE *) NULL)
    {
        exist_anchor = (QUEUE *) NULL;
        exist_last = (QUEUE *) NULL;
    }
    else
    {
        exist_anchor = *q_anchor;
        for (exist_last = exist_anchor;
                exist_last-> q_next != (QUEUE *) NULL;
                exist_last=exist_last->q_next);
    }
    new_last = (QUEUE *) NULL;
    if ((exist_anchor == (QUEUE *) NULL &&
       (!queue_file_open(1,fpp,QUEUELIST,&exist_anchor,&exist_last)))||
       (exist_anchor != (QUEUE *) NULL &&
        ((*fpp = openfile(sg.supernat, QUEUELIST, "r+")) == (FILE *) NULL))||
                                                     /* needed to lock the file
                                                      */
        !queue_file_open(0,(FILE **) NULL,queue_file,&exist_anchor,&new_last)||
        new_last == exist_last)          /* No input */
    {
        (void) e2unlink(queue_file);
        if (*q_anchor == (QUEUE *) NULL)
            destroy_queue_list(exist_anchor);
        return (0);
    }
    (void) e2unlink(queue_file);
    *q_anchor = exist_anchor;
    *q_last = exist_last;
    *n_last = new_last;
    return 1;
}
/*
 *        Add queue to the list. Queue details are in a submitted file.
 *        Create a grantfile, and a Job Directory for the queue.
 */
int addqueue(queue_file, q_anchor)
char        *queue_file;
QUEUE ** q_anchor;
{
FILE        *fp;
QUEUE     *exist_anchor;
QUEUE     *q;
QUEUE     *exist_last;
QUEUE     *new_last;

    if (q_anchor == (QUEUE **) NULL ||
        *q_anchor == (QUEUE *) NULL)
    {
        exist_anchor = (QUEUE *) NULL;
        exist_last = (QUEUE *) NULL;
    }
    else
        exist_anchor = *q_anchor;
    if (!queuecomm(&fp,&exist_anchor,&exist_last,&new_last,queue_file))
        return 0;
    (void) fseek(fp,0,2);
    if (q_anchor != (QUEUE **) NULL)
        *q_anchor = exist_anchor;
/*
 * Validate
 */
    for (q = exist_last->q_next;
             q != (QUEUE *) NULL;
                 q = q->q_next)
    {
        if (find_ft(exist_anchor,q->queue_name,(char *) NULL,1) != q ||
                    !queue_val(q))
        {        /* Check for duplicates */
                 /* Check the syntax of the natcalc code */
            if (q_anchor == (QUEUE **) NULL)
                destroy_queue_list(exist_anchor);
            else
            {
                destroy_queue_list(exist_last->q_next);
                exist_last->q_next = (QUEUE *) NULL;
            }
            (void) fclose(fp);
            return(0);
        }
/*
 * Only create directories if time periods are defined
 */
        if (q->time_cnt)
        {
            char * x = getnextqueue();
            strcpy(q->queue_dir,x);
            (void) free(x);
        }
    }
    (void) fseek(fp,0,2);       /* Go to the end */
    queue_file_write(fp,exist_last->q_next);
    for (q = exist_last->q_next;
             q != (QUEUE *) NULL;
                 q = q->q_next)
    {
        if (q->time_cnt)
            queue_os_setup(q);
    }
    (void) fclose(fp);
    if (q_anchor == (QUEUE **) NULL)
        destroy_queue_list(exist_anchor);
    return(1);
}
int chew_sym(low_high,chew_type)
int low_high;
int chew_type;
{
/*
 * Function to recognise a token in the input stream;
 * in the event of an error it returns the default value given.
 *
 */
char * x;
int i;
double secs_since;

    secs_since = 0.0;
    if ((x=strtok(sg.chew_glob,"\r\n\t ")) == NULL)
         return low_high;
    else
    switch(chew_type)
    {
    case CHEW_TIME:
        if (!strncmp(x,"2400",4) ||
            !strncmp(x,"24:00",5) ||
            !date_val(x,"HH24MI",&x,&secs_since) ||
             secs_since == 0.0 )
            return low_high;
        i = (int) secs_since;
        break;      
    case CHEW_INT:
    default:
        if (sscanf(x,"%d",&i) == 0)
            return low_high;
        break;      
    }
    return i;
}
/*
 * Search for previous item in the list
 */
QUEUE * find_prev(anchor,element)
QUEUE * anchor;
QUEUE * element;
{
QUEUE * q;

    if (element == anchor)
        return (QUEUE *) NULL;
    for (q = anchor;
             q != (QUEUE *) NULL &&
             q->q_next != element;
                 q = q->q_next);
    return q;
}
/*
 *        Change queue; any of the details
 *      Grantfile is unchanged
 */
int chgqueue(queue_file,q_anchor)
char        *queue_file;
QUEUE ** q_anchor;
{
char    buf[BUFSIZ];
char    *tmp;
FILE    *fp;
FILE    *fpw;
QUEUE     *old;
QUEUE     *exist_anchor;
QUEUE     *q;
QUEUE     *exist_last;
QUEUE     *new_last;
QUEUE     *prev_new;

    if (q_anchor == (QUEUE **) NULL ||
        *q_anchor == (QUEUE *) NULL)
    {
        exist_anchor = (QUEUE *) NULL;
        exist_last = (QUEUE *) NULL;
    }
    else
        exist_anchor = *q_anchor;
    if (!queuecomm(&fp,&exist_anchor,&exist_last,&new_last,queue_file)
          || exist_last == (QUEUE *) NULL)
        return 0;
/*
 * Check for matches
 */
    for (q = exist_last->q_next,
             exist_last->q_next = (QUEUE *) NULL;
             q != (QUEUE *) NULL;
                 prev_new = q,
                 q = q->q_next,
                 free(prev_new))
    {
        QUEUE * prev_old;
        if (((old = find_ft(exist_anchor,q->queue_name, (char *) NULL,1)) == 
              (QUEUE *) NULL)
            || !queue_val(q) ||
            ((getperm(old, sg.username) != (U_READ|U_WRITE|U_EXEC))
            && (getperm(old, sg.groupname) != (U_READ|U_WRITE|U_EXEC))))
        {        /* Doesn't exist! */
                 /* Check the syntax of the natcalc() code */
                 /* Insufficient privileges */
            if (q_anchor == (QUEUE **) NULL)
                destroy_queue_list(exist_anchor);
            destroy_queue_list(q);
            (void) fclose(fp);
            return(0);
        }
        if (old->time_cnt == 0 &&
            q->time_cnt > 0)
        {         /* Added time details to a set of rules */
            char * x = getnextqueue();
            strcpy(q->queue_dir,x);
            (void) free(x);
            queue_os_setup(q);
        }
        else if (old->time_cnt != 0 && q->time_cnt == 0 &&
             !queue_open(old) && old->job_cnt == 0)
                /* Cannot delete directory with jobs in it */
            queue_os_destroy(old);
        else
            strcpy(q->queue_dir,old->queue_dir);
/*
 * We must work the new queue into the old position, in order not to
 * invalidate other memory references
 */
        NEW(QUEUE,prev_old);
        *prev_old = *old;
        *old = *q;
        old->q_next = prev_old->q_next;
        old->job_cnt = prev_old->job_cnt;
        old->first_job = prev_old->first_job;
        old->cur_job_ind = prev_old->cur_job_ind;
        old->cur_simul = prev_old->cur_simul;
        old->job_order = prev_old->job_order;
        queue_destroy(prev_old);    /* Does not kill off any associated jobs */
    }
    tmp = getnewfile(sg.supernat,"tmp");
    if ((fpw = openfile("/", tmp, "w")) == (FILE *) NULL)
    {       /* Can't create! */
        destroy_queue_list(exist_anchor);
        if (q_anchor != (QUEUE **) NULL)
            *q_anchor = (QUEUE *) NULL;
        (void) fclose(fp);
        return(0);
    }
    setbuf(fpw,buf);
    queue_file_write(fpw,exist_anchor);
    queue_file = filename("%s/%s",sg.supernat,QUEUELIST);
    if (lrename(tmp, queue_file) < 0)
    {
        ERROR(__FILE__, __LINE__, 0, "can't rename %s to %s", tmp, 
                      queue_file);
        (void) free(queue_file);
        (void) e2unlink(tmp);
        (void) free(tmp);
        (void) fclose(fp);
        (void) fclose(fpw);
        return(0);
    }
    (void) free(queue_file);
    (void) free(tmp);
    (void) fclose(fp);
    (void) fclose(fpw);
    return(1);
}
/*
 *        get the user's permission from the Userlist/Grant list for
 *        the queue.
 */
int getperm(queue, user)
QUEUE        *queue;
char        *user;
{
char        *grant;
USER        *up;
int user_perm;

    if (queue == (QUEUE *) NULL || !strlen(queue->queue_dir))
        up = getuserbyname(USERLIST, user);
    else
    {
        grant = grantname(queue);
        up = getuserbyname(grant, user);
        (void) free (grant);
    }
    user_perm = (up == (USER *) NULL) ? 0 : up->u_perm;
    if (up != (USER *) NULL)
        (void) free (up);
    return user_perm;
}

/*
 *        return the user's name and login groups as character strings.
 *      If we can't find the name, then return the uid and effective uid.
 *      This won't get past the permission test later on.
 */
static void findusername(up,gp)
char **up;
char **gp;
{
struct passwd   *pwp;
char             buf[MAXUSERNAME];
int              euid;
int              uid;
#ifdef MINGW32
    *up = strnsave("dme", 3);
    *gp = strnsave("everyone",8);    /* set to junk value */
#else

    if ((pwp = getpwuid(uid = getuid())) != (struct passwd *) NULL)
    {
        *up =strnsave(pwp->pw_name,strlen(pwp->pw_name));
        *gp = getgroupforname(*up);
    }
    else
    {                    /* access will be denied if the user can't be found */
        euid = geteuid();
        (void) sprintf(buf, "[%d/%d]", uid, euid);
        *up = strnsave(buf, strlen(buf));
        *gp = strnsave("-999",4);    /* set to junk value */
    }
#endif
    return;
}
/*
 * Return the userid for the Administrator
 */
char * getgroupforname(user)
char * user;
{
struct passwd    *pwp;
struct group    *gp;
char buf[MAXUSERNAME];
#ifdef MINGW32
    return strnsave("everyone",8);            /* that is, nobody */
#else
    if ((pwp = getpwnam(user)) == (struct passwd *) NULL)
        return strnsave("-999",4);            /* that is, nobody */
    if ((gp = getgrgid(pwp->pw_gid)) == (struct group *) NULL)
    {    /* Just use the number */
        (void) sprintf(buf, "%d", pwp->pw_gid);
        return strnsave(buf, strlen(buf));
    }
    else
        return strnsave(gp->gr_name,strlen(gp->gr_name));
#endif
}
/*
 *        Check user permissions.
 *        If insufficient access rights, log it and exit the program.
 */
void
checkperm(need1, queue)
int        need1;
QUEUE        *queue;
{
FILE    *fpw;
char        *cp;
time_t        t;
int        got;
char *queuename;

    if (((got = getperm(queue, sg.username)) == 0 || ((got & need1) != need1))
        && ((got = getperm(queue, sg.groupname)) == 0 || ((got & need1) != need1)))
    {
        time(&t);
        cp = ctime(&t);
        cp[24] = 0;
        if (queue == (QUEUE *) NULL)
            queuename="SUPERNAT";
        else
            queuename=queue->queue_dir;
#ifndef ULTRIX
        if ((fpw=openfile(sg.supernat,ACTLOG,"a+")) != (FILE *) NULL)
#else
        if ((fpw=openfile(sg.supernat,ACTLOG,"A+")) != (FILE *) NULL)
#endif
        {
        (void) fprintf(fpw,
"*** SUPERNAT Access Violation by %s, pid %d, queue %s at %s *** Needs %d, Has %d\n",
                sg.username,getpid(), queuename, cp, need1, got);
        (void) fclose(fpw);
        }
        exit(1);
    }
    return;
}
/*
 * Function to convert a 'rwx' permission string into an integer
 */
int
doperms(s)
char *s;
{
int i;

    for (i = 0 ; *s ; s++)
    {
        switch(*s)
        {
        case 'r' :
            i |= U_READ;
            break;
        case 'w' :
            i |= U_WRITE;
            break;
        case 'x' :
            i |= U_EXEC;
            break;
        case '-' :
            break;
        default :
            return(-1);
        }
    }
    return(i);
}
/*
 * Inverse function to convert an integer into a 'rwx' permission string
 */

char *
pperm(i)
int        i;
{
static char        buf[4];

        buf[0] = (i & U_READ) ? 'r' : '-';
        buf[1] = (i & U_WRITE) ? 'w' : '-';
        buf[2] = (i & U_EXEC) ? 'x' : '-';
        buf[3] = 0;
        return(buf);
}
#ifdef AIX
#ifdef SIGDANGER
void sigdanger()
{
    printf("pid %d SIGDANGER received!\n",getpid());
    return;
}
#endif
#endif

/****************************************************************************
 * Initialise the global variables and log the activity.
 */
void natinit(argc, argv)
int        argc;
char        **argv;
{
#ifdef AIX
#ifdef SIGDANGER
    sigset(SIGDANGER,sigdanger);
#endif
#endif
    if ((sg.supernat = getenv(SUPERNATSTRING)) == (char *) NULL)
        sg.supernat = DEFSUPERNAT;
    if ((sg.supernatdb = getenv(SUPERNATDB)) == (char *) NULL)
        sg.supernatdb = DEFSUPERNATDB;
    if ((sg.adminname = getenv("ADMINNAME")) == (char *) NULL)
        sg.adminname = ADMINNAME;
    if ((sg.admingroup = getenv("ADMINGROUP")) == (char *) NULL)
        sg.admingroup = ADMINGROUP;
    if ((sg.fifo_name = getenv("SUPERNAT_FIFO")) == (char *) NULL)
        sg.fifo_name = SUPERNAT_FIFO;
    if ((sg.lock_name = getenv("SUPERNAT_LOCK")) == (char *) NULL)
        sg.lock_name = SUPERNAT_LOCK;
    findusername(&sg.username,&sg.groupname);
    sg.dirname = sg.supernat;
    sg.cur_db = (DH_CON *) NULL;
    sg.errout = stderr;            /* Default error disposition */
/*
 * Security: define PATH, IFS and SHELL to suit 
 */ 
#ifndef ULTRIX
    putenv("PATH=/bin:/usr/bin");
    putenv("SHELL=/bin/sh");
#else
    putenv("PATH=/usr/local/bin:/usr/ucb:/bin:/usr/bin");
    putenv("SHELL=/bin/sh5");
#endif
    putenv("IFS= \t\n");
    natlog(argc,argv);
    return;
}
/*
 * Catch the alarm if the ball gets dropped
 */
static void drop_time()
{
    return;
}
/*
 * Catch the alarm if the ball gets dropped
 */
static void user_int()
{
    e2unlink(sg.lock_name);
    return;
}
/*
 * Notify natrun that something is to be done.
 *
 * Send a message on the FIFO to natrun.
 *
 * Don't care if it doesn't get through; return 1 if can get through,
 * 0 otherwise.
 *
 * The timer will not be set by any programs that call this routine.
 */
int wakeup_natrun(wake_up)
char * wake_up;
{
FILE * fifo, *rfifo;
char * results;
char buf[BUFSIZ];
int i;
int lfd;
int efd;
struct stat sbuf;
int len;
int (*prev_hup)();
int (*prev_int)();
int (*prev_quit)();

    if ((stat(sg.fifo_name, &sbuf)) < 0)
    {
        perror("FIFO does not exist");
 (void) fprintf(stderr,"Error: %d; perhaps natrun isn't running?\n",errno);
        return 0;
    }
    results = getnewfile(sg.supernat,"tmp");
    efd = 1;
#ifndef SOLAR
#ifndef MINGW32
    if (mknod(results,0010660,0) < 0)
    { /* create the error FIFO */
        (void) fprintf(stderr,"Error: %d\n",errno);
        perror("Cannot Create FIFO");
        strcpy(buf,wake_up);
        efd = -1;
    }
#endif
#endif
#ifdef SOLAR
    (void) sprintf(buf,"-e %s %s\n%s\n",results,wake_up,LOOK_STR);
#else
#ifdef MINGW32
    (void) sprintf(buf,"-e %s %s\n%s\n",results,wake_up,LOOK_STR);
#else
    (void) sprintf(buf,"-e %s %s\n",results,wake_up);
#endif
#endif
    len = strlen(buf);
/*
 * Attempt to deliver the message
 */
    for (;;)
    {
        i = 1;
        (void) sigset(SIGALRM,drop_time);
        while (!((lfd = open(sg.lock_name,
                       O_CREAT|O_WRONLY|O_EXCL,0x180L)) > -1))
        {
            (void) fprintf(stderr,"Waiting for lock...\n"); 
            sleep ( i * i); 
            i++;
        }
        prev_hup = sigset(SIGHUP,user_int);
        prev_int = sigset(SIGINT,user_int);
        prev_quit = sigset(SIGQUIT,user_int);
/*
 * Keep retrying the write
 */
        for (;;)
        {
            int ret=0;
#ifdef SOLAR
            if ((sg.fifo_fd = fifo_connect(results, sg.fifo_name)) < 0)
            {
                (void) e2unlink(sg.lock_name);
                (void) e2unlink(results);
                return 0;
            }
            if ( (fifo = fdopen(sg.fifo_fd,"w")) == (FILE *) NULL ||
                 (rfifo = fdopen(sg.fifo_fd,"r")) == (FILE *) NULL )
            {
                perror("FIFO connect failed");
     (void) fprintf(stderr,"Error: %d; perhaps natrun isn't running?\n",errno);
                return 0;
            }
            setbuf(rfifo, (char *) NULL);
#else
#ifdef MINGW32
            if ((sg.fifo_fd = fifo_connect(results, sg.fifo_name)) < 0)
            {
                (void) e2unlink(sg.lock_name);
                (void) e2unlink(results);
                return 0;
            }
            if ( (fifo = fdopen(sg.fifo_fd,"wb")) == (FILE *) NULL ||
                 (rfifo = fdopen(sg.fifo_fd,"rb")) == (FILE *) NULL )
            {
                perror("FIFO connect failed");
     (void) fprintf(stderr,"Error: %d; perhaps natrun isn't running?\n",errno);
                return 0;
            }
            setbuf(rfifo, (char *) NULL);
#else
            if ((fifo=fopen(sg.fifo_name,"w")) == (FILE *) NULL)
            {
                perror("Cannot Open FIFO");
                (void) sigset(SIGHUP,prev_hup);
                (void) sigset(SIGINT,prev_int);
                (void) sigset(SIGQUIT,prev_quit);
                (void) e2unlink(results);
                (void) e2unlink(sg.lock_name);
                (void) free(results);
                return(0);
            }
            else
#endif
#endif
            {
                setbuf(fifo,(char *) NULL);
            }
            if ((ret = fwrite(buf,sizeof(char),len,fifo)) < len)
            {
                perror("FIFO Write Failed");
                (void) e2unlink(sg.lock_name);
                return 0;
            }
#ifdef SOLAR
#ifndef PTX4
            if (shutdown(sg.fifo_fd,1))
                perror("Socket shutdown failed");
#endif
#else
            (void) fclose(fifo);
#endif
            if (ret == len)
                break; 
            (void) e2unlink(sg.lock_name);
        }
        (void) close(lfd);
        (void) sigset(SIGHUP,drop_time);
        (void) sigset(SIGINT,drop_time);
        (void) sigset(SIGQUIT,drop_time);
        if (efd > -1)
        {
#ifdef SOLAR
            fifo = rfifo;
#else
#ifdef MINGW32
            fifo = rfifo;
#else
            if ((fifo=fopen(results,"r")) == (FILE *) NULL)
            {
                if (errno != EINTR)
                    perror("Cannot Open Return FIFO");
            }
            else
#endif
#endif
            {
                while ((efd = fread(buf,sizeof(char),sizeof(buf),fifo)) > 0)
                    (void) fwrite(buf,sizeof(char),efd,stderr);
                if (efd < 0)
                {
                    perror("Results message collection failed");
                }
                (void) fclose(fifo);
                break;
            }
        }
    }
    (void) sigset(SIGHUP,prev_hup);
    (void) sigset(SIGINT,prev_int);
    (void) sigset(SIGQUIT,prev_quit);
    (void) e2unlink(results);
    (void) free(results);
    return 1;
}
