/*********************************************
 * defs.h - useful macros
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems, 1990
 * $Header$
 */
#ifndef DEFS_H
#define DEFS_H

#define ERROR(fn, ln, ex, fmt, s1, s2) { \
(void) fprintf((sg.errout == (FILE *) NULL)?stderr:sg.errout,\
         "SUPERNAT: Error detected by File \"%s\" at Line %d:\n", fn, ln);\
(void) fprintf((sg.errout == (FILE *) NULL)?stderr:sg.errout, fmt, s1, s2); \
(void) fputc('\n', (sg.errout == (FILE *) NULL)?stderr:sg.errout); \
if (ex) \
   exit(ex); \
}
	
#define NEWARRAY(type, ptr, nel) \
	if ((ptr = (type *) calloc((unsigned) nel, sizeof(type))) == (type *) NULL) \
		ERROR(__FILE__, __LINE__, 1, "can't calloc %d bytes", (nel * sizeof(type)), (char *) NULL)

#define RENEW(type, ptr, nel) \
	if ((ptr = (type *) realloc((char *)ptr, (unsigned)(sizeof(type) * (nel)))) == (type *) NULL) \
		ERROR(__FILE__, __LINE__, 1, "can't realloc %d bytes", (nel * sizeof(type)), (char *) NULL)

#define NEW(type, ptr)	NEWARRAY(type, ptr, 1)

#define FREE(s)		(void) free((char *) s)

#define ZAP(type, s)	(FREE(s), s = (type *) NULL)

#ifndef MAX
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#endif /* !MAX */

#ifndef MIN
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif /* !MIN */

#define PREV(i, n)	(((i) - 1) % (n))
#define THIS(i, n)	((i) % (n))
#define NEXT(i, n)	(((i) + 1) % (n))
#define STRCPY(x,y) {strncpy((x),(y),sizeof(x)-1);*((x)+sizeof(x)-1)='\0';}

#endif /* !DEFS_H */
