/*
 * machine dependencies
 *
 */

#include "defs.h"

#if	defined(BSD4)
#define	strchr	index
#define srand48	srandom
#define lrand48	random
#endif

#if	defined(USGV3)
#define	srand48	srand
#define	lrand48	rand
#define	_doprnt	_dprnt
#define	setegid	setgid
#define	seteuid	setuid
#endif
