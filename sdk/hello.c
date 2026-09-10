/*
 * hello.c -- a program from outside the kernel tree, on the machine.
 *
 * Says hello on its console, reads the clock through the one call that
 * needs no capability, and, given a text (point the running program at
 * one), says how long it is. Compiled on the machine: lay erebus.h and
 * this text in one list, stand on the list, `compile hello.c`, then
 * `run hello.c code`. Or on the host: build/cchost sdk/hello.c sdk/erebus.h.
 */
#include "erebus.h"

long main(long console, long inbox)
{
    eb_say(console, "hello from the sdk");
    if (eb_clock() >= 0) eb_say(console, "the clock answers");

    /* Every gift that comes within a second: the first capability in the
     * message is the thing given; a text's length is its letters up to
     * the first zero byte, found by reading until that byte or until the
     * read is refused at the object's end. (A program is given something
     * at its start already; what the person gives it follows.) */
    long i;
    for (i = 0; i < 100; i++) {
        unsigned char m[EB_MSG_BYTES];
        if (eb_receive(inbox, m, 1) == EB_OK) {
            if (eb_msg_ncaps(m) > 0) {
                long h = eb_msg_cap(m, 0), n = 0;
                while (eb_byte(h, n) > 0) n++;
                char line[24];
                long k = 0, d = n, len = 0, digits[20];
                if (d == 0) digits[len++] = 0;
                while (d > 0) { digits[len++] = d % 10; d /= 10; }
                const char *head = "given ";
                while (head[k]) { line[k] = head[k]; k++; }
                while (len > 0) line[k++] = (char)('0' + digits[--len]);
                const char *tail = " bytes";
                for (d = 0; tail[d]; d++) line[k++] = tail[d];
                line[k] = 0;
                eb_say(console, line);
            }
            continue;                 /* more may be waiting */
        }
        eb_yield();
    }
    return 0;
}
