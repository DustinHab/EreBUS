/*
 * serial.c -- COM1 as the developer console.
 */
#include <eb/serial.h>
#include <eb/io.h>

#define COM1 0x3F8

/* Register offsets from the base address. */
#define REG_DATA        0  /* DLAB=0: data; DLAB=1: divisor low  */
#define REG_IER         1  /* DLAB=1: divisor high               */
#define REG_FIFO_CTRL   2
#define REG_LINE_CTRL   3
#define REG_MODEM_CTRL  4
#define REG_LINE_STATUS 5

#define LCR_DLAB        0x80
#define LCR_8N1         0x03
#define LSR_THR_EMPTY   0x20

static bool present;

bool serial_init(void)
{
    outb(COM1 + REG_IER, 0x00);              /* no interrupts */
    outb(COM1 + REG_LINE_CTRL, LCR_DLAB);    /* expose the divisor */
    outb(COM1 + REG_DATA, 0x01);             /* 115200 = 115200 / 1 */
    outb(COM1 + REG_IER, 0x00);
    outb(COM1 + REG_LINE_CTRL, LCR_8N1);     /* 8 bits, no parity, 1 stop */
    outb(COM1 + REG_FIFO_CTRL, 0xC7);        /* FIFO on, cleared, trigger 14 */
    outb(COM1 + REG_MODEM_CTRL, 0x0B);       /* DTR, RTS, OUT2 */

    /* Self test: in loopback whatever we send must come back. With no
     * port present one usually reads 0xFF. */
    outb(COM1 + REG_MODEM_CTRL, 0x1E);
    outb(COM1 + REG_DATA, 0xAE);
    present = (inb(COM1 + REG_DATA) == 0xAE);

    outb(COM1 + REG_MODEM_CTRL, 0x0F);       /* back to normal operation */
    return present;
}

bool serial_present(void) { return present; }

void serial_putc(char c)
{
    if (!present) return;
    if (c == '\n') serial_putc('\r');

    /* Wait for the transmit register to free up. The counter is an
     * emergency brake: without it the kernel stalls if the other end
     * jams. */
    for (u32 spin = 0; spin < 100000u; spin++)
        if (inb(COM1 + REG_LINE_STATUS) & LSR_THR_EMPTY)
            break;

    outb(COM1 + REG_DATA, (u8)c);
}

void serial_write(const char *s)
{
    while (*s) serial_putc(*s++);
}
