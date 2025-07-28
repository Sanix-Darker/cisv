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

static inline void yield_field(
    cisv_parser *parser,
    const uint8_t *start,
    const uint8_t *end
) {
    if (!parser->fcb) {
        return;
    }

    parser->fcb(
        parser->user,
        (const char *)start,
        (size_t)(end - start)
    );
}

static inline void yield_row(cisv_parser *parser) {
    if (!parser->rcb) {
        return;
    }

    parser->rcb(parser->user);
}

// Universal scalar parser. Works on all architectures.
static void parse_scalar(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    for (; cur < end; ++cur) {
        uint8_t c = *cur;
        switch (parser->st) {
            case S_UNQUOTED:
                if (c == ',') {
                    yield_field(parser, parser->field_start, cur);
                    parser->field_start = cur + 1;
                } else if (c == '"') {
                    parser->st = S_QUOTED;
                } else if (c == '\n') {
                    yield_field(parser, parser->field_start, cur);
                    yield_row(parser);
                    parser->field_start = cur + 1;
                }
                break;
            case S_QUOTED:
                if (c == '"') parser->st = S_QUOTE_ESC;
                break;
            case S_QUOTE_ESC:
                if (c == '"') {
                    parser->st = S_QUOTED; // Escaped quote
                } else {
                    parser->st = S_UNQUOTED; // End of quoted field
                    // Reprocess the current character as unquoted
                    cur--;
                }
                break;
        }
    }
}

static int parse_memory(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    parser->field_start = buffer;
    parser->st = S_UNQUOTED;

    // Use the portable scalar parser directly.
    // The SIMD logic is now bypassed on non-x86 architectures.
    parse_scalar(parser, buffer, len);

    // Trailing field
    if (parser->field_start < buffer + len) {
        yield_field(parser, parser->field_start, buffer + len);
    }
    return 0;
}

cisv_parser *cisv_parser_create(
    cisv_field_cb fcb,
    cisv_row_cb rcb,
    void *user
) {
    cisv_parser *parser = (cisv_parser *)calloc(1, sizeof(*parser));
    if (!parser) return NULL;

    // We Allocate ring buffer with a generic size since cisv_VEC_BYTES might be 1
    parser->ring = (uint8_t*)malloc(RINGBUF_SIZE + 64); // 64 is safe padding
    if (!parser->ring) {
        free(parser);
        return NULL;
    }

    parser->fd = -1;
    parser->fcb = fcb;
    parser->rcb = rcb;
    parser->user = user;
    return parser;
}

void cisv_parser_destroy(cisv_parser *parser) {
    if (!parser) {
        return;
    }

    if (parser->base && parser->base != MAP_FAILED) {
        munmap(parser->base, parser->size);
    }
    if (parser->fd >= 0) {
        close(parser->fd);
    }

    free(parser->ring);
    free(parser);
}

int cisv_parser_parse_file(
    cisv_parser *parser,
    const char *path
) {
    struct stat st;
    // NOTE:
    // O_NOATIME is Linux-specific, remove for portability on macOS
    parser->fd = open(path, O_RDONLY);
    if (parser->fd < 0) return -errno;
    if (fstat(parser->fd, &st)) {
        close(parser->fd);
        return -errno;
    }
    parser->size = (size_t)st.st_size;

    parser->base = (uint8_t *)mmap(NULL, parser->size, PROT_READ, MAP_PRIVATE, parser->fd, 0);
    if (parser->base == MAP_FAILED) {
        close(parser->fd);
        return -errno;
    }

    // madvise hints (don't fail if not present)
#ifdef MADV_SEQUENTIAL
    madvise(parser->base, parser->size, MADV_SEQUENTIAL);
#endif
#ifdef MADV_HUGEPAGE
    madvise(parser->base, parser->size, MADV_HUGEPAGE);
#endif

    return parse_memory(parser, parser->base, parser->size);
}

int cisv_parser_write(
    cisv_parser *parser,
    const uint8_t *chunk,
    size_t len
) {
    if (len >= RINGBUF_SIZE) return -EINVAL;

    if (parser->head + len > RINGBUF_SIZE) {
        parse_memory(parser, parser->ring, parser->head);
        parser->head = 0;
    }

    memcpy(parser->ring + parser->head, chunk, len);
    parser->head += len;

    if (memchr(chunk, '\n', len) || parser->head > (RINGBUF_SIZE / 2)) {
        parse_memory(parser, parser->ring, parser->head);
        parser->head = 0;
    }
    return 0;
}

void cisv_parser_end(cisv_parser *parser) {
    if (parser->head <= 0) {
        return;
    }

    parse_memory(parser, parser->ring, parser->head);
    parser->head = 0;
}
