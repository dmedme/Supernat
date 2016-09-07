/*******************************************************************
*
* semlock.c
*
*   Notes on Use
*   ------------
*
*   Manipulates locks based on exclusive access to a given file
*
*   Options
*   --------
*    -s test and set
*    -u unset
*
*
*   Assumptions
*   -----------
*
*   This program implements a simple locking mechanism to control
*   access to server named pipes.
*
*   The mechanism is based on exclusive access to a file.
*   It ensures that only one process attempts to write to
*   the pipe at the same time, since interleaving
*   of multiple writes is possible (though unlikely) if processes are allowed
*   to write to the named pipe indiscriminately.
* 
*   Pseudo-code
*   -----------
*  
* Process the arguments.
*
* Do it.
*
* Return success if -s and created, or -u and removed.
*
* Return fail otherwise.
*/  
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1992";

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/***********************************************************************
 *
 * Getopt support
 */
extern int optind;           /* Current Argument counter.      */
extern char *optarg;         /* Current Argument pointer.      */
extern int opterr;           /* getopt() err print flag.       */
extern int errno;
char buf[4096];              /* buffer for file copy           */

/**************************************************************************
 * Main Program Start Here
 * VVVVVVVVVVVVVVVVVVVVVVV
 */
int main(argc,argv,envp)
int     argc;
char    *argv[];
char    *envp[];
{
int c;

    while ( ( c = getopt ( argc, argv, "s:u:" ) ) != EOF )
    {
        switch ( c )
        {
        case 's' :
/*  -s filename (set the lock)                                                */
            if (open(optarg,O_CREAT | O_EXCL,0) < 0) 
                 /* create the lock file */
                exit(1);
            else exit(0);
            break;
        case 'u' :
/*  -u filename   (set the lock)                                */
            if ( unlink(optarg))
                 /* remove the lock file */
                exit(1);
            else exit(0);
            break;
        default:
        case '?' : /* Default - invalid opt.*/
            (void) fprintf(stderr,"usage semlock {-s filename | -u filename}\n");
            exit(1);
            break;
        }
    }
    exit(0);
}
