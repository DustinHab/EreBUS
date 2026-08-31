/*
 * serial.c -- COM1 als Entwicklerkonsole.
 */
#include <eb/serial.h>
#include <eb/io.h>

#define COM1 0x3F8

/* Registerabstände ab der Basisadresse. */
#define REG_DATA        0  /* bei DLAB=0: Daten; bei DLAB=1: Teiler niedrig */
#define REG_IER         1  /* bei DLAB=1: Teiler hoch */
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
    outb(COM1 + REG_IER, 0x00);              /* keine Unterbrechungen */
    outb(COM1 + REG_LINE_CTRL, LCR_DLAB);    /* Teiler ansprechbar machen */
    outb(COM1 + REG_DATA, 0x01);             /* 115200 = 115200 / 1 */
    outb(COM1 + REG_IER, 0x00);
    outb(COM1 + REG_LINE_CTRL, LCR_8N1);     /* 8 Bit, keine Parität, 1 Stopp */
    outb(COM1 + REG_FIFO_CTRL, 0xC7);        /* FIFO an, leeren, Schwelle 14 */
    outb(COM1 + REG_MODEM_CTRL, 0x0B);       /* DTR, RTS, OUT2 */

    /* Selbsttest: im Schleifenmodus muss zurückkommen, was wir senden.
     * Fehlt die Schnittstelle, liest man üblicherweise 0xFF. */
    outb(COM1 + REG_MODEM_CTRL, 0x1E);       /* Schleifenmodus */
    outb(COM1 + REG_DATA, 0xAE);
    present = (inb(COM1 + REG_DATA) == 0xAE);

    outb(COM1 + REG_MODEM_CTRL, 0x0F);       /* zurück in den Normalbetrieb */
    return present;
}

bool serial_present(void) { return present; }

void serial_putc(char c)
{
    if (!present) return;
    if (c == '\n') serial_putc('\r');

    /* Warten, bis das Senderegister wieder frei ist. Der Zähler ist eine
     * Notbremse: ohne ihn steht der Kernel, wenn die Gegenstelle klemmt. */
    for (u32 spin = 0; spin < 100000u; spin++)
        if (inb(COM1 + REG_LINE_STATUS) & LSR_THR_EMPTY)
            break;

    outb(COM1 + REG_DATA, (u8)c);
}

void serial_write(const char *s)
{
    while (*s) serial_putc(*s++);
}
