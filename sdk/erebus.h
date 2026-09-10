/*
 * erebus.h -- the program interface, for a program written outside the
 * kernel tree and compiled by the machine (or by tools/cchost on the
 * host). MANUAL.md chapter 18 is the reference this follows.
 *
 * A program is entered as  long main(long console, long inbox)  holding
 * exactly two capabilities: the console it may say things to, and the
 * letter box it receives gifts and messages on. Everything else it ever
 * holds arrives through the letter box. There is no library under this
 * header: the eight system calls are the whole interface, and this file
 * is only the calling convention written down once.
 */
#ifndef EREBUS_H
#define EREBUS_H

/* The eight calls. */
#define EB_EXIT    0     /* exit(code) */
#define EB_YIELD   1     /* yield(): rest until the next tick */
#define EB_SEND    2     /* send(port, tag, w0, w1, w2) */
#define EB_RECEIVE 3     /* receive(port, buffer, no_wait) -> a message of 72 bytes */
#define EB_READ    4     /* read(handle, offset) -> eight bytes */
#define EB_WRITE   5     /* write(handle, offset, value): eight bytes */
#define EB_PASS    6     /* pass(port, tag, capability, mask, word) */
#define EB_CLOCK   7     /* clock() -> seconds since midnight */

/* What a call answers when it does not do what was asked. */
#define EB_OK        0L
#define EB_DENIED   (-1L)   /* no such capability, or not enough rights */
#define EB_WOULDFAIL (-2L)  /* the port is full, or nothing to receive */
#define EB_BADCALL  (-3L)   /* no such call */

/* Rights a capability may carry, and the mask pass narrows with. */
#define EB_READ_RIGHT    1
#define EB_WRITE_RIGHT   2
#define EB_GRANT_RIGHT   4
#define EB_CALL_RIGHT    8
#define EB_DESTROY_RIGHT 16

/* Message tags the kernel and the standard programs use. */
#define EB_TAG_TEXT 0x54584554L   /* "TEXT": up to 24 bytes across w0..w2 */
#define EB_TAG_GIVE 0x45564947L   /* "GIVE": a capability handed to the program */
#define EB_TAG_RANG 0x474E4152L   /* "RANG": a stretch, low and high, of a split task */
#define EB_TAG_NUMB 0x424D554EL   /* "NUMB": a bare number */

/* A message as receive lays it down: 72 bytes. */
#define EB_MSG_BYTES  72
#define EB_MSG_TAG    0     /* long */
#define EB_MSG_NWORDS 8     /* int */
#define EB_MSG_NCAPS  12    /* int */
#define EB_MSG_WORDS  16    /* four longs */
#define EB_MSG_CAPS   48    /* two longs: fresh handles in this program's own table */
#define EB_MSG_MASKS  64    /* two ints: the rights each carries */

/* The calling convention: the number in rax, the arguments in rdi, rsi,
 * rdx, r10, r8; the answer in rax. The instruction itself overwrites rcx
 * and r11, and the kernel hands back rdi, rsi, rdx, r8, r9 and r10 as
 * zero; rbx, rbp and r12 to r15 keep their values. */
static inline long eb_sys3(long nr, long a0, long a1, long a2)
{
    __asm__ volatile ("syscall"
                      : "+a"(nr), "+D"(a0), "+S"(a1), "+d"(a2)
                      :
                      : "rcx", "r11", "r8", "r9", "r10", "memory");
    return nr;
}

static inline long eb_sys5(long nr, long a0, long a1, long a2, long a3, long a4)
{
    register long r10 __asm__("r10") = a3;
    register long r8  __asm__("r8")  = a4;
    __asm__ volatile ("syscall"
                      : "+a"(nr), "+D"(a0), "+S"(a1), "+d"(a2),
                        "+r"(r10), "+r"(r8)
                      :
                      : "rcx", "r11", "r9", "memory");
    return nr;
}

static inline void eb_exit(long code)                { eb_sys3(EB_EXIT, code, 0, 0); for (;;) {} }
static inline void eb_yield(void)                    { eb_sys3(EB_YIELD, 0, 0, 0); }
static inline long eb_clock(void)                    { return eb_sys3(EB_CLOCK, 0, 0, 0); }
static inline long eb_read(long handle, long offset) { return eb_sys3(EB_READ, handle, offset, 0); }
static inline long eb_write(long handle, long offset, long value)
{
    return eb_sys3(EB_WRITE, handle, offset, value);
}
static inline long eb_send(long port, long tag, long w0, long w1, long w2)
{
    return eb_sys5(EB_SEND, port, tag, w0, w1, w2);
}
static inline long eb_pass(long port, long tag, long capability, long mask, long word)
{
    return eb_sys5(EB_PASS, port, tag, capability, mask, word);
}

/* Waits for a message on the port (no_wait 0) or answers EB_WOULDFAIL
 * at once when there is none (no_wait 1). buf holds EB_MSG_BYTES. */
static inline long eb_receive(long port, void *buf, long no_wait)
{
    return eb_sys3(EB_RECEIVE, port, (long)buf, no_wait);
}

/* The parts of a received message. */
static inline long eb_msg_long(const void *buf, long at)
{
    const unsigned char *b = (const unsigned char *)buf + at;
    long v = 0;
    long i;
    for (i = 0; i < 8; i++) v |= (long)b[i] << (i * 8);
    return v;
}
static inline long eb_msg_tag(const void *buf)       { return eb_msg_long(buf, EB_MSG_TAG); }
static inline long eb_msg_word(const void *buf, long i) { return eb_msg_long(buf, EB_MSG_WORDS + i * 8); }
static inline long eb_msg_cap(const void *buf, long i)  { return eb_msg_long(buf, EB_MSG_CAPS + i * 8); }
static inline long eb_msg_ncaps(const void *buf)
{
    const unsigned char *b = (const unsigned char *)buf;
    return (long)b[EB_MSG_NCAPS] | ((long)b[EB_MSG_NCAPS + 1] << 8);
}

/* Says up to 24 letters on the console: the text rides in three words. */
static inline long eb_say(long console, const char *s)
{
    long w[3];
    char *b = (char *)w;
    long i;
    for (i = 0; i < 24; i++) b[i] = ' ';
    for (i = 0; i < 24 && s[i]; i++) b[i] = s[i];
    return eb_send(console, EB_TAG_TEXT, w[0], w[1], w[2]);
}

/* One byte of an object, through read: eight bytes at a time, the
 * offset rounded down to eight, the byte picked out. -1 past the end
 * or without the right. */
static inline long eb_byte(long handle, long at)
{
    long v = eb_read(handle, at & ~7L);
    if (v == EB_DENIED) return -1;
    return (v >> ((at & 7) * 8)) & 0xFF;
}

#endif /* EREBUS_H */
