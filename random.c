/* wipe
 * 
 * by Berke Durak
 *
 * Cryptographically-strong (as well as weak) random data generation
 *
 */

/* As of version 0.10, different methods for generating
 * the required random data can be selected at compile-time.
 *
 * This improvement was motivated by security and portability
 * reasons.
 *
 * Prior to version 0.10, cryptographically _weak_ random data
 * was generated using the following method:
 *
 *   - libc's random () PRNG was seeded with data from /dev/urandom
 *     (and if not available using getpid () ^ clock () !)
 *   - since random () returned only 31 bits of data, for speed reasons,
 *     the low 24 bits were used.
 *
 * I don't know how much the cryptographical strength of the random
 * sequences used to overwrite the magnetic data affect the chances
 * of recovery.
 *
 * Besides this potential problem, there is something much more
 * worrying: since wipe is a user-mode program, it uses write ()
 * to overwrite the data. However nothing guarantees that the
 * same file blocks are used. I guess a normal filesystem would
 * not waste its time reallocating file data blocks when overwriting
 * the old ones would suffice but I can't guarantee this.
 *
 * The only way to solve this would be to do block device-level
 * wiping, which requires deep knowledge of the file system structure.
 * The best solution would be to have file-systems incorporate
 * secure wiping options. On Linux, the ext2fs provides a "s"
 * file attribute, which makes the file system overwrite the
 * file with zeroes when it is unlinked: this could be changed
 * to implement a secure erasure mechanism.
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "misc.h"
//#include "rc6.h"
#include "arcfour.h"
#include "md5.h"
#include "random.h"

#ifdef RC6_ENABLED
static struct rc6_KeySchedule rand_rc6;
static u32 rand_rc6_array[2][4];
static int rand_rc6_i;
#endif

u32 (*rand_Get32p) (void);
void (*rand_Fillp) (u8 *b, int n);

static struct arcfour_KeySchedule rand_arcfour;
static u32 rand_extra;
static u32 rand_extra_i;

static void rand_Get128BitsPID (u8 buf[16])
{
  MD5_CTX md5;
  pid_t p;
  time_t t;
  char **pt;

  debugf ("using poor man's 128 bit random seeder");
  MD5Init (&md5);

  p = getpid ();
  t = time (0);

  for (pt = environ; *pt; pt++) MD5Update (&md5, (u8 *) *pt, strlen (*pt));
  MD5Update (&md5, (u8 *) &p, sizeof (p));
  MD5Update (&md5, (u8 *) &t, sizeof (t));

  MD5Final (buf, &md5);
}

static void rand_Get128BitsDevRandom (u8 buf[16])
{
  int fd;

  debugf ("getting 128 bits from %s", o_devrandom);
  fd = open (o_devrandom, O_RDONLY);
  if (fd < 0)
    errorf (ERF_ERN|ERF_EXIT, "could not open \"%s\" for random seeding", o_devrandom);
  if (16 != read (fd, buf, 16))
    errorf (ERF_ERN|ERF_EXIT, "short read or read error from \"%s\" for random seeding", o_devrandom);
  (void) close (fd);	/* we got what we wanted, why should we check for errors ? */
}

#define MINIMUM_PIPE_BYTES 128L

static void rand_Get128BitsPipe (u8 buf128[16])
{
  FILE *f;
  MD5_CTX md5;
  u8 buf[1024];
  size_t r, totr;

  debugf ("getting 128 bits by hashing output of %s", o_randomcmd);

  MD5Init (&md5);
  f = popen (o_randomcmd, "r");
  if (!f) errorf (ERF_ERN|ERF_EXIT, "could not popen () command \"%s\" for random seeding", o_randomcmd);

  for (totr = 0; ;) {
    r = fread (buf, 1, sizeof (buf), f);

    if (r > 0) {
      MD5Update (&md5, buf, r);
      totr += r;
    }

    if (r < sizeof(buf)) {
      if (!feof (f))
        errorf (ERF_ERN|ERF_EXIT, "error reading from pipe \"%s\"", o_randomcmd);
      break;
    }
  }

  if (totr < MINIMUM_PIPE_BYTES)
    errorf (ERF_EXIT,
        "pipe command \"%s\" must output at least %ld bytes (got %ld)",
        o_randomcmd,
        (long) MINIMUM_PIPE_BYTES,
        totr);

  if (pclose (f)) errorf (ERF_ERN|ERF_EXIT, "error on pclose ()");
  MD5Final (buf128, &md5);
}

#ifdef RC6_ENABLED
inline static u32 rand_Get32_rc6 ()
{
  rand_rc6_i ++; if (rand_rc6_i == 4) rand_rc6_i = 0;

  if (!rand_rc6_i) {
    /* elegant incrementation tip by Ben Laurie <ben@algroup.co.uk> */
    randa_rc6_array[0][0]++ || randa_rc6_array[0][1]++
      || randa_rc6_array[0][2]++ || randa_rc6_array[0][3]++;
    rc6_Encrypt (rand_rc6_array[0], rand_rc6_array[1], &rand_rc6);
  }
  return rand_rc6_array[rand_rc6_i];
}
#endif

static u32 rand_Get32_arcfour ()
{
  u32 x;

  x = 0;
  x |= arcfour_GetByte (&rand_arcfour); x <<= 8;
  x |= arcfour_GetByte (&rand_arcfour); x <<= 8;
  x |= arcfour_GetByte (&rand_arcfour); x <<= 8;
  x |= arcfour_GetByte (&rand_arcfour);

  return x;
}

inline static u32 rand_Get32_libc ()
{
  u32 r;

#ifdef HAVE_RANDOM
  r = random ();
#else
  r = rand ();
#endif
  if (!rand_extra_i) {
#ifdef HAVE_RANDOM
    rand_extra = random () << 1;
#else
    rand_extra = rand () << 1;
#endif
    rand_extra_i = 31;
  } else rand_extra_i --;

  r ^= rand_extra & 0x80000000;
  rand_extra <<= 1;
  return r;
}

#if RC6_ENABLED
static void rand_Fill_rc6 (u8 *b, int n)
{
  u32 l;

  /* there used to be a stupid bug here, thanks to Michael S.Ree for
   * pointing this out.
   */

  for (; n >= 4; n -= 4) {
    l = rand_Get32_rc6 ();

    *((u32 *) b) = l;
    b = (char *) (((u32 *) b) + 1);
  }

  if (n) {
    l = rand_Get32_rc6 ();
    while (n--) {
      *(b++) = l & 0xff; l >>= 8;
    }
  }
}
#endif

static void rand_Fill_libc (u8 *b, int n)
{
  u32 l;

  /* there used to be a stupid bug here, thanks to Michael S.Ree for
   * pointing this out.
   */

  for (; n >= 4; n -= 4) {
    l = rand_Get32_libc ();

    *((u32 *) b) = l;
    b = (u8 *) (((u32 *) b) + 1);
  }

  if (n) {
    l = rand_Get32 ();
    while (n--) {
      *(b++) = l & 0xff; l >>= 8;
    }
  }
}

static void rand_Fill_arcfour (u8 *b, int n)
{
  arcfour_Fill (&rand_arcfour, b, n);
}

#if 0
u32 rand_Get32 ()
{
  return rand_Get32p ();
}
#endif

void rand_Init ()
{
  u8 key[16];

  switch (o_randseed) {
    case RANDS_DEVRANDOM:
      debugf ("using random device %s", o_devrandom);
      rand_Get128BitsDevRandom (key);
      break;
    case RANDS_PIPE:
      debugf ("using random seed pipe %s", o_devrandom);
      rand_Get128BitsPipe (key);
      break;
    case RANDS_PID:
      debugf ("using pid as random seed");
      rand_Get128BitsPID (key);
      break;
  }

  switch (o_randalgo) {
    case RANDA_LIBC:
      debugf ("using libc random generator");
#ifdef HAVE_RANDOM
      (void) srandom (key[0] | key[1]<<8 | key[2]<<16 | key[3]<<24);
#else
      srand (key[0] | key[1]<<8 | key[2]<<16 | key[3]<<24);
#endif
      rand_Get32p = rand_Get32_libc;
      rand_Fillp = rand_Fill_libc;

      rand_extra = key[4] | key[5]<<8 | key[6]<<16 | key[7]<<24;
      rand_extra_i = 32;
      break;
#ifdef RC6_ENABLED
    case RANDA_RC6:
      debugf ("using rc6 random generator");
      rc6_SetupKey (key, &rand_rc6);
      rand_rc6_array [0] = rand_rc6_array [1] =
        rand_rc6_array [2] = rand_rc6_array [3] = 0;
      rand_rc6_i = 3;

      rand_Get32p = rand_Get32_rc6;
      rand_Fillp = rand_Fill_rc6;
      break;
#endif
    case RANDA_ARCFOUR:
      debugf ("using arcfour random generator");
      arcfour_SetupKey (key, sizeof (key), &rand_arcfour);

      /* "give the crank those extra turns after initialisation"
       * suggestion of Jim Gillogly <jim@mentat.com>
       */
      {
        int i;
        for (i = 0; i<RAND_ARCFOUR_EXTRA; i++) arcfour_GetByte (&rand_arcfour);
      }
      rand_Get32p = rand_Get32_arcfour;
      rand_Fillp = rand_Fill_arcfour;
      break;
  }
}

/* vim:set sw=4:set ts=8: */
