/* wipe
 * $Id: random.h,v 1.1.1.1 2002/11/25 23:59:49 berke Exp $
 * by Berke Durak
 * Author may be contacted at 'echo berke1ouvaton2org|tr 12 @.'
 *
 * Header file for cryptographically-strong (as well as weak)
 * random data generation routines
 *
 */

#ifndef RANDOM_H
#define RANDOM_H

#ifndef U32U16U8
typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;
#define U32U16U8
#endif

void rand_Init ();
#define rand_Get32 rand_Get32p
#define rand_Fill rand_Fillp
#if 0
u32 rand_Get32 ();
#endif

extern u32 (*rand_Get32p) ();
extern void (*rand_Fill) (u8 *, int);

extern char *o_devrandom;
#define o_randomcmd o_devrandom
extern int o_randseed;
extern int o_randalgo;
extern char **environ;

#define RANDS_DEVRANDOM 0
#define RANDS_PIPE 1
#define RANDS_PID 2

#define RANDA_LIBC 0
#define RANDA_RC6 1
#define RANDA_ARCFOUR 2

#define RAND_ARCFOUR_EXTRA 8192

#endif

/* vim:set sw=4:set ts=8: */
