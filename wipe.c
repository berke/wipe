/* wipe
 *
 * by Berke Durak
 *
 * With many thanks to the following contributors:
 *     Jason Axley
 *     Alexey Marinichev
 *     Chris L. Mason
 *     Karako Miklos
 *     Jim Paris
 *     Thomas Schoepf
 *
 * Author may be contacted at 'echo berke1lambda-diode|com 12 @.'
 *
 * URL for wipe:   http://lambda-diode.com/software/wipe
 * Git repository: http://github.com/berke/wipe
 *                 git clone https://github.com/berke/wipe.git
 *
 * Securely erase files from magnetic media
 * Based on data from article "Secure Deletion of Data from Magnetic
 * and Solid-State Memory" from Peter Gutmann.
 *
 */

/*** defines */

#define WIPE_VERSION "0.22"
#define WIPE_DATE "2010-11-07"
#define WIPE_CVS "$Id: wipe.c,v 1.2 2004/06/12 17:49:47 berke Exp $"

/* exit codes */

#define WIPE_EXIT_COMPLETE_SUCCESS 0
#define WIPE_EXIT_FAILURE 1
#define WIPE_EXIT_MINOR_ERROR 2
#define WIPE_EXIT_USER_ABORT 3
#define WIPE_EXIT_MANIPULATION_ERROR 4

/* NAME_MAX_TRIES is the maximum number of attempts made to find a free file
 * name in the current directory, by checking for random file names.
 */

#define NAME_MAX_TRIES 10

/* NAME_MAX_PASSES is the maximum number of times a filename is renamed.
 * The actual rename() calls made on a file might be less if during
 * the wiping process wipe fails to find a free file name after NAME_TRIES.
 */

#define NAME_MAX_PASSES 1

/* BUFSIZE determines the default buffer size */

#define BUFLG2 14
#define BUFSIZE (1<<BUFLG2)

/* 64M, 2^12 bytes blocksize : 57.66 s
 *      2^16                   55.39
 *      2^20                   60.08
 */

/* defines ***/

/*** includes */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef FIND_DEVICE_SIZE_BY_BLKGETSIZE
#include <linux/fs.h>
#ifdef memset
#undef memset
#endif
#endif

#ifdef HAVE_GETOPT
#include <getopt.h>
#endif
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "random.h"
#include "misc.h"
#include "version.h"

/* includes ***/

/*** more defines */

#define DEVRANDOM "/dev/urandom"
#define QUICKPASSES 4

/* Fix sent by Jason Axley:  NAME_MAX is not defined under Solaris 2.6 so we need to
 * define it to be something reasonable for Solaris and UFS.  I define
 * it here to be MAXNAMLEN, since that seems reasonable (much more
 * reasonable than the POSIX value of 14!) and its the same thing done
 * under linux glibc.  The quote from the Solaris /usr/include/limits.h
 * is included below for your edification:
 *
 * "POSIX 1003.1a, section 2.9.5, table 2-5 contains [NAME_MAX] and the
 * related text states:
 *
 * A definition of one of the values from Table 2-5 shall be omitted from the
 * <limits.h> on specific implementations where the corresponding value is
 * equal to or greater than the stated minimum, but where the value can vary
 * depending on the file to which it is applied. The actual value supported for
 * a specific pathname shall be provided by the pathconf() (5.7.1) function.
 * 
 * This is clear that any machine supporting multiple file system types
 * and/or a network can not include this define, regardless of protection
 * by the _POSIX_SOURCE and _POSIX_C_SOURCE flags.
 *
 * #define      NAME_MAX        14".
 */

#ifndef NAME_MAX
#define NAME_MAX MAXNAMLEN
#endif

/* more defines ***/

/*** errno, num_* statistics, middle_of_line */

extern int errno;

int num_errors = 0;
int num_files = 0;
int num_dirs = 0;
int num_spec = 0;
int num_symlinks = 0;

int middle_of_line = 0;

/* errno, num_* statistics, middle_of_line ***/

/*** options */

char *progname = 0;
int o_force = 0;
int o_dochmod = 0;
int o_errorabort = 0;
int o_recurse = 0;
int o_dereference_symlinks = 0;
int o_quick = 0;
int o_quick_passes = QUICKPASSES;
int o_quick_passes_set = 0;
int o_verbose = 0;
int o_silent = 0;
char *o_devrandom = DEVRANDOM;
int o_randseed = RANDS_DEVRANDOM;
int o_randalgo = RANDA_ARCFOUR;
int o_randseed_set = 0;
int o_no_remove = 0;
int o_dont_wipe_filenames = 0;
int o_name_max_tries = NAME_MAX_TRIES;
int o_name_max_passes = NAME_MAX_PASSES;
int o_dont_wipe_filesizes = 0;
off_t o_wipe_length;
off_t o_wipe_offset = 0;
int o_lg2_buffer_size = BUFLG2;
int o_buffer_size = 1<<BUFLG2;
int o_wipe_length_set = 0;
int o_wipe_exact_size = 0;
int o_skip_passes = 0;

/* End of Options ***/

static int wipe_filename_and_remove (char *fn);

/*** do_remove */

int do_remove (char *fn)
{
    if (!o_no_remove) {
        if (o_dont_wipe_filenames) return remove (fn);
        else return wipe_filename_and_remove (fn);
    } else return 0;
}

/* do_remove ***/

/*** signal_handler */

#ifdef SIGHANDLER_RETURNS_INT
int signal_handler (int i)
#else
void signal_handler (int i)
#endif
{
    fflush (stdout);
    fflush (stderr);
    if (middle_of_line) { fputc ('\n', stderr); middle_of_line = 0; }
    fprintf (stderr, "*** Interrupted by signal %d\n", i);
    /* exit () does fflush () */
    exit (i == SIGINT?WIPE_EXIT_USER_ABORT:WIPE_EXIT_FAILURE);
}

/* signal_handler ***/

/*** fill_random_from_table */

/* This function is used to create random filenames */

inline static void fill_random_from_table (char *b,
        int n, char *table, int mask)
{
    u32 l;

    for (; n >= 4; n -= 4) {
        l = rand_Get32 ();

        *(b++) = table[l & mask]; l >>=8;
        *(b++) = table[l & mask]; l >>=8;
        *(b++) = table[l & mask]; l >>=8;
        *(b++) = table[l & mask];
    }

    if (n) {
        l = rand_Get32 ();
        while (n--) {
            *(b++) = table[l & mask]; l >>= 8;
        }
    }
}

/* fill_random_from_table ***/

/*** fill_random */

/* fills buffer b with n (pseudo-)random bytes. */

inline static void fill_random (char *b, int n)
{
    rand_Fill ((u8 *) b, n);
}

/* fill_random ***/

/*** fill_pattern */

/* this could be seriously optimised */

void fill_pattern (char *b, int n, char *p, int l)
{
    int i;
    char *p1, *p2;

    for (p1 = p, p2 = p + l, i = 0; i<n; i++) {
        *(b++) = *(p1++);
        if (p1 == p2) p1 = p;
    }
}

/* fill_pattern ***/

/*** passinfo table */

/* P.Gutmann, in his article, was recommening to make
 * the DETERMINISTIC passes (i.e. those that don't write random data)
 * in random order, and to make 4 random passes before and after these. 
 *
 * Here, the deterministic and the 8 random passes are made in random
 * order, unless, of course, the "quick" mode is selected.
 */

struct passinfo_s {
    int type;
    int len;
    char *pat;
} passinfo[] = {
    { 0, 0, 0,		},	/* 1  random */
    { 0, 0, 0,		},	/* 2  random */
    { 0, 0, 0,		},	/* 3  random */
    { 0, 0, 0, 		},	/* 4  random */
    { 1, 1, "\x55", 	},	/* 5  RLL MFM */
    { 1, 1, "\xaa", 	},	/* 6  RLL MFM */
    { 1, 3, "\x92\x49\x24", },	/* 7  RLL MFM */
    { 1, 3, "\x49\x24\x92", },	/* 8  RLL MFM */
    { 1, 3, "\x24\x92\x49", },	/* 9  RLL MFM */
    { 1, 1, "\x00", 	},	/* 10 RLL */
    { 1, 1, "\x11", 	},	/* 11 RLL */
    { 1, 1, "\x22", 	},	/* 12 RLL */
    { 1, 1, "\x33", 	},	/* 13 RLL */
    { 1, 1, "\x44", 	},	/* 14 RLL */
    { 1, 1, "\x55", 	},	/* 15 RLL */
    { 1, 1, "\x66", 	},	/* 16 RLL */
    { 1, 1, "\x77", 	},	/* 17 RLL */
    { 1, 1, "\x88", 	},	/* 18 RLL */
    { 1, 1, "\x99", 	},	/* 19 RLL */
    { 1, 1, "\xaa", 	},	/* 20 RLL */
    { 1, 1, "\xbb", 	},	/* 21 RLL */
    { 1, 1, "\xcc", 	},	/* 22 RLL */
    { 1, 1, "\xdd", 	},	/* 23 RLL */
    { 1, 1, "\xee", 	},	/* 24 RLL */
    { 1, 1, "\xff", 	},	/* 25 RLL */
    { 1, 3, "\x92\x49\x24",	},	/* 26 RLL MFM */
    { 1, 3, "\x49\x24\x92",	},	/* 27 RLL MFM */
    { 1, 3, "\x24\x92\x49",	},	/* 28 RLL MFM */
    { 1, 3, "\x6d\xb6\xdb",	},	/* 29 RLL */
    { 1, 3, "\xb6\xdb\x6d",	},	/* 30 RLL */
    { 1, 3, "\xdb\x6d\xb6",	},	/* 31 RLL */
    { 0, 0, 0,		},	/* 32 random */
    { 0, 0, 0,		},	/* 33 random */
    { 0, 0, 0,		},	/* 34 random */
    { 0, 0, 0, 		},	/* 35 random */
};

#define MAX_PASSES (sizeof(passinfo)/(sizeof(*passinfo)))
#define BUFT_RANDOM (1<<0)
#define BUFT_USED (1<<1)

/* Passinfo table ***/

/*** pattern buffers and wipe info declarations */

struct wipe_pattern_buffer {
    int type;
    char *buffer;
    int pat_len;
};

#define MAX_BUFFERS 30
#define RANDOM_BUFFERS 16

struct wipe_info {
    int random_length;
    int n_passes;
    int n_buffers;
    struct wipe_pattern_buffer random_buffers[RANDOM_BUFFERS];
    struct wipe_pattern_buffer buffers[MAX_BUFFERS];
    struct wipe_pattern_buffer *passes[MAX_PASSES];
};

/* pattern buffers and wipe info declarations ***/

/*** shut_wipe_info */

void shut_wipe_info (struct wipe_info *wi)
{
    int i;

    for (i = 0; i<RANDOM_BUFFERS; free (wi->random_buffers[i++].buffer));
    for (i = 0; i<wi->n_buffers; free (wi->buffers[i++].buffer));
}

/* shut_wipe_info ***/

/*** dirty_all_buffers */

void dirty_all_buffers (struct wipe_info *wi)
{
    int i;

    for (i = 0; i<RANDOM_BUFFERS; wi->random_buffers[i++].type |= BUFT_USED);
}

/* dirty_all_buffers ***/

/*** revitalize_random_buffers */

int revitalize_random_buffers (struct wipe_info *wi)
{
    int i;

    for (i = 0; i<RANDOM_BUFFERS; i++) {
        if (wi->random_buffers[i].type & BUFT_USED) {
            fill_random (wi->random_buffers[i].buffer, wi->random_length);
            wi->random_buffers[i].type &= ~BUFT_USED;
            return 0;
        }
    }

    return -1; /* there were no buffers to revitalize */
}

/* revitalize_random_buffers ***/

/*** wipe_pattern_buffer */

struct wipe_pattern_buffer *get_random_buffer (struct wipe_info *wi)
{
    int i;

    for (i = 0; i<RANDOM_BUFFERS; i++) {
        if (!(wi->random_buffers[i].type & BUFT_USED)) {
            wi->random_buffers[i].type |= BUFT_USED;
            return &wi->random_buffers[i];
        }
    }

    /* no random buffers available, revitalize buffer 0 */
    fill_random (wi->random_buffers[0].buffer, wi->random_length);
    /* no need to set used flag */
    return &wi->random_buffers[0];
}

/* wipe_pattern_buffer ***/

/*** init_wipe_info */

void init_wipe_info (struct wipe_info *wi)
{
    int i, j;

    wi->n_passes = o_quick?o_quick_passes:MAX_PASSES;

    /* allocate buffers for random patterns */

    for (i = 0; i<RANDOM_BUFFERS; i ++) {
        wi->random_buffers[i].type = BUFT_RANDOM;
        wi->random_buffers[i].buffer = malloc (o_buffer_size);
        if (!wi->random_buffers[i].buffer) {
            fprintf (stderr, "could not allocate buffer [1]");
            exit (EXIT_FAILURE);
        }
    }

    /* allocate buffers for periodic patterns */

    if (!o_quick)
        for (wi->n_buffers = 0, i = 0; i<wi->n_passes; i++) {
            if (!passinfo[i].type) {
                wi->passes[i] = 0; /* random pass... */
            } else {
                /* look if this pattern has already been allocated */
                for (j = 0; j<wi->n_buffers; j++) {
                    if (wi->buffers[i].pat_len == passinfo[i].len &&
                            !memcmp (wi->buffers[i].buffer, passinfo[i].pat, passinfo[i].len))
                        break;
                }

                if (j >= wi->n_buffers) {
                    /* unfortunately we'll have to allocate a new buffers */
                    j = wi->n_buffers ++;
                    wi->buffers[j].type = 1; /* periodic */
                    wi->buffers[j].buffer = malloc (o_buffer_size);
                    if (!wi->buffers[j].buffer) {
                        fprintf (stderr, "could not allocate buffer [2]");
                        exit (EXIT_FAILURE);
                    }
                    wi->buffers[j].pat_len = passinfo[i].len;

                    /* fill in pattern */
                    fill_pattern (wi->buffers[j].buffer,
                            o_buffer_size, passinfo[i].pat, passinfo[i].len);
                }

                wi->passes[i] = &wi->buffers[j];
            }
        }
}

/* init_wipe_info ***/

#define fnerror(x)  { num_errors++; if(middle_of_line) fputc('\n', stderr); fprintf (stderr, "\r%.32s: " x ": %.32s\n", fn, strerror (errno)); }
#define fnerrorq(x) { num_errors++; if(middle_of_line) fputc('\n', stderr); fprintf (stderr, "\r%.32s: " x "\n", fn); }

#define FLUSH_MIDDLE if (middle_of_line) { fputc ('\n', stderr); \
    middle_of_line = 0; }

    /*** directory_name */

#if 0
inline static char *directory_name (char *fn)
{
    char *dn;
    char *sp;
    int dl;

    sp = strrchr (fn, '/');
    if (!sp) return strdup ("");
    dl = sp - fn + 1 + 1;
    dn = malloc (dl);
    if (!dn) return 0;
    memcpy (dn, fn, dl - 1);
    dn[dl] = 0;
    return dn;
}
#else
inline static int directory_name_length (char *fn)
{
    char *bn;

    bn = strrchr (fn, '/');
    if (!bn) return 0;
    else return bn - fn + 1;
}
#endif

/* directory_name ***/

static char valid_filename_chars[64] =
"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-.";

/*** wipe_filename_and_remove */

/* actually, after renaming a file, the only way to make sure that the
 * name change is physically carried out is to call sync (), which flushes
 * out ALL the disk caches of the system, whereas for
 * reading and writing one can use the O_SYNC bit to get syncrhonous
 * I/O for one file. as sync () is very slow, calling sync () after
 * every rename () makes wipe extremely slow.
 */

static int wipe_filename_and_remove (char *fn)
{
    int i, j, k, l;
    int r = -1;
    int fn_l, dn_l;
    /* char *dn; */
    char *buf[2];
    struct stat st;
    int t_l; /* target length */

    /* dn = directory_name (fn); */
    fn_l = strlen (fn);
    dn_l = directory_name_length (fn);

    buf[0] = malloc (fn_l + NAME_MAX + 1);
    buf[1] = malloc (fn_l + NAME_MAX + 1);

    r = 0;

    t_l = fn_l - dn_l; /* first target length */

    if (buf[0] && buf[1]) {
        strcpy (buf[0], fn);
        strcpy (buf[1], fn);
        for (j = 1, i = 0; i < o_name_max_passes;  j ^= 1, i++) {
            for (k = o_name_max_tries; k; k--) {
                l = t_l;
                fill_random_from_table (buf[j] + dn_l, l,
                        valid_filename_chars, 0x3f);
                buf[j][dn_l + l] = 0;
                if (stat (buf[j], &st)) break;
            }

            if (k) {
                if (!o_silent) {
                    fprintf (stderr, "\rRenaming %32.32s -> %32.32s", buf[j^1], buf[j]);
                    middle_of_line = 1;
                    fflush (stderr);
                }
                if (rename (buf[j^1], buf[j])) {
                    FLUSH_MIDDLE
                        fprintf (stderr, "%.32s: could not rename '%s' to '%s': %s (%d)\n",
                                fn, buf[j^1], buf[j], strerror (errno), errno);
                    r = -1;
                    break;
                }
                (void) sync ();
            } else {
                /* we could not find a target name of desired length, so
                 * increase target length until we find one. */
                t_l ++;
                j ^= 1;
            }
        }
        if (remove (buf[j^1])) r = -1;
    }
    free (buf[0]); free (buf[1]);
    return r;
}

/* wipe_filename_and_remove ***/

#include <sys/time.h>

static double
get_time_of_day(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (tv.tv_usec * 1e-6 + tv.tv_sec);
}

static double eta_start_time;

static void
eta_begin()
{
    eta_start_time = get_time_of_day();
}

static void
eta_progress(char *buf, size_t bufsiz, double frac)
{
    double now;
    double elapsed;
    double total;
    double seconds_per_year;
    double remaining;
    long lsec;
    int sec;
    long lmin;
    int min;
    long lhours;
    int hours;
    long ldays;
    int days;
    int weeks;
    
    now = get_time_of_day();
    elapsed = now - eta_start_time;

    buf[0] = '\0';
    if (elapsed < 5 && frac < 0.1)
        return;

    total = elapsed / frac;
    seconds_per_year = 365.25 * 24 * 60 * 60;
    remaining = total - elapsed;
    if (remaining > seconds_per_year)
    {
        snprintf(buf, bufsiz, "  ETA %g years", remaining / seconds_per_year);
        return;
    }
    lsec = remaining;
    sec = lsec % 60;
    lmin = lsec / 60;
    min = lmin % 60;
    lhours = lmin / 60;
    hours = lhours % 24;
    ldays = lhours / 24;
    days = ldays % 7;
    weeks = ldays / 7;
    if (weeks > 0)
    {
        snprintf(buf, bufsiz, "  ETA %dw %dd", weeks, days);
        return;
    }
    if (days > 0)
    {
        snprintf(buf, bufsiz, "  ETA %dd %dh", days, hours);
        return;
    }
    if (hours > 0)
    {
        snprintf(buf, bufsiz, "  ETA %dh%02dm", hours, min);
        return;
    }
    snprintf(buf, bufsiz, "  ETA %dm%02ds", min, sec);
}


static void
pad(char *buf, size_t bufsiz)
{
    size_t len;
    
    len = strlen(buf);
    while (len + 1 < bufsiz)
        buf[len++] = ' ';
    buf[len] = '\0';
}


static void
backspace(char *dst, const char *src)
{
    while (*src++)
        *dst++ = '\b';
    *dst = '\0';
}

/*** dothejob -- nonrecursive wiping of a single file or device */

/* determine parameters of region to be wiped
 *    file descriptor,
 *    offset,
 *    size.
 *
 *
 * compute the number of entire buffers needed to cover the region,
 * and the size of the remaining area.
 *
 * if pattern buffers have not been computed, compute them.
 *
 * compute one pattern buffer (either fill it or fetch it).
 * send an asynchronous write.
 *
 * select
 */

#define max(x,y) ((x>y)?x:y)

static int dothejob (char *fn)
{
    int fd;
    int p[MAX_PASSES];

    int bpi = 0;	/* block progress indicator enabled ? */
    int i;
    off_t j;

    static struct wipe_info wi;
    static int wipe_info_initialized = 0;
    struct wipe_pattern_buffer *wpb = 0;

    struct stat st;
    off_t buffers_to_wipe; /* number of buffers to write on device */
    int first_buffer_size;
    int last_buffer_size;
    int this_buffer_size;

    time_t lt = 0, t;

    fd_set w_fd;

    /* passing a null filename pointer means: free your internal buffers, please. */
    /* thanks to Thomas Schoepf and Alexey Marinichev for pointing this out */
    if (!fn) {
        if (wipe_info_initialized)
            shut_wipe_info (&wi);
        return 0;
    }

    /* to do a cryptographically strong random permutation on the 
     * order of the passes, we need lg_2(MAX_PASSES!) bits of entropy:
     * for MAX_PASSES=35, this means about 132 bits.
     *
     * rand_seed () should give 31 bits of entropy per call
     * we need a maximum of lg_2(MAX_PASSES) bits of entropy per random
     * value. therefore we should call rand_seed () at least for
     * every 31/lg_2(MAX_PASSES) ~= 6 random values.
     *
     * on the other way, as we'll constantly force disk access,
     * /dev/urandom should have no difficulty providing us good random
     * values.
     */

    if (!o_quick) {
        for (i = 0; i<MAX_PASSES; p[i]=i, i++);

        for (i = 0; i<MAX_PASSES-2; i++) {
            int a, b;

            /* a \in { 0, 1, ... MAX_PASSES-i-1 } */

            /* since MAX_PASSES-i is not necessarily a divisor of
             * 2^32, we won't get uniform distribution
             * with this. however, since MAX_PASSES is very
             * small compared to RAND_MAX, ti
             */

            a = rand_Get32 () % (MAX_PASSES-i);
            b = p[i+a];
            p[i+a] = p[i];
            p[i] = b;
        }
    }

    /* see what kind of file it is */

    if (o_dereference_symlinks ? stat (fn, &st) : lstat (fn, &st)) {
        fnerror("stat or lstat error");
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (!o_silent) fprintf (stderr, "Skipping directory %.32s...\n", fn);
        return 1;
    }

    /* checks if the file is a regular file, block device or character
     * device. used to refuse to wipe block/char devices. thanks
     * to Dan Hollis for pointing out this.
     */

    if (S_ISREG(st.st_mode) || S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
#ifdef HAVE_OSYNC
        fd = open (fn, O_WRONLY | O_SYNC | O_NONBLOCK);
#else
        fd = open (fn, O_WRONLY | O_NONBLOCK);
#endif

        if (fd < 0) {
            if (errno == EACCES) {
                if (o_force || o_dochmod) {
                    if (chmod (fn, 0700)) {
                        fnerror("chmod error");
                        return -1;
                    } else fd = open (fn, O_WRONLY);
                } else { fnerror("permission error: try -c"); return -1; }
            } else { fnerror("open error"); return -1; }
        }
        if (fd < 0) { fnerror("open error even with chmod"); return -1; }

        if (!o_wipe_length_set) {
            if (S_ISBLK(st.st_mode)) {
#ifdef FIND_DEVICE_SIZE_BY_BLKGETSIZE
                /* get device size in blocks */
                {
#ifndef SIXTYFOUR
                    long l;

                    if (ioctl (fd, BLKGETSIZE, &l)) {
                        fnerror ("could not get device block size via ioctl BLKGETSIZE; check option -l");
                        return -1;
                    }
                    o_wipe_length = l << 9; /* assume 512-byte blocks */
#else
                    long long l;

                    if (ioctl (fd, BLKGETSIZE64, &l)) {
                        fnerror ("could not get device block size via ioctl BLKGETSIZE64; check option -l");
                        return -1;
                    }
		    o_wipe_length = l; /* BLKGETSIZE64 returns bytes */
#endif

                }
#else
                {
#ifdef SIXTYFOUR
                    long long l;
                    /* find device size by seeking... might work on some devices */
                    l = lseek64 (fd, 0, SEEK_END);
                    if (!l) {
                        fnerror ("could not get device block size with lseek; check option -l");
                    }
                    debugf ("lseek -> %d", l);
                    o_wipe_length = l;
#else
                    off_t l;
                    /* find device size by seeking... might work on some devices */
                    l = lseek (fd, 0, SEEK_END);
                    if (!l) {
                        fnerror ("could not get device block size with lseek; check option -l");
                    }
                    debugf ("lseek -> %d", l);
                    o_wipe_length = l;
#endif
                }
#endif
                debugf ("block device block size %d", st.st_blksize);
            } else {
                /* not a block device */
                o_wipe_length = st.st_size;
                if (!o_wipe_exact_size) {
                    o_wipe_length += st.st_blksize - (o_wipe_length % st.st_blksize);
                }
            }
            o_wipe_length -= o_wipe_offset;
        }

        /* don't do anything to zero-sized files */
        if (!o_wipe_length) {
            goto skip_wipe;
        }

#if SIXTYFOUR
        debugf ("o_wipe_length = %Ld", o_wipe_length);
#else
        debugf ("o_wipe_length = %ld", o_wipe_length);
#endif

        /* compute number of writes... */
        {
            int fb, lb;

            fb = o_wipe_offset >> o_lg2_buffer_size;
            lb = (o_wipe_offset + o_wipe_length + o_buffer_size - 1) >> o_lg2_buffer_size;
            buffers_to_wipe = lb - fb;

            debugf ("fb = %d lb = %d", fb, lb);

            if (buffers_to_wipe == 1) {
                last_buffer_size = first_buffer_size = o_wipe_length;
            } else {
                first_buffer_size = o_buffer_size - (o_wipe_offset & (o_buffer_size - 1));
                last_buffer_size = (o_wipe_offset + o_wipe_length) & (o_buffer_size - 1);
                if (!last_buffer_size) last_buffer_size = o_buffer_size;
            }
        }

        debugf ("buffers_to_wipe = %d first_buffer_size = %d last_buffer_size = %d",
                buffers_to_wipe, first_buffer_size, last_buffer_size);

        /* initialize wipe info */
        if (!wipe_info_initialized) {
            init_wipe_info (&wi);
            wipe_info_initialized = 1;
        }

        /* if the possibly existing leftover random buffers
         * from the last wipe are shorter than what we need for
         * now, mark all of them as used.
         *
         * ok it's possible to have a smarter random buffer management,
         * but it only helps on small files (i.e. files smaller than the
         * buffer size) so it's not really worth it (in other words:
         * you can't escape from linear asymptotic complexity:)
         */

        {
            int x;

            if (buffers_to_wipe <= 2)
                x = max (first_buffer_size, last_buffer_size);
            else
                x = o_buffer_size;

            if (x > wi.random_length)
                dirty_all_buffers (&wi);

            wi.random_length = x;
        }

        debugf ("buffers_to_wipe = %d, o_buffer_size = %d, wi.n_passes = %d",
                buffers_to_wipe, o_buffer_size, wi.n_passes);

	if (o_skip_passes > 0) {
	    printf ("\rSkip first %d pass(es)\n", o_skip_passes);
	}

        /* do the passes */
        eta_begin();
        for (i = o_skip_passes; i<wi.n_passes; i++) {
            ssize_t wr;

            if (!o_silent) {
                if (o_quick) 
                    fprintf (stderr, "\rWiping %.32s, pass %d in quick mode   ", fn, i);
                else
                    fprintf (stderr, "\rWiping %.32s, pass %-2d (%-2d)   ", fn, i, p[i]);
                middle_of_line = 1;
            }

            lseek (fd, o_wipe_offset, SEEK_SET);

            if (!o_silent) lt = time (0);

            for (j = 0; j<buffers_to_wipe; j ++) {
                if (!j) this_buffer_size = first_buffer_size;
                else if (j + 1 == buffers_to_wipe) this_buffer_size = last_buffer_size;
                else this_buffer_size = o_buffer_size;

                if (!o_silent) {
                    t = time (0);
                    if ((bpi && (t-lt)) || ((t-lt>2) && j<(buffers_to_wipe>>1))) {
                        char buf1[30];
                        char buf1_bs[sizeof (buf1)];
                        char buf2[18];
                        char buf2_bs[sizeof(buf2)];
                        snprintf(buf1, sizeof(buf1),
                                "[%8ld / %8ld]", (long) j, (long)buffers_to_wipe);
                        backspace(buf1_bs, buf1);
                        eta_progress(buf2, sizeof(buf2),
                            ((double)i + ((double)j / buffers_to_wipe)) / wi.n_passes);
                        if (buf2[0])
                            pad(buf2, sizeof(buf2));
                        backspace(buf2_bs, buf2);
                        fprintf(stderr, "%s%s%s%s", buf1, buf2, buf2_bs,
                            buf1_bs);
                        fflush (stderr);
                        lt = t;
                        bpi = 1;
                    }
                }

                /* get a fresh random buffer */
                {
                    if (o_quick || !wi.passes[p[i]]) {
                        wpb = get_random_buffer (&wi);
                    } else {
                        wpb = wi.passes[p[i]];
                    }

                    for (;;) {
                        wr = write (fd, wpb->buffer,
                                this_buffer_size); /* asynchronous write */

                        if (wr < 0) {
                            if (errno == EAGAIN) {
                                /* since we are idle we can do some socially useful
                                 * work.
                                 */
                                if (revitalize_random_buffers (&wi)) {
                                    /* there isn't anything we can do to occupy ourselves,
                                     * so we'll just wait until our previous write requests
                                     * are queued, and retry.
                                     */
                                    FD_ZERO (&w_fd);
                                    FD_SET (fd, &w_fd);
                                    if (select (fd + 1, 0, &w_fd, 0, 0) < 0) {
                                        fnerror ("select");
                                        close (fd);
                                        return -1;
                                    }

                                    /* we MUST have FD_ISSET(&w_fd) */
                                }
                            } else {
                                fnerror ("write error");
                                exit (EXIT_FAILURE);
                            }
                        } else if (wr != this_buffer_size) {
                            /* argh short write... what does this mean exactly with
                             * non-blocking i/o ? */
                            debugf ("short write, expecting %d got %d",
                                    this_buffer_size, wr);
                            fnerror ("short write");
                            close (fd);
                            return -1;
                        } else break;
                    }
                }

#ifndef HAVE_OSYNC
                if (fsync (fd)) {
                    fnerror ("fsync error [1]");
                    close (fd);
                    return -1;
                }
#endif
            }

            if (fsync (fd)) {
                fnerror ("fsync error [2]");
                close (fd);
                return -1;
            }
        }

        /* try to wipe out file size by truncating at various sizes... */

skip_wipe:    
        if (S_ISREG(st.st_mode) && !o_dont_wipe_filesizes) {
            off_t s;
            u32 x;

            s = st.st_size;
            x = rand_Get32 ();

            while (s) {
                s >>= 1;
                x >>= 1;
                if (x & 1) {
                    if (ftruncate (fd, s)) {
                        fnerror ("truncate");
                        close (fd);
                        return -1;
                    }
                }
            }
        }

        close (fd);
    }

    /* if this is a symbolic link, then remove the target of the link */
    /* of course, we have a race condition here. */
    /* no user should be able to control a link you're wiping... */
    if (o_dereference_symlinks) {
        struct stat st2;

        if (lstat (fn, &st2)) {
            fnerror("lstat error");
            return -1;
        }
        if (S_ISLNK(st2.st_mode)) {
            int m;
            char buf[NAME_MAX+1];

            num_symlinks ++;
            m = readlink (fn, buf, NAME_MAX);
            if (m < 0) {
                fnerror ("readlink");
                return -1;
            }
            buf[m] = 0;
            if (do_remove (buf)) { fnerror("remove"); return -1; }
        }
    }

    /* remove link or file */
    if (do_remove (fn)) { fnerror("remove"); return -1; }

    if (!o_silent) {
        fprintf (stderr, "\r                                                                              \r");
        middle_of_line = 0;
    }

    if (S_ISLNK(st.st_mode)) {
        num_symlinks ++;
        if (o_verbose) {
            printf ("Not following symbolic link %.32s\n", fn);
            middle_of_line = 0;
        }
    } else {
        num_files ++;
        if (o_verbose) {
            printf ("File %.32s (%ld bytes) wiped\n", fn, (long) st.st_size);
            middle_of_line = 0;
        }
    }
    return 0;
}

int recursive (char *fn)
{
    int r = 0;
    struct stat st;
    char *olddir;

    if (!strcmp(fn,".") || !strcmp(fn,"..")) {
        printf("Will not remove %s\n", fn);
        return 0;
    }

    if (!strcmp(fn,".") || !strcmp(fn,"..")) {
        printf("Will not remove %s\n", fn);
        return 0;
    }

    if (lstat (fn, &st)) { fnerror ("stat error"); return -1; }

    if (S_ISDIR(st.st_mode)) {
        DIR *d;
        struct dirent *de;

        olddir = getcwd (0, 4096);
        if (!olddir) { fnerror("getcwd"); return -1; }

        if (o_verbose) {
            printf ("Entering directory '%s'\n", fn);
            middle_of_line = 0;
        }

        /* fix on 14.02.1998 -- check r/w/x permissions on directory, fix if necessary */
        if ((st.st_mode & 0700) != 0700) {
            if (o_dochmod) {
                if (o_verbose) {
                    printf ("Changing permissions from %04o to %04o\n",
                            (int) st.st_mode, (int) st.st_mode | 0700);
                    middle_of_line = 0;
                }
                if (chmod (fn, st.st_mode | 0700)) {
                    fnerror ("chmod [1]");
                    return -1;
                }
            } else {
                fnerrorq ("directory mode is not u+rwx; use -c option");
                return -1;
            }
        }

        d = opendir (fn);
        if (!d) {
            if (errno == EACCES && o_dochmod) {
                if (chmod (fn, st.st_mode | 0700)) {
                    fnerror("chmod [2]"); return -1;
                } else d = opendir (fn);
            } else { fnerror("opendir"); return -1; }
        }
        if (!d) { fnerror("opendir after chmod"); return -1; }

        if (chdir (fn)) { fnerror("chdir"); return -1; }

        errno = 0;
        num_dirs ++;

        while ((de = readdir (d))) {
            if (strcmp (de->d_name, ".") && strcmp (de->d_name, "..")) {
                if (recursive (de->d_name)) {
                    /* num_errors ++; */
                    r = -1;
                    if (o_errorabort) break;
                }
            }
            errno = 0;
        }

        if (errno) { fnerror("readdir"); return -1; }
        closedir (d);
        if (o_verbose) {
            printf ("Going back to directory %s\n", olddir);
            middle_of_line = 0;
        }
        if (chdir (olddir)) { fnerror("chdir .."); return -1; }
        free (olddir);
        if (!r && !o_no_remove && rmdir (fn)) { fnerror ("rmdir"); return -1; }	
    } else {
        if (S_ISREG(st.st_mode)) {
            return dothejob (fn);
        } else if (S_ISLNK(st.st_mode)) { num_symlinks ++; }
        else { 
            if (o_verbose) {
                printf ("Not wiping special file %s in recursive mode\n",
                        fn);
                middle_of_line = 0;
            }
            num_spec ++;
        }
        if (do_remove (fn)) { fnerror ("remove"); return -1; }	
    }

    return r;
}

/* dothejob ***/

/*** banner */

void banner ()
{
    fprintf (stderr, "This is wipe version " WIPE_VERSION ".\n"
            "\n"
            "Author:                  Berke Durak.\n"
            "Author's e-mail address: echo berke1lambda-diode2com|tr 12 @.\n"
            "Web site:                http://lambda-diode.com/software/wipe/\n"
            "Release date:            " WIPE_DATE "\n"
            "Compiled:                " __DATE__ "\n"
            "Git version:             " WIPE_GIT "\n"
            "\n"
            "Based on data from \"Secure Deletion of Data from Magnetic and Solid-State\n"
            "Memory\" by Peter Gutmann <pgut001@cs.auckland.ac.nz>.\n");
}

/* banner ***/

#define OPTSTR "X:DfhvrqspciR:S:M:kFZl:o:b:Q:T:P:e"

/*** reject and usage */

void reject (char *msg)
{
    fprintf (stderr, "Invocation error (-h for help): %s\n", msg);
    exit(WIPE_EXIT_MANIPULATION_ERROR);
}

void usage (void)
{
    fprintf (stderr,
            "Usage: %s [options] files...\n"
            "Options:\n"
            "\t\t-a Abort on error\n"
            "\t\t-b <buffer-size-lg2> Set the size of the individual i/o buffers\n"
            "\t\t\tby specifying its logarithm in base 2. Up to 30 of these\n"
            "\t\t\tbuffers might be allocated\n"
            "\t\t-c Do a chmod() on write-protected files\n"
            "\t\t-D Dereference symlinks (conflicts with -r)\n"
            "\t\t-e Use exact file size: do not round up file size to wipe\n"
            "\t\t\tpossible junk remaining on the last block\n"
            "\t\t-f Force, i.e. don't ask for confirmation\n"
            "\t\t-F Do not attempt to wipe filenames\n"
            "\t\t-h Display this help\n"
            "\t\t-i Informative (verbose) mode\n"
            "\t\t-k Keep files, i.e. do not remove() them after overwriting\n"
            "\t\t-l <length> Set wipe length to <length> bytes, where <length> is\n"
            "\t\t\tan integer followed by K (Kilo:1024), M (Mega:K^2) or\n"
            "\t\t\tG (Giga:K^3)\n"
            "\t\t-M (l|r) Set PRNG algorithm for filling blocks (and ordering passes)\n"
            "\t\t\tl Use libc's "
#ifdef HAVE_RANDOM
        "random()"
#else
        "rand()"
#endif
        " library call\n"
#ifdef RC6_ENABLED
        "\t\t\tr Use RC6 encryption algorithm\n"
#endif
        "\t\t\ta Use arcfour encryption algorithm\n"
        "\t\t-o <offset> Set wipe offset to <offset>, where <offset> has the\n"
        "\t\t\tsame format as <length>\n"
        "\t\t-P <passes> Set number of passes for filename wiping.\n"
        "\t\t\tDefault is 1.\n"
        "\t\t-Q <number> set number of passes for quick wipe\n"
        "\t\t-q Quick wipe, less secure, 4 random passes by default\n"
        "\t\t-r Recurse into directories -- symlinks will not be followed\n"
        "\t\t-R Set random device (or random seed command with -S c)\n"
        "\t\t-S (r|c|p) Random seed method\n"
        "\t\t\t r Read from random device (strong)\n"
        "\t\t\t c Read from output of random seed command\n"
        "\t\t\t p Use pid(), clock() etc. (weakest)\n"
        "\t\t-s Silent mode -- suppresses all output\n"
        "\t\t-T <tries> Set maximum number of tries for free\n"
        "\t\t\tfilename search; default is 10\n"
        "\t\t-v Show version information\n"
        "\t\t-Z Do not attempt to wipe file size\n"
        "\t\t-X <number> Skip this number of passes (useful for continuing a wiping operation)\n",
    progname
        );

    exit (WIPE_EXIT_COMPLETE_SUCCESS);
}

/* reject and usage ***/

/*** parse_length_offset_description */

int parse_length_offset_description (char *d, off_t *v)
{
    off_t s = 0, t = 0;
    char c;

    for (;;) {
        c = *d;
        switch (c) {
            case 0:
                *v = s + t;
                debugf ("parse length description: *v = %ld", *v);
                return 0;
            case '0' ... '9':
                s *= 10;
                s += c & 0x0f;
                break;
            case 'K':
                t += s << 10;
                s = 0;
                break;
            case 'M':
                t += s << 20;
                s = 0;
                break;
            case 'G':
                t += s << 30;
                s = 0;
                break;
            case 'b': /* b for block */
                t += s << 9;
                s = 0;
                break;
            default:
                fprintf (stderr, "error: character '%c' is illegal in length description \"%s\"\n",
                        c, d);
                return -1;
        }
        d ++;
    }
}

/* parse_length_offset_description ***/

/*** main */

int main (int argc, char **argv)
{
    int i;
    int n, ndir, nreg, nlnk;
    int c;
    struct stat st;

    /* basic setup */

    progname = strrchr (*argv, '/');
    if (progname) progname ++;
    else progname = *argv;

    signal (SIGINT,  signal_handler);
    signal (SIGTERM, signal_handler);

    /* parse options */

    for (;;) {
        c = getopt (argc, argv, OPTSTR);
        if (c<0) break;

        switch (c) {
	    case 'X': o_skip_passes = atoi(optarg);
	        if (o_skip_passes <= 0) {
                    reject ("number of skipped passes must be strictly positive");
                }
                break;
            case 'c': o_dochmod = 1; break;
            case 'D': o_dereference_symlinks = 1; break;
            case 'e': o_wipe_exact_size = 1; break;
            case 'F': o_dont_wipe_filenames = 1; break;
            case 'f': o_force = 1; break;
            case 'i': o_verbose = 1; break;
            case 'k': o_no_remove = 1; break;
            case 'P': o_name_max_passes = atoi (optarg); break;
            case 'q': o_quick = 1; break;
            case 'R': o_devrandom = optarg; o_randseed_set = 1; break;
            case 'r': o_recurse = 1; break;
            case 's': o_silent = 1; break;
            case 'T': o_name_max_tries = atoi (optarg); break;
            case 'v': banner (); exit (0);
            case 'Z': o_dont_wipe_filesizes = 1; break;
            case 'b':
                      o_lg2_buffer_size = atoi (optarg);
                      if (o_lg2_buffer_size < 9)
                          reject ("the power of 2 specified must be greater than 8 "
                                  "to ensure a buffer size of at least 512 bytes");
                      else if (o_lg2_buffer_size > 30) {
                          reject ("A power of two over 30 is probably useless "
                                  "since it means a buffer over one gigabyte");
                      }
                      o_buffer_size = 1<<o_lg2_buffer_size;
                      debugf ("buffer_size = %ld", o_buffer_size);
                      break;
            case 'o':
                      if (parse_length_offset_description (optarg,
                                  &o_wipe_offset)) exit (EXIT_FAILURE);
                                  debugf ("wipe offset set to %ld", o_wipe_offset);
                                  break;
            case 'l':
                                  {
                                      off_t o;

                                      if (parse_length_offset_description (optarg,
                                                  &o)) exit (EXIT_FAILURE);
                                                  o_wipe_length = o;
                                                  o_wipe_length_set = 1;
                                                  debugf ("wipe length set to %ld", o_wipe_length);
                                  }
                                  break;
            case 'Q':   o_quick_passes = atoi (optarg);
                        o_quick_passes_set = 1;
                        break;
            case 'S':
                        if (optarg[1]) 
                            reject ("'random seed method' must be single char");
                        switch (optarg[0]) {
                            case 'r':
                                o_randseed = RANDS_DEVRANDOM; break;
                            case 'c':
                                o_randseed = RANDS_PIPE; break;
                            case 'p':
                                o_randseed = RANDS_PID; break;
                            default:
                                reject ("unknown random seed method, must be r,c or p");
                                break;
                        }
                        o_randseed_set = 1;
                        break;
            case 'M':
                        if (optarg[1])
                            reject ("'PRNG algorithm' must be single char");
                        switch (optarg[0]) {
                            case 'l':
                                o_randalgo = RANDA_LIBC; break;
#ifdef RC6_ENABLED
                            case 'r':
                                o_randalgo = RANDA_RC6; break;
#endif
                            case 'a':
                                o_randalgo = RANDA_ARCFOUR; break;
                            default:
                                reject ("unknown random seed method, see help");
                                break;
                        }
                        break;
            case 'h':
            case '?':
            default:
                        usage ();
                        /* not reached */
        }
    }

    if (o_quick_passes_set && !o_quick) {
        reject ("option -Q useless without -q");
    }

    if (optind >= argc) reject ("wrong number of arguments");

    if (o_recurse && o_dereference_symlinks) reject ("options -D and -r are mutually exclusive");

    /* automatic detection of a suitable random device */

    if (!o_randseed_set) {
        char *t;

        t = getenv ("WIPE_SEEDPIPE");

        if (t || access (o_devrandom, R_OK)) {
            o_devrandom = t;
            if (access (o_devrandom, X_OK))
                o_randseed = RANDS_PID;
            else o_randseed = RANDS_PIPE;
        }
    }

    /* initialise PRNG */
    rand_Init ();

    /* stat specified files/directories */

    n = argc-optind;
    ndir = nreg = nlnk = 0;

    for (i = optind; i<argc; i++) {
        if (lstat (argv[i], &st)) {
            fprintf (stderr, "%s: fatal: could not lstat: %s\n",
                    argv[i],
                    strerror (errno));
            exit (EXIT_FAILURE);
        }
        if (S_ISLNK(st.st_mode)) nlnk ++;
        else if (S_ISDIR (st.st_mode)) ndir ++;
        else if (S_ISREG (st.st_mode)) nreg ++;
    }

    if (!o_recurse && ndir) {
        if (!o_silent && (n - ndir)) fprintf (stderr, "Warning - will skip %d director%s\n", ndir, ndir>1?"ies":"y");
        else {
            fprintf (stderr, "Use -r option to wipe directories\n");
            exit (EXIT_FAILURE);
        }
    } 

    if (!o_force) {	
        char buf2[80];
        char buf[80];
        char *b = buf;

        if (nreg) {
            sprintf (b, "%d regular file%s", nreg, (nreg>1)?"s":"");
            b += strlen (b);
        }

        if (ndir && o_recurse) {
            sprintf (b, "%s%d director%s",
                    (b != buf)?((n-nreg-ndir)?", ":" and "):"",
                    ndir, ndir!=1?"ies":"y");
            b += strlen (b);
        }

        if (nlnk) {
            sprintf (b, "%s%d symlink%s%s:",
                    (b != buf)?" and ":"",
                    nlnk, nlnk!=1?"s":"",
                    (o_dereference_symlinks?
                     (nlnk!=1?" and their targets":" and its target"):
                     (nlnk!=1?" (without following the links)":" (without following the link)")));
            b += strlen(b);
        }
        if (n-nreg-ndir-nlnk) {
            sprintf (b, "%s%d special file%s",
                    (b != buf)?" and ":"",
                    n-nreg-ndir,
                    (n-nreg-ndir-nlnk)!=1?"s":"");
        }

        /* if stderr is a tty, ask for confirmation */

#define CONFIRMATION_MAX_RETRIES 5

        if (isatty (fileno (stderr))) {
            for (i = 0; i<CONFIRMATION_MAX_RETRIES; i++) {
                /* OK there might be some some atomicity problems here... */
                fprintf (stderr, "Okay to WIPE %s ? (Yes/No) ", buf);
                middle_of_line = 1;
                fflush (stderr);
                if (!fgets (buf2, sizeof (buf2), stdin)) goto user_aborted;
                if (buf2[0]) buf2[strlen (buf2)-1] = 0;
                middle_of_line = 0;

#ifdef HAVE_STRCASECMP
                if (!strcasecmp (buf2, "no"))
#else
                    if ('n' == tolower (buf2[0]) &&
                            'o' == tolower (buf2[1]) &&
                            !tolower (buf2[2]))
#endif
                    {
user_aborted:
                        if (!o_silent) {
                            FLUSH_MIDDLE;
                            fprintf (stderr, "Aborted.\n");
                        }
                        exit (WIPE_EXIT_USER_ABORT);
                    }

#ifdef HAVE_STRCASECMP
                if (strcasecmp (buf2, "yes"))
#else
                    if ('y' != tolower (buf2[0]) ||
                            'e' != tolower (buf2[1]) ||
                            's' != tolower (buf2[2]) ||
                            buf2[3])
#endif
                    {
                        fprintf (stderr, "Please answer \"Yes\" or \"No\".\n");
                    } else break;
            }
            if (i == CONFIRMATION_MAX_RETRIES) {
                fprintf (stderr, "User refused to answer correctly "
                        "for %d queries, aborting.\n",
                        CONFIRMATION_MAX_RETRIES);
                exit (WIPE_EXIT_MANIPULATION_ERROR);
            }
        } else {
            if (!o_silent) fprintf (stderr,
                    "Please use -f option in "
                    "non-interactive mode.\n");
            exit (WIPE_EXIT_MANIPULATION_ERROR);
        }
    }

    for (i = optind; i<argc; i++) {
        int r;

        if (o_recurse) r = recursive (argv[i]);
        else r = dothejob (argv[i]);


        /* if (r < 0) num_errors ++; */
    }

    /* free internal buffers */
    dothejob (0);

    /* final synchronisation */
    if (!o_silent) fprintf (stderr, "Syncing..."); fflush (stderr);
#ifdef SYNC_WAITS_FOR_SYNC
    sync ();
#else
    sync (); sleep (1); sync ();
#endif
    if (!o_silent) {
        if(o_dereference_symlinks) {
            fprintf (stderr, "\rOperation finished.\n"
                    "%d file%s (of which %d special) in %d director%s wiped, "
                    "%d symlink%s removed and their targets wiped, "
                    "%d error%s occured.\n",
                    num_files+num_spec, (1==num_files+num_spec)?"":"s",
                    num_spec,
                    num_dirs, (1==num_dirs)?"y":"ies",
                    num_symlinks, (1==num_symlinks)?"":"s",
                    num_errors, (1==num_errors)?"":"s");
        } else {
            fprintf (stderr, "\rOperation finished.\n"
                    "%d file%s wiped and %d special file%s ignored in %d director%s, "
                    "%d symlink%s removed but not followed, "
                    "%d error%s occured.\n",
                    num_files,(1==num_files)?"":"s",
                    num_spec,(1==num_spec)?"":"s",
                    num_dirs, (1==num_dirs)?"y":"ies",
                    num_symlinks, (1==num_symlinks)?"":"s",
                    num_errors, (1==num_errors)?"":"s");
        }
    }

    return num_errors?WIPE_EXIT_FAILURE:WIPE_EXIT_COMPLETE_SUCCESS;
}
/* main ***/

/* vim:set sw=4:set ts=8: */
