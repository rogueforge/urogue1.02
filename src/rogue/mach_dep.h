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
