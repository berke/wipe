/* wipe
 * $Id: misc.c,v 1.1.1.1 2002/11/25 23:59:49 berke Exp $
 * by Berke Durak
 * Author may be contacted at 'echo berke1ouvaton2org|tr 12 @.'
 *
 * URL for wipe: http://abaababa.ouvaton.org/wipe/
 *
 * General-purpose miscellaneous routines: debugging, etc.
 *
 */

#include <stdlib.h> 
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "misc.h"

void *xmalloc (size_t l)
{
  void *m;

  m = malloc (l);
  if (!m) {
    errorf (0, "could not allocate %ld bytes", l);
    exit (EXIT_FAILURE);
  }

  return m;
}

int errorf (unsigned long e, char *fmt, ...)
{
  va_list arg;
  int en;

  en = (e & ERF_ERN)?errno:e & ERF_MASK;

  va_start (arg, fmt);
  fprintf (stderr, "Error: ");
  vfprintf (stderr, fmt, arg);
  if (en) fprintf (stderr, " (%s -- error %d)", strerror (en), en);
  fputc ('\n', stderr);
  va_end (arg);

  if (e & ERF_EXIT) exit (EXIT_FAILURE);
  if (e & ERF_RET0) return 0;
  return -1;
}

void informf (char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  vfprintf (stderr, fmt, arg);
  fputc ('\n', stderr);
  va_end (arg);
}

char *msprintf (char *fmt, ...)
{
  char *s;
  va_list arg;
  char buf[1024];

  va_start (arg, fmt);
  vsprintf (buf, fmt, arg);
  s = xmalloc (strlen (buf)+1);
  strcpy (s, buf);
  va_end (arg);
  return s;
}
#ifdef DEBUG
void debug_pf (char *fmt, ...)
{
  va_list arg;

  va_start (arg, fmt);
  fprintf (stderr, "debug: ");
  vfprintf (stderr, fmt, arg);
  fputc ('\n', stderr);
  va_end (arg);
}
#define debugf(x, y...) debug_pf(x,## y)
#else
#define debugf(x, y...)
#endif

/* vim:set sw=4:set ts=8: */
