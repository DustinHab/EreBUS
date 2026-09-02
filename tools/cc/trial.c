/* trial.c -- a kernel file through the machine's compiler, on the host.
 *
 * Nothing runs; this measures the distance. The file named on the
 * command line after this one is included whole, and a main is added
 * so the compiler has an entry to insist on.
 */
#include "trial_target.c"

long main(long console, long inbox)
{
    return 0;
}
