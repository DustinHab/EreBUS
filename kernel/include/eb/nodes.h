#ifndef EB_NODES_H
#define EB_NODES_H

#include <eb/types.h>
#include <eb/object.h>

/* The nodes table: one text object, one row per machine met through the pipe.
 *   name | key | address | version | may
 * - a node is its key: the door key it signs the knock with
 * - name is a local petname, first set from the node's own claim; the person may change it
 * - address and version are what the kernel last saw; the kernel rewrites them
 * - may is the person's column: "work" (may ask work of this machine), "update" (may install a kernel on it), "all"
 * - a row removed is a node forgotten; a row added by hand is a node trusted before it is met */

#define NODES_MAX 16
#define NODE_MAY_WORK   1u
#define NODE_MAY_UPDATE 2u

bool    nodes_create(void);
void    nodes_adopt(object *o);
object *nodes_object(void);

/* Reads the text. Cheap; called whenever it may have changed. */
void nodes_apply(void);

u32  nodes_count(void);
i32  nodes_by_key(const u8 key[32]);
i32  nodes_by_name(const char *name);          /* letters compared without case */
i32  nodes_by_address(const u8 ip[4]);
bool nodes_name_at(u32 i, char out[24]);
bool nodes_key_at(u32 i, u8 out[32]);
bool nodes_address_at(u32 i, u8 ip[4], u16 *port);
bool nodes_version_at(u32 i, char out[24]);
u32  nodes_may_at(u32 i);
void nodes_may_words(u32 may, char out[24]);

/* A machine that proved its key: its row is made or brought up to date
 * (address, version, a name for one still unnamed). The row's index, or
 * -1 when the table is full. Quiet: no journal line, for rows carried
 * over from older settings. */
i32  nodes_meet(const char *claim, const u8 key[32], const u8 ip[4], u16 port,
                const char *version, bool quiet);

/* The person's column, written for them by a word in the terminal. */
bool nodes_allow(u32 i, u32 may);

#endif /* EB_NODES_H */
