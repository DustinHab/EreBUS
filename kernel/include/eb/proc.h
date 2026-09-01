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
process *proc_create(const char *name, const void *entry_point,
                     object *console);

/* The program as it appears in the object graph. Pointing this object
 * at something hands the program that thing -- the same gesture as
 * anywhere else, and the only way a program comes to hold anything. */
object *proc_object(process *p);
bool    proc_grant(object *program, object *what, u32 rights);

/* The same giving, with one number riding along in the message. The
 * runner reads a time budget off its first gift this way. */
bool    proc_grant_word(object *program, object *what, u32 rights,
                        u64 word);

/* A bare number into the program's letter box, no capability. */
bool    proc_post_number(object *program, u64 tag, u64 w0);

/* Ends a running program from outside; it finishes at its next step
 * into the kernel and is reaped like any other end. */
bool    proc_end(object *program);

/* And taking it back. The program is not asked and cannot refuse; it
 * finds out by trying. */
bool    proc_revoke(object *program, object *what);

/* Whether a program object still refers to something running. A graph
 * restored from disk can contain one that does not. */
bool    proc_is_running(object *program);

/* The capability table behind a running program's object, so the shell
 * can show what the program holds. NULL unless it is running. */
domain *proc_domain_of(object *program);

/* The living, one row at a time, for the activity table. mem_kib is
 * measured, not booked: the walk counts every frame the lower half
 * owns, tables included, shared code excluded. */
u32  proc_live_count(void);
bool proc_live_at(u32 i, const char **name, u64 *id, u64 *holds,
                  u64 *ran_ns, u64 *mem_kib);

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
