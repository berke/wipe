/* wipe
 * $Id: arcfour.c,v 1.1.1.1 2002/11/25 23:59:49 berke Exp $
 * by Berke Durak
 * Author may be contacted at 'echo berke1ouvaton2org|tr 12 @.'
 *
 * URL for wipe: http://abaababa.ouvaton.org/wipe/
 *
 * Arcfour implementation, beta.
 *
 */

#include "arcfour.h"

void arcfour_SetupKey (u8 *k, int n, struct arcfour_KeySchedule *ks)
{
  int i, j;
  u8 kt[256];

  for (i = 0, j = 0; i<256; i++) {
    ks->s[i] = i;
    kt[i] = k[j];
    j ++; if (j == n) j = 0;
  }

  for (i = 0, j = 0; i<256; i++) {
    u8 t;

    j = 0xff & (j + ks->s[i] + kt[i]);
    t = ks->s[i];
    ks->s[i] = ks->s[j];
    ks->s[j] = t;
  }

  ks->i = i; ks->j = j;
}

inline u8 arcfour_GetByte (struct arcfour_KeySchedule *ks)
{
  int i, j;
  u8 x;

  i = ks->i; j = ks->j;

  i = 0xff & (i+1); j = (j + ks->s[i]) & 0xff;
  x = ks->s[i];
  ks->s[i] = ks->s[j];
  ks->s[j] = x;
  ks->i = i; ks->j = j;
  return ks->s[(ks->s[i] + ks->s[j]) & 0xff];
}

void arcfour_Fill (struct arcfour_KeySchedule *ks, u8 *b, int n)
{
  while (n--) *(b++) = arcfour_GetByte (ks);
}

#ifdef TEST_ARCFOUR
#define TESTKEY "thisisamoddafokkingtest"
#include <stdio.h>

int main (int argc, char **argv)
{
  int i;
  struct arcfour_KeySchedule ks;

  arcfour_SetupKey (TESTKEY, strlen (TESTKEY), &ks);

  for (i = 0; i<10240; i++) {
    putchar (arcfour_GetByte (&ks));
  }

  return 0;
}
#undef TESTKEY
#endif

/* vim:set sw=4:set ts=8: */
