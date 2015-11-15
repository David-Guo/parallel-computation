/* Sequential failed literal dimacs preprocessor using counting.
 * Copyright (C) 2009 by Armin Biere, FMV, JKU, Linz, Austria.
 *
 * VERSION 2
 *
 * This is a preprocessor for SAT problems. It reads in a SAT instance
 * in DIMACS format and writes a simplified formula in DIMACS, with all
 * the failed and implied literal added as unit clauses.
 *
 * Failed literal preprocessing consists of assuming literals in turn,
 * propagating them and in case a conflict occurs, flipping the value of the
 * assumption and making the assignment permanent.  This is continued until
 * nor more failed literals are found or a top level conflict is generated.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/resource.h>
#include <omp.h>

/* By setting this to '1' you can enable 'logging'.
*/
#if 0
#define LOG(code) do { code; } while (0)
#else
#define LOG(code) do { } while (0)
#endif

/* The variables on the next for lines are for parsing only.
*/
static int line;
static FILE * input, * output;
static const char * input_name, * output_name;
static int * stack, * top_of_stack, * end_of_stack;

static int inconsistent;/* found empty clause */
static int m;		/* number of variables 'm' in 'p cnf m n' */
static int n;		/* number of clauses 'n' in 'p cnf m n' */
static int ** clauses;	/* 'n' clauses (0..n-1) zero terminated */

/* Variables are signed integers in the range '1..m'.
*/
static int * assignment;/* for 'var=1..m' <0 false, >0 true, =0 unassigned */

/* Signed variables are literals.  Negative values denote negated variables.
 * The literals are in the range '-m..m' where '0' is of course excluded.
 * Data associated with literals can thus be stored as an array which is
 * also indexed by negative values.
 */
static int * counts;	/* for 'lit=-m...m' number occurrences */
static int ** lit2occs;	/* for 'lit=-m..m' clause indices -1 terminated */
static int * occs;	/* one memory block for all clauses indices */

/* The next two lines contain statistics.
*/
static int units, rounds, decisions;
static long long propagations;

/*---------------------------------------------------------*\
 * NOTE: in the threaded version each worker thread should *
 * probably have a copy of the following static variables. *
 *---------------------------------------------------------*/

/* This counts for the i'th clause 'clauses[i]' the number of literals that
 * are not assigned to false.  If this counter becomes zero a conflict is
 * found.  If this conflict occurs on the top level, e.g. if there has not
 * been a previous decision and therefore 'decision == 0', then turns out to
 * be inconsisten, e.g. unsatisfiable.  If the counter becomes 1, then
 * the remaining literal is assigned to true.
 */
static int * nonfalse;

/* The trail is a stack that contains all the literals assigned to true:
 *
 *                   decision   next_to_propagate
 *                     v           v
 *       +---+---+---+---+---+---+---+---+---+---+---+...+
 *       |-3 | 1 | 2 |-5 |-7 |-9 | 8 | 6 |-4 |   |   |   :
 *       +---+---+---+---+---+---+---+---+---+---+---+...+
 *         ^                                   ^       ^
 *        trail                       top_of_trail  trail+m
 *
 * The trail is preallocated, since it is bounded by the number of
 * variables.  If a variable is assigned and the corresponding literal
 * pushed onto the trail, the 'nonfalse' counters need to be updated, which
 * is done during propagation of this literal.  Since propagation may
 * produce additional assignments, one needs to decide how to order
 * propagations.  In this implementation we do a BFS over the assigned
 * variables during propagation and for this purpose we use the trail also
 * as queue for this BFS.  The head of the queue is 'next_to_propagate', the
 * tail 'top_of_trail'.  If a literal is assigned on the top level without
 * assuming anything, for instance if the input contains a unit clause or a
 * if failed literal is produced, then this literal is still pushed on the
 * trail and also needs to be propagated.  If this propagation terminates
 * without conflict and an assumption/decision is made, the start of those
 * variables that need to be unassigned during backtracking is saved in the
 * 'decision' pointer, which if no assumption is made.
 */
static int * trail, * next_to_propagate, *top_of_trail, * decision;

#define NEW(p,n) do { (p) = calloc ((n), sizeof (*(p))); } while (0)

    static void
die (const char * msg, ...)
{
    va_list ap;
    fputs ("*** sflprepc: ", stderr);
    va_start (ap, msg);
    vfprintf (stderr, msg, ap);
    va_end (ap);
    fputc ('\n', stderr);
    exit (1);
}

    static double
seconds () 
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
    fprintf (stderr, "[sflprepc-%07.2f] ", seconds ());
    va_start (ap, msg);
    vfprintf (stderr, msg, ap);
    va_end (ap);
    fputc ('\n', stderr);
    fflush (stderr);
}

    static int
nextch (void)
{
    int ch = getc (input);
    if (ch == '\n')
        line++;
    return ch;
}

    static void
perr (const char * msg, ...)
{
    va_list ap;
    fprintf (stderr, "%s:%d: ", input_name, line);
    va_start (ap, msg);
    vfprintf (stderr, msg, ap);
    va_end (ap);
    fputc ('\n', stderr);
    exit (1);
}

    static void
enlarge (void)
{
    int old_size = end_of_stack - stack;
    int new_size = old_size ? 2 * old_size : 1;
    int count = top_of_stack - stack;
    stack = realloc (stack, new_size * sizeof *stack);
    end_of_stack = stack + new_size;
    top_of_stack = stack + count;
}

    static void
push (int elem)
{
    if (top_of_stack == end_of_stack)
        enlarge ();
    *top_of_stack++ = elem;
}

    static int
empty (void)
{
    return stack == top_of_stack;
}

    static void
parse (void)
{
    int ch, c, sign, lit, * clause, i, l;
    msg ("parsing %s", input_name);
    line = 1;
    while ((ch = nextch ()) == 'c')
        while ((ch = nextch ()) != '\n')
            if (ch == EOF)
                perr ("EOF in comment");
    if (ch != 'p')
        perr ("expected 'c' or 'p'");
    if (nextch () != ' ' ||
            nextch () != 'c' ||
            nextch () != 'n' ||
            nextch () != 'f' ||
            nextch () != ' ')
        perr ("invalid 'p' header");

    ch = nextch ();
    if (!isdigit (ch))
        perr ("expected digit after 'p cnf '");

    m = ch - '0';
    while (isdigit (ch = nextch ()))
        m = 10 * m + (ch - '0');

    if (ch != ' ')
        perr ("expected space after 'p cnf <vars>'");

    ch = nextch ();
    if (!isdigit (ch))
        perr ("expected digit after 'p cnf <vars> '");

    n = ch - '0';
    while (isdigit (ch = nextch ()))
        n = 10 * n + (ch - '0');

    while (ch == ' ')
        ch = nextch ();

    if (ch != '\n')
        perr ("expected new line after header");

    msg ("parsed header p cnf %d %d", m, n);

    NEW (trail, m);
    NEW (assignment, m + 1);
    NEW (clauses, n);
    NEW (nonfalse, n);

    next_to_propagate = top_of_trail = trail;
    c = 0;

    for (;;)
    {
        ch = nextch ();
        if (ch == EOF)
        {
            if (!empty ())
                perr ("unexpected EOF: trailing zero missing");

            if (c + 1 == n)
                perr ("clause missing");

            if (c < n)
                perr ("clauses missing");

            assert (c == n);

            return;
        }

        if (ch == 'c')
        {
            while ((ch  = nextch ()) != '\n')
                if (ch == EOF)
                    perr ("unexpected EOF in comment");

            continue;
        }

        if (ch == '-')
        {
            sign = -1;
            ch = nextch ();
            if (ch == '0')
                perr ("zero after '-'");
            if (!isdigit (ch))
                perr ("expected digit after '-'");
        }
        else
        {
            sign = 1;
            if (!isdigit (ch))
                perr ("expected digit or '-'");
        }

        if (c >= n)
            perr ("too many clauses");

        lit = ch - '0';
        while (isdigit (ch = nextch ()))
            lit = 10 * lit + (ch - '0');

        if (lit > m)
            perr ("maximum variable index exceeded");

        lit *= sign;

        if (lit)
        {
            push (lit);
        }
        else
        {
            l = top_of_stack - stack;
            NEW (clause, l + 1);
            for (i = 0; i < l; i++)
                clause[i] = stack[i];
            clause[i] = 0;
            clauses[c++] = clause;
            top_of_stack = stack;
        }
    }
}

    static void
connect (void)
{
    int i, var, lit, * p, * q, sign, count, lits;

    NEW (counts, 2 * m + 1);
    counts += m;			/* accessible as [-m,...,-1,0,1,..,m] */

    for (i = 0; i < n; i++)
    {
        for (p = clauses[i]; (lit = *p); p++)
            counts[lit]++;

        nonfalse[i] = p - clauses[i];
    }

    lits = 0;
    for (var = 1; var <= m; var++)
        lits += counts[var] + counts[-var];

    NEW (lit2occs, 2 * m + 1);
    lit2occs += m;

    NEW (occs, lits + 2 * m);
    q = occs;

    for (var = 1; var <= m; var++)
        for (sign = 1; sign >= -1; sign -= 2)
        {
            lit = sign * var;
            count = counts[lit];
            q += count;
            lit2occs[lit] = q;
            *q++ = -1;		/* neg. sentinel for clause indices */
        }
    assert (occs + lits + 2 * m == q);

    for (i = 0; i < n; i++)
        for (p = clauses[i]; (lit = *p); p++)
        {
            q = lit2occs[lit];
            *--q = i;
            lit2occs[lit] = q;
        }

    msg ("connected %d literals", lits);
}

/* A literal is unassigned if its variable is unassigned.  Otherwise it has
 * the value of the assignment of its variable if it is a positive literal
 * or the negated value if the literal is a negated variable.
 */
    static int
val (int lit)
{
    int res;
    assert (lit);
    assert (abs (lit) <= m);
    if ((res = assignment[abs (lit)]) && lit < 0)
        res = -res;
    return res;
}

    static int
satisfied (int * clause)
{
    int * p, lit;
    for (p = clause; (lit = *p); p++)
        if (val (lit) > 0)
            return 1;
    return 0;
}

    static void
print (void)
{
    int c = units, i, * p, lit, tmp;
    for (i = 0; i < n; i++)
        if (!satisfied (clauses[i]))
            c++;
    fprintf (output, "p cnf %d %d\n", m, c);
    for (i = 0; i < n; i++)
        if (!satisfied (p = clauses[i]))
        {
            while ((lit = *p++))
            {
                tmp = val (lit);
                if (tmp)
                {
                    assert (tmp < 0);
                    continue;
                }
                fprintf (output, "%d ", lit);
            }
            fputs ("0\n", output);
        }
    for (lit = 1; lit <= m; lit++)
        if ((tmp = val (lit)))
            fprintf (output, "%d 0\n", (tmp < 0 ? -lit : lit));
    fflush (output);
}

    static void
assign (int lit)
{
    if (!decision)
        units++;
    assert (top_of_trail < trail + m);
    *top_of_trail++ = lit;
    assignment [abs (lit)] = lit;
    LOG (msg ("assign %d", lit));
}

    static void
decide (int lit)
{
    assert (!decision);
    LOG (msg ("decide %d", lit));
    decision = next_to_propagate;
    decisions++;
    assign (lit);
}

    static void
unassign (int lit)
{
    assert (val (lit) > 0);
    assignment[abs (lit)] = 0;
    LOG (msg ("unassign %d", lit));
}

    static void
inc (int lit)
{
    int * p, clsidx;
    for (p = lit2occs[lit]; (clsidx = *p) >= 0; p++)
        nonfalse[clsidx]++;
}

    static void
backtrack (void)
{
    int lit;
    assert (decision);
    while (top_of_trail > decision)
    {
        lit = *--top_of_trail;
        unassign (lit);
        if (top_of_trail < next_to_propagate)
            inc (-lit);
    }
    decision = 0;
    next_to_propagate = top_of_trail;
}

    static int
bcp (void)
{
    int lit, * p, clsidx, failed, count, other, * q, tmp;

    failed = 0;
    while (!failed && next_to_propagate < top_of_trail)
    {
        lit = -*next_to_propagate++;
        LOG (msg ("propagate %d", -lit));
        propagations++;
#pragma omp single
        {
            p = lit2occs[lit];
            while (*p >= 0)
            {
#pragma omp task firstprivate(p) shared(nonfalse, clauses, failed)
                {
                    clsidx = *p;
                    count = nonfalse[clsidx];
                    assert (count > 0);
                    count--;
                    nonfalse[clsidx] = count;
                    if (!count)
                    {
                        if (!failed)
                        {
FOUND_CONFLICTING_CLAUSE:
                            failed = 1;
                            LOG (msg ("conflicting clause %d", clsidx));
                        }
                    }
                    else if (!failed && count == 1)
                    {
                        tmp = 0;
                        for (q = clauses[clsidx]; (other = *q); q++)
                        {
                            tmp = val (other);
                            if (tmp >= 0)
                                break;
                        }
                        if (!other)
                            goto FOUND_CONFLICTING_CLAUSE;
                        if (tmp <= 0)
                            /*    continue;*/
                            /*LOG (msg ("implying %d by clause %d", other, clsidx));*/
                            assign (other);
                    }
                }
                p++;
            }
        }
    }

    return !failed;
}

    static void
process (void)
{
    int i, * clause, lit, tmp, var, changed, failed;

    for (i = 0; i < n; i++)
    {
        clause = clauses[i];
        if (!clause[0])
        {
            LOG (msg ("found empty clause %d", i));
            inconsistent = 1;
            return;
        }

        if (clause[1])
            continue;

        lit = clause[0];
        tmp = val (lit);
        if (tmp > 0)
            continue;

        if (tmp < 0)
        {
            LOG (msg ("found contradictory unit clause %d", i));
            inconsistent = 1;
            return;
        }

        LOG (msg ("implying %d by unit clause %d", lit, i));
        assign (lit);
    }

    if (!bcp ())
    {
        msg ("initial top level propagation failed");
        inconsistent = 1;
        return;
    }

    do {
        changed = 0;
        for (var = 1; var <= m; var++)
        {
            if (val (var))
                continue;

            lit = var;
            decide (lit);
            failed = !bcp ();
            backtrack ();
            if (failed)
            {
FOUND_FAILED_LITERAL:
                changed = 1;
                LOG (msg ("failed literal %d", lit));
                assign (-lit);
                if (!bcp ())
                {
                    msg ("top level propagation in round %d failed", rounds);
                    inconsistent = 1;
                    return;
                }
            }
            else
            { 
                lit = -var;
                decide (lit);
                failed = !bcp ();
                backtrack ();
                if (failed)
                    goto FOUND_FAILED_LITERAL;
            }
        }
        rounds++;
        msg ("%d units after round %d", units, rounds);
    } while (changed);
}

    static void
stats (void)
{
    double t = seconds (), p = propagations;
    p /= 1e6;
    msg ("%d units, %d rounds, %d decisions, %lld propagations", 
            units, rounds, decisions, propagations);
    msg ("%.1f million propagations per second", (t > 0 ? p / t : 0));
}

    static void
release (void)
{
    int i;
    for (i = 0; i < n; i++)
        free (clauses[i]);
    free (clauses);
    free (nonfalse);
    free (counts - m);
    free (lit2occs - m);
    free (occs);
    free (assignment);
    free (trail);
    free (stack);
}

int threadNum = 1;

    int
main (int argc, char ** argv)
{
    int i, close_input = 0, close_output = 0;

    /*for (i = 2; i <= argc; i++)*/
    /*{*/
        /*if (strcmp (argv[1], "-p"))*/
        /*{*/
            /*printf ("usage: sflprepc -p num [ <input> [ <output> ]]\n");*/
            /*exit (0);*/
        /*}*/

        /*[>if (output_name)<]*/
            /*[>die ("too many command line options");<]*/
        /*if (threadNum == 1) */
            /*threadNum = atoi(argv[i]);*/
        /*else {*/
        /*if (input_name)*/
            /*output_name = argv[i];*/
        /*else*/
            /*input_name = argv[i];*/
        /*}*/
    /*}*/
    
    if (strcmp (argv[1], "-p") || argc > 5 || argc < 3)
        printf("usage: sflprepc -p num  [<input> [ <output> ]]\n");
    threadNum   = atoi(argv[2]);
    input_name  = argv[3];
    output_name = argv[4];
    

    if (input_name && strcmp (input_name, "-"))
    {
        if (!(input = fopen (input_name, "r")))
            die ("can not read '%s'", input_name);

        close_input = 1;
    }
    else
    {
        input = stdin;
        input_name = "<stdin>";
    }

    parse ();
    connect ();

    if (close_input)
        fclose (input);

    omp_set_num_threads(threadNum);
    process ();
    stats ();

    if (output_name && strcmp (output_name, "-"))
    {
        if (!(output = fopen (output_name, "w")))
            die ("can not write '%s'", output_name);

        close_output = 1;
    }
    else
    {
        output = stdout;
        output_name = "<stdout>";
    }

    if (inconsistent)
    {
        msg ("inconsistent");
        fprintf (output, "p cnf 1 2\n-1 0\n1 0\n");
        fflush (output);
    }
    else
        print ();

    if (close_output)
        fclose (output);

    release ();

    return 0;
}
