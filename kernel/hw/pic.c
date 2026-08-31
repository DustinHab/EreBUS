/*
 * pic.c -- the 8259 interrupt controller pair.
 */
#include <eb/pic.h>
#include <eb/io.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x11   /* start initialisation, expect ICW4 */
#define ICW4_8086 0x01   /* 8086 mode rather than the ancient 8080 one */
#define OCW3_READ_ISR 0x0B
#define EOI 0x20

static u8 offset_master = 0x20;

void pic_init(u8 off1, u8 off2)
{
    offset_master = off1;

    u8 mask1 = inb(PIC1_DATA);
    u8 mask2 = inb(PIC2_DATA);

    /* The initialisation is a fixed four-word sequence. io_wait() sits
     * between the writes because the chip is older than the bus timing
     * that would otherwise cover for it. */
    outb(PIC1_CMD, ICW1_INIT); io_wait();
    outb(PIC2_CMD, ICW1_INIT); io_wait();
    outb(PIC1_DATA, off1);     io_wait();   /* ICW2: vector base */
    outb(PIC2_DATA, off2);     io_wait();
    outb(PIC1_DATA, 0x04);     io_wait();   /* ICW3: slave on line 2 */
    outb(PIC2_DATA, 0x02);     io_wait();   /* ICW3: slave identity */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_mask_all(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_set_mask(u8 irq, bool masked)
{
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  bit  = (u8)(1u << (irq & 7));
    u8  cur  = inb(port);

    outb(port, masked ? (u8)(cur | bit) : (u8)(cur & ~bit));

    /* Anything on the slave only reaches the CPU while line 2 on the
     * master is open. Forgetting this is the classic reason a keyboard
     * works but a disk controller stays silent. */
    if (irq >= 8 && !masked)
        pic_set_mask(2, false);
}

void pic_eoi(u8 irq)
{
    if (irq >= 8) outb(PIC2_CMD, EOI);
    outb(PIC1_CMD, EOI);
}

bool pic_spurious(u8 irq)
{
    if (irq != 7 && irq != 15) return false;

    u16 cmd = (irq == 7) ? PIC1_CMD : PIC2_CMD;
    outb(cmd, OCW3_READ_ISR);
    u8 isr = inb(cmd);

    /* If the in-service bit is clear, nothing was really being served:
     * the interrupt was noise. */
    if (isr & (u8)(1u << (irq & 7))) return false;

    /* A spurious line 15 still went through the master, so the master
     * alone has to be acknowledged. */
    if (irq == 15) outb(PIC1_CMD, EOI);
    return true;
}
