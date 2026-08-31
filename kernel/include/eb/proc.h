#ifndef EB_PROC_H
#define EB_PROC_H

#include <eb/types.h>
#include <eb/cap.h>
#include <eb/thread.h>

/* Processes: a domain, an address space, and a thread running in ring 3.
 *
 * The address space is the second wall, behind the capability table.
 * The table already means a program cannot name anything it was not
 * given; the address space means it cannot reach the kernel's memory
 * even by accident, and cannot see another process's at all.
 *
 * Every process maps the same kernel into the upper half and its own
 * program into the lower one. That is why a system call needs no change
 * of page tables -- the kernel is already there, just not reachable
 * from ring 3.
 */

typedef struct process process;

/* Creates a process from a chunk of the kernel image marked as user
 * code. The program is mapped read and execute; a stack and a small
 * data page are mapped read and write, never executable. */
process *proc_create(const char *name, const void *entry_point);

domain     *proc_domain(process *p);
const char *proc_name(const process *p);
u64         proc_id(const process *p);
phys_addr   proc_pml4(const process *p);

/* Starts the process's first thread in ring 3. */
bool proc_start(process *p);

/* Where a process finds itself in its own address space. */
#define USER_CODE_BASE  0x0000000000400000ULL
#define USER_STACK_TOP  0x0000000000800000ULL
#define USER_STACK_SIZE (16 * 1024)

/* Copies out of and into a user address space, checking every page
 * against the tables first and opening the SMAP window only for as long
 * as the copy takes. Returns false rather than faulting if the range is
 * not user memory the process actually owns. */
bool copy_from_user(void *dst, virt_addr src, u64 len);
bool copy_to_user(virt_addr dst, const void *src, u64 len);

/* Called from the fault handler when the fault came from ring 3: report
 * it, end that thread, and let the rest of the system carry on. */
void proc_fault(const char *what, virt_addr where);

u64 proc_count(void);
u64 proc_faults(void);

#endif /* EB_PROC_H */
