/*
 *    natulib.c
 *
 *    Copyright (c) E2 Systems, UK, 1990. All rights reserved.
 *
 *    Common utility routines for nat system.
 *
 * 17 January 1992 : Changed so that files are always created as the 
 *                   admin user, so that root doesn't own things manipulated
 *                   by natrun.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 1993\n";
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#ifdef AIX
#include <fcntl.h>
#include <time.h>
#else
#ifndef ULTRIX
#include <sys/fcntl.h>
#endif
#endif
#ifdef MINGW32
#include <fcntl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include "supernat.h"
struct passwd * getpwnam();
struct group * getgrnam();

/***********************************************
 * General Utility Routines
 *
 *    convert a user record to a string.
 */
char *
user2s(up)
USER    *up;
{
    char    buf[BUFSIZ];

    (void) sprintf(buf, "%c%c%c%s\n", ((up->u_perm & U_READ) ? 'r' : '-'),
                    ((up->u_perm & U_WRITE) ? 'w' : '-'),
                    ((up->u_perm & U_EXEC) ? 'x' : '-'),
                    up->u_name);
    return(strnsave(buf, strlen(buf)));
}
/*
 *    convert a string to a user record.
 */
USER *
s2user(s)
char    *s;
{
    USER    *up;
    NEW(USER, up);
    up->u_perm = ((s[0] == 'r') ? U_READ : 0) |
            ((s[1] == 'w') ? U_WRITE : 0) |
            ((s[2] == 'x') ? U_EXEC : 0);
    (void) strncpy(up->u_name, &s[3], sizeof(up->u_name) - 1);
    up->u_name[sizeof(up->u_name) - 1] = 0;
    return(up);
}
/*
 * Return the userid for the Administrator
 */
int getadminuid()
{
#ifdef MINGW32
    return 0;
#else
    struct passwd    *pwp;
    if ((pwp = getpwnam(sg.adminname)) == (struct passwd *) NULL)
        return -2;            /* that is, nobody */
    else return pwp->pw_uid;
#endif
}
/*
 * Return the group for the Administrator
 */
int getadmingid()
{
#ifdef MINGW32
    return 0;
#else
    struct group    *gp;
    if ((gp = getgrnam(sg.admingroup)) == (struct group *) NULL)
        return -2;            /* that is, nobody */
    else return gp->gr_gid;
#endif
}
/*
 *        Log the arguments to the global log file.
 */
void
natlog( argc, argv)
int        argc;
char        **argv;
{
    FILE    *fpw;
    char        buf[BUFSIZ];
    char        *cp;
    time_t     t;
    int        i;

    time(&t);
    cp = (char *) ctime(&t);
    cp[24] = 0;
    (void) sprintf(buf, "%s, %s, %d, ", sg.username,cp,getpid());
    for (i = 0 ; i < argc ; i++)
    {
        (void) strcat(buf, argv[i]);
        (void) strcat(buf, " ");
    }
    (void) strcat(buf, "\n");
#ifndef ULTRIX
    if ((fpw=openfile(sg.supernat,ACTLOG,"a+")) != (FILE *) NULL)
#else
    if ((fpw=openfile(sg.supernat,ACTLOG,"A+")) != (FILE *) NULL)
#endif
    {
        (void) fprintf(fpw,buf);
        (void) fclose(fpw);
    }
    return;
}
/*
 *    Open the file for sg.dirname, filename, and return the fp.
 *      Lock the file to prevent modification by others.
 *      Updates are done by a 'master file update' technique; it
 *      is therefore essential to prevent people getting hold of the
 *      the old version simultaneously.
 */
FILE *
openfile(d, f, mode)
char    *d;
char    *f;
char    *mode;
{
#ifndef ULTRIX
        static struct f_mode_flags {
          char * modes;
          int flags;
        } f_mode_flags[] =
{{ "r", O_RDONLY },
{"w", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY  },
{"a", O_CREAT | O_WRONLY | O_APPEND  },
{"r+",  O_RDWR  },
{"w+", O_CREAT | O_TRUNC | O_RDWR  },
{"a+", O_CREAT | O_APPEND | O_RDWR  },
{0,0}};
#ifndef MINGW32
       static struct flock fl = {F_WRLCK,0,0,0,0};
#endif
#else
        static struct f_mode_flags {
          char * modes;
          int flags;
        } f_mode_flags[] =
{{ "r", O_RDONLY },
{"w", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY | O_BLKANDSET },
{"a", O_CREAT | O_WRONLY | O_APPEND | O_BLKANDSET },
{"A", O_CREAT | O_APPEND | O_WRONLY | O_BLKANDSET },
{"r+",  O_RDWR },
{"w+", O_CREAT | O_TRUNC | O_RDWR | O_BLKANDSET },
{"a+", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
{"A+", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
{0,0}};
#endif
    register struct f_mode_flags * fm;
    static int cmode=0660;
    FILE    *fp;
    char    *name;
    int fd;
    int oumask,numask=0x7;     /* Make sure the mode flags on creation are the
                                   ones that we want */
    for (fm = f_mode_flags;
             fm->modes != (char *) NULL && strcmp(fm->modes,mode);
                 fm++);
    if (fm->modes == (char *) NULL)
    {
        ERROR(__FILE__, __LINE__, 0, "invalid file mode %s", mode, NULL);
        return (FILE *) NULL;
    }
    if (*f != '/')
        name = filename("%s/%s", d, f);
    else
        name = strnsave(f,strlen(f));

    oumask = umask(numask);
#ifndef ULTRIX
    for (fd = -1; fd == -1;)
    {   /* until we have the file exclusively to ourselves */
        if ((fd = open(name,fm->flags,cmode)) < 0)
        {
            (void) free(name);
            (void) umask(oumask);
            return (FILE *) NULL;
        }
#ifndef AIX
#ifndef MINGW32
        if (fm->flags != O_RDONLY && fcntl(fd,F_SETLK,&fl) == -1)
        {
            close(fd);
            fd = -1;
            sleep(1);
        }
#endif
#endif
    }
#else
    if ((fd = open(name,fm->flags,cmode)) < 0)
    {
        (void) free(name);
        (void) umask(oumask);
        return (FILE *) NULL;
    }
#endif
    (void) umask(oumask);
    (void) chown(name,getadminuid(),getadmingid());
    if ((fp = fdopen(fd, mode)) == (FILE *) NULL)
    {
        ERROR(__FILE__, __LINE__, 0, "can't fdopen %s", name, NULL);
    }
    else
        setbuf(fp, (char *) NULL);
    (void) free(name);
    return(fp);
}
/*
 * routine to exec a sub-process owned by the person in question,
 * and connected to the passed file descriptors. Used:
 * - For job execution
 * - As a secure implementation of SYSTEM() for natcalc.
 */
#ifdef MINGW32
#ifndef O_NOINHERIT
#define O_NOINHERIT 0x80
#endif
#ifndef P_DETACH
#define P_DETACH 4
#endif
int lcmd(rem_in,rem_out,rem_err,job_owner,job_group,name,argv,envp)
int * rem_in;
int * rem_out;
int * rem_err;
char * job_owner;
char * job_group;
char * name;
char ** argv;
char ** envp;
{
/*
 * This version of the code doesn't do any of the proper security stuff
 */
int hthread;
HANDLE hthread1;
int pwrite[2];
int pread[2];
int perr[2];
int h0;
int h1;
int h2;
STARTUPINFO si;
PROCESS_INFORMATION pi;

    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwX = 0;
    si.dwY = 0;
    si.dwXSize = 0;
    si.dwYSize= 0;
    si.dwXCountChars = 0;
    si.dwYCountChars= 0;
    si.dwFillAttribute= 0;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.wShowWindow = 0;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;
/*
 * Create the read pipe
 */
    if (_pipe(&pread[0],4096,O_BINARY|O_NOINHERIT))
    {
        fprintf(sg.errout,"pipe(pread...) failed error:%d\n", errno);
        return 0;
    }
#ifdef DEBUG
    else
        fprintf(sg.errout,"pipe(pread...) gives files:(%d,%d)\n", pread[0],
                   pread[1]);
#endif
    if (_pipe(&pwrite[0],4096,O_BINARY|O_NOINHERIT))
    {
        fprintf(sg.errout,"pipe(pwrite...) failed error:%d\n", errno);
        return 0;
    }
#ifdef DEBUG
    else
        fprintf(sg.errout,"pipe(pwrite...) gives files:(%d,%d)\n", pwrite[0],
                   pwrite[1]);
#endif
    if (_pipe(&perr[0],4096,O_BINARY|O_NOINHERIT))
    {
        fprintf(sg.errout,"pipe(perr...) failed error:%d\n", errno);
        return;
    }
#ifdef DEBUG
    else
        fprintf(sg.errout,"pipe(perr...) gives files:(%d,%d)\n", perr[0],
                   perr[1]);
#endif
/*
 * Set up the pipe handles for inheritance
 */
    if ((h0 = _dup(pread[0])) < 0)
    {
        fprintf(sg.errout,"dup2(pread[0],0) failed error:%d\n", errno);
        return 0;
    }
    close(pread[0]);
    if ((h1 = _dup(pwrite[1])) < 0)
    {
        fprintf(sg.errout,"dup2(pwrite[1],1) failed error:%d\n", errno);
        return 0;
    }
    close(pwrite[1]);
    if (( h2 = _dup(perr[1])) < 0)
    {
        fprintf(sg.errout,"dup2(perr[1],2) failed error:%d\n", errno);
        return 0;
    }
    close(perr[1]);
/*
 * Restore the normal handles
 */
    si.hStdInput = (HANDLE) _get_osfhandle(h0);
    si.hStdOutput = (HANDLE) _get_osfhandle(h1);
    si.hStdError = (HANDLE) _get_osfhandle(h2);

    if (!CreateProcess(argv[0], argv[0], NULL, NULL, TRUE,
                  0, NULL, NULL, &si, &pi))
    {
        fprintf(sg.errout,"Create process failed error %d\n",errno);
        *rem_in = -1;
        *rem_out = -1;
        *rem_err = -1;
        close(pread[1]);
        close(pwrite[0]);
        close(perr[0]);
    }
#ifdef DEBUG
    else
        fputs("Create process succeeded\n", sg.errout);
#endif
    fflush(sg.errout);
    close(h0);
    close(h1);
    close(h2);
    *rem_out = pread[1];
    *rem_in = pwrite[0];
    *rem_err = perr[0];
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return pi.dwProcessId;
}
#else
int lcmd(rem_in,rem_out,rem_err,job_owner,job_group,name,argv,envp)
int * rem_in;
int * rem_out;
int * rem_err;
char * job_owner;
char * job_group;
char * name;
char ** argv;
char ** envp;
{
    int i;
    int in[2];
    int out[2];
    int err[2];

    (void) pipe(in); 
    (void) pipe(out); 
    (void) pipe(err); 
           
    if ((i=fork()) == 0)
    {
        struct passwd * p;
        struct group * g;
        int gid;
/*
 * CHILD
 */
        if ((p = getpwnam(job_owner)) == (struct passwd *) NULL)
        {
            char * x = "Non-existent job owner";
            natlog(1,&x);
            (void) fprintf(stderr,"No such owner %s!\n",job_owner);
            exit(1);
        }
        if ((g = getgrnam(job_group)) == (struct group *) NULL)
            gid = p->pw_gid;
        else
            gid = g->gr_gid;
        (void) close(in[1]);
        (void) close(out[0]);
        (void) close(err[0]);
        dup2(in[0],0);
        dup2(out[1],1);
        dup2(err[1],2);
        if (setgid(gid) < 0)
        {
            char * x = "Can't setgid() to job group";
            natlog(1,&x);
            (void) fprintf(stderr,"Can't setgid() to job group %d\n",gid);
            exit(1);
        }
#ifdef SCO
#ifndef V4
#define NOIG
#endif
#endif
#ifndef NOIG
        if (initgroups(job_owner,gid) < 0)
        {
            char * x = "Can't set up supplementary groups with initgroups()";
            natlog(1,&x);
            (void) fprintf(stderr,
                          "Can't initgroups(%s,%d)\n",job_owner,gid);
            exit(1);
        }
#endif
        if (setuid(p->pw_uid)  < 0)
        {
            char * x = "Can't setuid() to job owner";
            natlog(1,&x);
            (void) fprintf(stderr,"Can't setuid() to job owner %s\n",job_owner);
            exit(1);
        }
        if (execve(name,argv,envp)  < 0)
        {
            char * x = "Can't execute command";
            natlog(1,&x);
            perror("execve() failed");
            exit(1);
        }
    }
/*
 * PARENT
 */
    else if (i<0)
    {
        perror("Fork to spawn shell failed");
        *rem_in = -1;
        *rem_out = -1;
        *rem_err = -1;
        (void) close(in[0]);
        (void) close(out[1]);
        (void) close(err[1]);
    }
    else
    {
        *rem_out = in[1];
        *rem_in = out[0];
        *rem_err = err[0];
        (void) close(in[0]);
        (void) close(out[1]);
        (void) close(err[1]);
    }
    return i;
}
#endif
#ifdef AIX
int e2unlink(fname)
char * fname;
{
struct stat sbuf;
char *x[2];

    x[0] = "Unlinking file";
    x[1] = fname;
    natlog(2,x);
    if (stat(fname, &sbuf) < 0)
    {
        x[0] = "Non-existent file";
        natlog(2,x);
        return -1;
    }
    else
    {
        if (S_ISDIR(sbuf.st_mode))
        {
            x[0] = "Attempting to unlink directory";
            x[1] = fname;
            natlog(2,x);
            return -1;
        }
        return unlink(fname);
    }
}
#endif
#ifdef MINGW32
DIR * opendir(fname)
char * fname;
{
char buf[260];
DIR * d = (struct dirent *) malloc(sizeof(struct dirent));

    strcpy(buf, fname);
    strcat(buf, "/*");
    if ((d->handle = _findfirst(buf, &(d->f))) < 0)
    {
        fprintf(stderr,"Failed looking for %s error %d\n",
                          buf, GetLastError());
        fflush(stderr);
        free((char *) d);
        return (DIR *) NULL;
    }
    d->read_cnt = 0;
    return d;
}
struct dirent * readdir(d)
DIR * d;
{
    if (d->read_cnt == 0)
    {
        d->read_cnt = 1;
        return d;
    }
    if (_findnext(d->handle, &(d->f)) < 0)
        return (struct dirent *) NULL;
    return d;
}
void closedir(d)
DIR *d;
{
    _findclose(d->handle);
    free((char *) d);
    return;
}
#endif
