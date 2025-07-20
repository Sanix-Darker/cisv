#define _GNU_SOURCE
#include "cisv_parser.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

// NOTE: this is at compile time only
#define RINGBUF_SIZE (1 << 20) // 1 MiB (we may adjust according to needs)

struct cisv_parser {
    uint8_t *base;              // pointer to the whole input, if memory-mapped
    size_t size;                // length of that mapping
    int fd;                     // the underlying file descriptor (-1 ⇒ none)
    uint8_t *ring;              // malloc’ed circular buffer when not mmapped
    size_t head;                // write head: next byte slot to fill
    enum {
        S_UNQUOTED,             // Parsing ordinary characters outside quotes.

        S_QUOTED,               // Inside a quoted field. Delimiters/newlines are treated as data.

        S_QUOTE_ESC             // Just saw a ". Decide whether it’s a doubled quote ("" → stay in field) or
                                // the closing quote.
    } st;
    cisv_field_cb fcb;           // field callback fired whenever a full cell is ready
                                // (delimiter or row-ending newline encountered, consistent with RFC 4180 rules)

    cisv_row_cb rcb;             // row callback fired after the last field of each record
    void *user;
    const uint8_t *field_start; // where the in-progress field began
};

static inline void yield_field(cisv_parser *p, const uint8_t *start, const uint8_t *end) {
    if (p->fcb) p->fcb(p->user, (const char *)start, (size_t)(end - start));
}

static inline void yield_row(cisv_parser *p) {
    if (p->rcb) p->rcb(p->user);
}

// Universal scalar parser. Works on all architectures.
static void parse_scalar(cisv_parser *p, const uint8_t *buf, size_t len) {
    const uint8_t *cur = buf;
    const uint8_t *end = buf + len;

    for (; cur < end; ++cur) {
        uint8_t c = *cur;
        switch (p->st) {
            case S_UNQUOTED:
                if (c == ',') {
                    yield_field(p, p->field_start, cur);
                    p->field_start = cur + 1;
                } else if (c == '"') {
                    p->st = S_QUOTED;
                } else if (c == '\n') {
                    yield_field(p, p->field_start, cur);
                    yield_row(p);
                    p->field_start = cur + 1;
                }
                break;
            case S_QUOTED:
                if (c == '"') p->st = S_QUOTE_ESC;
                break;
            case S_QUOTE_ESC:
                if (c == '"') {
                    p->st = S_QUOTED; // Escaped quote
                } else {
                    p->st = S_UNQUOTED; // End of quoted field
                    // Reprocess the current character as unquoted
                    cur--;
                }
                break;
        }
    }
}

static int parse_memory(cisv_parser *p, const uint8_t *buf, size_t len) {
    p->field_start = buf;
    p->st = S_UNQUOTED;

    // Use the portable scalar parser directly.
    // The SIMD logic is now bypassed on non-x86 architectures.
    parse_scalar(p, buf, len);

    // Trailing field
    if (p->field_start < buf + len) {
        yield_field(p, p->field_start, buf + len);
    }
    return 0;
}

cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user) {
    cisv_parser *p = (cisv_parser *)calloc(1, sizeof(*p));
    if (!p) return NULL;

    // We Allocate ring buffer with a generic size since cisv_VEC_BYTES might be 1
    p->ring = (uint8_t*)malloc(RINGBUF_SIZE + 64); // 64 is safe padding
    if (!p->ring) {
        free(p);
        return NULL;
    }

    p->fd = -1;
    p->fcb = fcb;
    p->rcb = rcb;
    p->user = user;
    return p;
}

void cisv_parser_destroy(cisv_parser *p) {
    if (!p) return;
    if (p->base && p->base != MAP_FAILED) munmap(p->base, p->size);
    if (p->fd >= 0) close(p->fd);
    free(p->ring);
    free(p);
}

int cisv_parser_parse_file(cisv_parser *p, const char *path) {
    struct stat st;
    // NOTE:
    //
    // O_NOATIME is Linux-specific, remove for portability on macOS
    p->fd = open(path, O_RDONLY);
    if (p->fd < 0) return -errno;
    if (fstat(p->fd, &st)) {
        close(p->fd);
        return -errno;
    }
    p->size = (size_t)st.st_size;

    p->base = (uint8_t *)mmap(NULL, p->size, PROT_READ, MAP_PRIVATE, p->fd, 0);
    if (p->base == MAP_FAILED) {
        close(p->fd);
        return -errno;
    }

    // madvise hints (don't fail if not present)
#ifdef MADV_SEQUENTIAL
    madvise(p->base, p->size, MADV_SEQUENTIAL);
#endif
#ifdef MADV_HUGEPAGE
    madvise(p->base, p->size, MADV_HUGEPAGE);
#endif

    return parse_memory(p, p->base, p->size);
}

int cisv_parser_write(cisv_parser *p, const uint8_t *chunk, size_t len) {
    if (len >= RINGBUF_SIZE) return -EINVAL;

    if (p->head + len > RINGBUF_SIZE) {
        parse_memory(p, p->ring, p->head);
        p->head = 0;
    }

    memcpy(p->ring + p->head, chunk, len);
    p->head += len;

    if (memchr(chunk, '\n', len) || p->head > (RINGBUF_SIZE / 2)) {
        parse_memory(p, p->ring, p->head);
        p->head = 0;
    }
    return 0;
}

void cisv_parser_end(cisv_parser *p) {
    if (p->head > 0) {
        parse_memory(p, p->ring, p->head);
        p->head = 0;
    }
}
