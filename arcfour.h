/* wipe
 * $Id: arcfour.h,v 1.1.1.1 2002/11/25 23:59:49 berke Exp $
 * by Berke Durak
 * Author may be contacted at 'echo berke1ouvaton2org|tr 12 @.'
 *
 * URL for wipe: http://abaababa.ouvaton.org/wipe/
 *
 * Arcfour implementation, beta.
 *
 */

#ifndef ARCFOUR_H
#define ARCFOUR_H
#include <stdio.h>

#ifndef U32U16U8
typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;
#define U32U16U8
#endif

struct arcfour_KeySchedule {
	int i, j;
	u8 s[256];
};

void arcfour_SetupKey (u8 *k, int n, struct arcfour_KeySchedule *ks);
u8 arcfour_GetByte (struct arcfour_KeySchedule *ks);
void arcfour_Fill (struct arcfour_KeySchedule *ks, u8 *b, int n);
#endif
