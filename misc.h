/* wipe
 * $Id: misc.h,v 1.1.1.1 2002/11/25 23:59:49 berke Exp $
 * by Berke Durak
 * Author may be contacted at 'echo berke1ouvaton2org|tr 12 @.'
 *
 * URL for wipe: http://abaababa.ouvaton.org/wipe/
 *
 * General-purpose miscellaneous routines: debugging, etc.
 *
 */

#ifndef MISC_H
#define MISC_H

#include <stdlib.h>

#define ERF_MASK 0x0000ffff
#define ERF_EXIT 0x80000000
#define ERF_RET0 0x40000000
#define ERF_ERN  0x20000000

int errorf (unsigned long e, char *fmt, ...);
void informf (char *fmt, ...);
char *msprintf (char *fmt, ...);
void *xmalloc (size_t l);

#ifdef DEBUG
void debug_pf (char *fmt, ...);
#define debugf(x, y...) debug_pf(x,## y)
#else
#define debugf(x, y...)
#endif

#endif

/* vim:set sw=4:set ts=8: */
