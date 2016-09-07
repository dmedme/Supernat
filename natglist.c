/************************************************************
 * natglist.c - program to list user access rights on a queue
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1990";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "supernat.h"
/*
 *	Usage: natglist [-l] dir...
 */
int natglist_main(argc, argv, envp)
int	argc;
char	**argv;
char	**envp;
{
	USER	*up;
	FILE	*fp;
	char	buf[BUFSIZ];
	QUEUE	*queue;
	int	lflag = 0;
	int	tot;
	int	i;
	int	n;
	int	c;
        QUEUE * anchor;

	/* see if a long listing's wanted */
	while ((c = getopt(argc, argv, "l")) != EOF) {
		switch(c) {
		case 'l' :
			lflag = 1;
			break;
		}
	}
        NEW(QUEUE,queue);
	natinit(argc, argv);
        anchor = (QUEUE * ) NULL;
	for (i = optind, tot = 0; i < argc ; i++)
        {
		/* check queue exists */
            strcpy(queue->queue_name,argv[i]);
		if (queuetodir(queue,&anchor) == (char *) NULL)
                {
			ERROR(__FILE__, __LINE__, 0,
                            "No such queue %s", argv[i], NULL);
			continue;
		}
		if (optind < argc - 1)
			printf("%s:\n", argv[i]);

		/* check that user can read the directory */
		checkperm(U_READ|U_WRITE|U_EXEC, queue);
		if ((fp = openfile(sg.supernat, grantname(queue), "r")) != (FILE *) NULL) {
			for (; (up = getuserent(fp)) != (USER *) NULL ; tot++) {
				printf("%s", up->u_name);
				if (lflag)
					printf("\t%s", pperm(up->u_perm));
				(void) putchar('\n');
			}
			(void) fclose(fp);
		}
		if (lflag)
			printf("Total: %d user%s\n", tot, (tot == 1)? "" : "s");
	}
	exit(0);
}
