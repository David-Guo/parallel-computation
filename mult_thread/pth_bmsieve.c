// Copyright (C) 2009, Armin Biere, JKU, Linz, Austria.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>
#include <math.h>
#include <pthread.h>

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

inline static int get (unsigned * sieve, int m)
{
    return sieve[m/32] & (1 << (m & 31));
}

inline static void set (unsigned * sieve, int m)
{
    sieve[m/32] |= (1 << (m & 31));
}

int n = 0;
int num_thd = 0;
unsigned *sieve;

void *primecount(void * rank) {
    int threadid = (long) rank;
    int m, t;

    int step = (n - 2)/num_thd;
    int my_first_i = 2 + threadid * step;
    int my_last_i;
    if (threadid == num_thd - 1)
        my_last_i = my_first_i + step + (n - 2)%num_thd + 1;
    else 
        my_last_i = my_first_i + step;

    for (m = my_first_i; m < my_last_i; m++) 
        for (t = 2 * m; t <= n; t += m)
            set(sieve, t);
    return NULL;
}


int
main (int argc, char ** argv)
{
    int  i;
    int thread;
    int sum = 0;
    pthread_t* thread_handles;

    if (argc !=4 || strcmp (argv[1], "-p"))
    {
        printf ("usage: pscntpsieve [-p] <process> <number>\n");
        exit (0);
    } 
    if ((num_thd = atoi (argv[2])) <= 0) die ("invalid process number");
    if ((n = atoi (argv[3])) <= 0) die("invalid number");

    if (n >= (1 << 30)) die ("number too large");
  
    sieve = calloc (n/32 + 1, 4);
    thread_handles = (pthread_t *) malloc(num_thd * sizeof(pthread_t));
    clock_t start = clock();
    for (thread  = 0; thread < num_thd; thread++)
        pthread_create(&thread_handles[thread], NULL, primecount, (void *)thread);

    for (thread = 0; thread < num_thd; thread++)
        pthread_join(thread_handles[thread], NULL);

    for (i = 2; i <= n; i++)
        if(get(sieve, i))
            sum++;

    clock_t end = clock();
    printf ("run time: %f\n", (double)(end - start)/CLOCKS_PER_SEC);
    printf ("%d\n", n - sum - 1);
    
    free(thread_handles);
    free (sieve);
    return 0;
}
