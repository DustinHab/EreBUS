#ifndef EB_FORMATS_H
#define EB_FORMATS_H

/* Every format this system writes to a disk or puts on a wire, with
 * the number it speaks and the oldest it still reads. One place, so
 * the promise is one promise: a kernel reads every format an older
 * kernel of the same major version wrote, and a format that has to
 * change gets a new number here rather than a new shape under the old
 * one. The terminal word `formats` prints this table; MANUAL.md 17 is
 * its prose. */

/* The store's first sector: "EREBUS STORE", then the format, an
 * identity (random, given when the store is made or first met by a
 * kernel that knows this format) and when it was made. Format 0 is
 * the bare mark older kernels wrote; it is read and raised to 1. */
#define FORMAT_STORE          1u
#define FORMAT_STORE_OLDEST   0u
#define FORMAT_STORE_MARK     "EREBUS STORE"

/* A generation in the ring: header "EREBSNAP" and the format. Format 4
 * kept every payload inline; 5 moves big ones to the log. */
#define FORMAT_SNAPSHOT        5u
#define FORMAT_SNAPSHOT_OLDEST 4u
#define FORMAT_SNAPSHOT_MAGIC  0x50414E5342455245ULL   /* "EREBSNAP" */

/* An entry in the log of big objects: header "EREBBLOB", size, hash,
 * sequence; format 1 says so in the header, format 0 said nothing. */
#define FORMAT_BLOB            1u
#define FORMAT_BLOB_OLDEST     0u
#define FORMAT_BLOB_MAGIC      0x424F4C4242455245ULL   /* "EREBBLOB" */

/* The pipe's datagrams: "EBPX", then the kind. Kinds 1-15 are the
 * protocol; an unknown kind is ignored, which is how a kind is added. */
#define FORMAT_PIPE_MAGIC      0x58504245u             /* "EBPX", little-endian */
#define FORMAT_PIPE_KINDS      15u

/* A release package: "EBUPDATE", the signature, the version, the kernel. */
#define FORMAT_PACKAGE_MAGIC   "EBUPDATE"

/* A rotation of the release key: "EBROTATE", the signature, the new
 * public key and a note; signed by a key the machine trusts at the time. */
#define FORMAT_ROTATE_MAGIC    "EBROTATE"

/* A bundle (a list as one byte stream): "EBB1". */
#define FORMAT_BUNDLE_MAGIC    0x31424245u             /* "EBB1", little-endian */

/* A program image: "EBX2" with an entry offset; "EBX1" (no entry) is
 * still read, the entry being the first code byte. */
#define FORMAT_IMAGE           "EBX2"
#define FORMAT_IMAGE_OLDEST    "EBX1"

/* The loader's handover to the kernel: common/bootinfo.h, version 3;
 * a kernel still reads 2. */
#define FORMAT_HANDOVER        3u
#define FORMAT_HANDOVER_OLDEST 2u

/* Texts the kernel reads by their lines: the settings ("matter | value",
 * the last line on a matter wins, an unknown matter is kept) and the
 * nodes table (a header line naming its columns; a column the kernel
 * does not know is kept as written). */

#endif /* EB_FORMATS_H */
