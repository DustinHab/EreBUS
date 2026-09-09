#ifndef EB_SPIN_H
#define EB_SPIN_H

#include <eb/types.h>

/* A plain test-and-set spinlock. Correct only when interrupts are off
 * while it is held, or when it is never taken from an interrupt -- use
 * the _irq forms otherwise, which disable interrupts for the caller so
 * a handler on the same processor cannot deadlock on a lock the
 * interrupted code holds. On one processor the lock is always free; it
 * earns its keep once the application processors run. */
typedef volatile u32 spinlock;

static inline void spin_lock(spinlock *l)
{
    while (__sync_lock_test_and_set(l, 1u))
        while (*l) __asm__ volatile ("pause");
}

static inline void spin_unlock(spinlock *l) { __sync_lock_release(l); }

static inline u64 spin_lock_irq(spinlock *l)
{
    u64 f;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(f) :: "memory");
    spin_lock(l);
    return f;
}

static inline void spin_unlock_irq(spinlock *l, u64 f)
{
    spin_unlock(l);
    if (f & (1ULL << 9)) __asm__ volatile ("sti" ::: "memory");
}

#endif /* EB_SPIN_H */
