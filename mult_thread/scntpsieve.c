// Copyright (C) 2009, Armin Biere, JKU, Linz, Austria.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>
#include <math.h>

static int verbose;
static double mods;

static void
die (const char * msg, ...)
{
  va_list ap;
  fputs ("*** scntpsieve: ", stderr);
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

double seconds () 
{
  struct rusage u;
  if (getrusage (RUSAGE_SELF, &u)) return 0;
  double res = u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec;
  res += u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
  return res;
}

static void
msg (const char * msg, ...)
{
  va_list ap;
  fprintf (stderr, "[scntpsieve-%07.2f] ", seconds ());
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

inline static int
sqrtbound (int n)
{
  int res, i = 0;
  assert (n < (1 << 30));
  while (i <= 15 && (1 << (2*i)) <= n)
    i++;
  res =  (1 << i);
  assert (res * res >= n);
  return res;
}

inline static int
isprime (int n)
{
  int b = sqrtbound (n), m;
  for (m = 2; m <= b && m < n; m++) 
  {
    if (verbose) mods++;
    if (!(n%m)) return 0;
  }
  if (verbose >= 2) msg ("prime %d", n);
  return 1;
}

int
main (int argc, char ** argv)
{
  int n = 0, m, t, res, i;
  char * sieve;

  for (i = 1; i < argc; i++) 
  {
    if (!strcmp (argv[i], "-h"))
    {
      printf ("usage: scntpsieve [-h] <number>\n");
      exit (0);
    } 
    else if (!strcmp (argv[i], "-v")) verbose++;
    else if (n) die ("multiple numbers specified");
    else if ((n = atoi (argv[i])) <= 0) die ("invalid number");
  }

  if (n <= 0) die ("no number specified");
  if (n >= (1 << 30)) die ("number too large");
  if (verbose) msg ("calculating number of primes below %d", n);

  sieve = calloc (n + 1, 1);
  res = 0;
  for (m = 2; m <= n; m++) {
    if (sieve[m]) continue;
    res++;
    for (t = 2 * m; t <= n; t += m)
      sieve[t] = 1;
  }

  free (sieve);
  printf ("%d\n", res);

  return 0;
}
