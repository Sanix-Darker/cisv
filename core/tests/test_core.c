#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include "cisv/parser.h"
#include "cisv/writer.h"
#include "cisv/transformer.h"

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { \
    test_count++; \
    printf("  Testing: %s... ", name); \
} while(0)

#define PASS() do { \
    pass_count++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

// Test data
static int field_count = 0;
static int row_count = 0;
static char last_field[1024];

// Stored fields for multiline tests
#define MAX_STORED_FIELDS 64
static char stored_fields[MAX_STORED_FIELDS][4096];
static size_t stored_field_lens[MAX_STORED_FIELDS];
static int stored_field_count = 0;

static void test_field_cb(void *user, const char *data, size_t len) {
    (void)user;
    field_count++;
    if (len < sizeof(last_field)) {
        memcpy(last_field, data, len);
        last_field[len] = '\0';
    }
    if (stored_field_count < MAX_STORED_FIELDS && len < sizeof(stored_fields[0])) {
        memcpy(stored_fields[stored_field_count], data, len);
        stored_fields[stored_field_count][len] = '\0';
        stored_field_lens[stored_field_count] = len;
        stored_field_count++;
    }
}

static void test_row_cb(void *user) {
    (void)user;
    row_count++;
}

static void reset_test_state(void) {
    field_count = 0;
    row_count = 0;
    stored_field_count = 0;
    memset(last_field, 0, sizeof(last_field));
}

// Helper to create a temp file from a string
static const char* write_temp_csv(const char *content) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/test_cisv_core_%d.csv", getpid());
    FILE *f = fopen(path, "w");
    if (!f) return NULL;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return path;
}

static int count_open_fds(void) {
    int count = 0;
    for (int fd = 0; fd < 1024; fd++) {
        if (fcntl(fd, F_GETFD) != -1) {
            count++;
        }
    }
    return count;
}

// Test: Config initialization
void test_config_init(void) {
    TEST("config initialization");

    cisv_config config;
    cisv_config_init(&config);

    if (config.delimiter == ',' &&
        config.quote == '"' &&
        config.from_line == 1) {
        PASS();
    } else {
        FAIL("default config values incorrect");
    }
}

// Test: Parser creation/destruction
void test_parser_lifecycle(void) {
    TEST("parser lifecycle");

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) {
        FAIL("failed to create parser");
        return;
    }

    cisv_parser_destroy(parser);
    PASS();
}

// Test: Parse simple CSV string
void test_parse_simple(void) {
    TEST("parse simple CSV");

    field_count = 0;
    row_count = 0;

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) {
        FAIL("failed to create parser");
        return;
    }

    const char *csv = "a,b,c\n1,2,3\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);

    cisv_parser_destroy(parser);

    if (field_count == 6 && row_count == 2) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 6 fields, 2 rows; got %d fields, %d rows",
                 field_count, row_count);
        FAIL(buf);
    }
}

// Test: Parse with custom delimiter
void test_parse_custom_delimiter(void) {
    TEST("parse with custom delimiter");

    field_count = 0;
    row_count = 0;

    cisv_config config;
    cisv_config_init(&config);
    config.delimiter = ';';
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) {
        FAIL("failed to create parser");
        return;
    }

    const char *csv = "a;b;c\n1;2;3\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);

    cisv_parser_destroy(parser);

    if (field_count == 6 && row_count == 2) {
        PASS();
    } else {
        FAIL("incorrect field/row count");
    }
}

// Test: Parse quoted fields
void test_parse_quoted(void) {
    TEST("parse quoted fields");

    field_count = 0;
    row_count = 0;
    memset(last_field, 0, sizeof(last_field));

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) {
        FAIL("failed to create parser");
        return;
    }

    const char *csv = "\"hello, world\",b\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);

    cisv_parser_destroy(parser);

    if (field_count == 2 && row_count == 1) {
        PASS();
    } else {
        FAIL("incorrect field/row count for quoted");
    }
}

// Test: Transformer uppercase
void test_transform_uppercase(void) {
    TEST("transform uppercase");

    cisv_transform_result_t result = cisv_transform_uppercase("hello", 5, NULL);

    if (result.len == 5 && strcmp(result.data, "HELLO") == 0) {
        PASS();
    } else {
        FAIL("uppercase transform failed");
    }

    cisv_transform_result_free(&result);
}

// Test: Transformer lowercase
void test_transform_lowercase(void) {
    TEST("transform lowercase");

    cisv_transform_result_t result = cisv_transform_lowercase("WORLD", 5, NULL);

    if (result.len == 5 && strcmp(result.data, "world") == 0) {
        PASS();
    } else {
        FAIL("lowercase transform failed");
    }

    cisv_transform_result_free(&result);
}

// Test: Transformer trim
void test_transform_trim(void) {
    TEST("transform trim");

    cisv_transform_result_t result = cisv_transform_trim("  hello  ", 9, NULL);

    if (result.len == 5 && strcmp(result.data, "hello") == 0) {
        PASS();
    } else {
        FAIL("trim transform failed");
    }

    cisv_transform_result_free(&result);
}

// Test: Transform pipeline
void test_transform_pipeline(void) {
    TEST("transform pipeline");

    cisv_transform_pipeline_t *pipeline = cisv_transform_pipeline_create(4);
    if (!pipeline) {
        FAIL("failed to create pipeline");
        return;
    }

    cisv_transform_pipeline_add(pipeline, 0, TRANSFORM_UPPERCASE, NULL);
    cisv_transform_pipeline_add(pipeline, 1, TRANSFORM_LOWERCASE, NULL);

    cisv_transform_result_t r1 = cisv_transform_apply(pipeline, 0, "hello", 5);
    cisv_transform_result_t r2 = cisv_transform_apply(pipeline, 1, "WORLD", 5);

    int success = (strcmp(r1.data, "HELLO") == 0 && strcmp(r2.data, "world") == 0);

    cisv_transform_result_free(&r1);
    cisv_transform_result_free(&r2);
    cisv_transform_pipeline_destroy(pipeline);

    if (success) {
        PASS();
    } else {
        FAIL("pipeline transforms incorrect");
    }
}

// Test: Writer basic
void test_writer_basic(void) {
    TEST("writer basic");

    FILE *tmp = tmpfile();
    if (!tmp) {
        FAIL("failed to create temp file");
        return;
    }

    cisv_writer *writer = cisv_writer_create(tmp);
    if (!writer) {
        fclose(tmp);
        FAIL("failed to create writer");
        return;
    }

    cisv_writer_field_str(writer, "a");
    cisv_writer_field_str(writer, "b");
    cisv_writer_field_str(writer, "c");
    cisv_writer_row_end(writer);

    cisv_writer_field_int(writer, 1);
    cisv_writer_field_int(writer, 2);
    cisv_writer_field_int(writer, 3);
    cisv_writer_row_end(writer);

    cisv_writer_flush(writer);

    // Read back
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t len = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[len] = '\0';

    cisv_writer_destroy(writer);
    fclose(tmp);

    if (strcmp(buf, "a,b,c\n1,2,3\n") == 0) {
        PASS();
    } else {
        FAIL("writer output mismatch");
    }
}

// Test: Writer with quoting
void test_writer_quoting(void) {
    TEST("writer quoting");

    FILE *tmp = tmpfile();
    if (!tmp) {
        FAIL("failed to create temp file");
        return;
    }

    cisv_writer *writer = cisv_writer_create(tmp);
    if (!writer) {
        fclose(tmp);
        FAIL("failed to create writer");
        return;
    }

    cisv_writer_field_str(writer, "hello, world");
    cisv_writer_field_str(writer, "normal");
    cisv_writer_row_end(writer);

    cisv_writer_flush(writer);

    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t len = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[len] = '\0';

    cisv_writer_destroy(writer);
    fclose(tmp);

    if (strcmp(buf, "\"hello, world\",normal\n") == 0) {
        PASS();
    } else {
        char msg[300];
        snprintf(msg, sizeof(msg), "got: %s", buf);
        FAIL(msg);
    }
}

// Test: Base64 encode
void test_base64_encode(void) {
    TEST("base64 encode");

    cisv_transform_result_t result = cisv_transform_base64_encode("Hello", 5, NULL);

    if (strcmp(result.data, "SGVsbG8=") == 0) {
        PASS();
    } else {
        FAIL("base64 encode failed");
    }

    cisv_transform_result_free(&result);
}

// =============================================================================
// Multiline CSV Tests (Issue #108)
// =============================================================================

void test_parse_multiline_quoted(void) {
    TEST("parse multiline quoted field");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    const char *csv = "a,b\n\"line1\nline2\",c\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 4 && row_count == 2) {
        // Verify the multiline field content
        if (strcmp(stored_fields[2], "line1\nline2") == 0) {
            PASS();
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "multiline field mismatch: got '%s'", stored_fields[2]);
            FAIL(buf);
        }
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 4 fields, 2 rows; got %d fields, %d rows",
                 field_count, row_count);
        FAIL(buf);
    }
}

void test_parse_multiline_row_count(void) {
    TEST("multiline row count via cisv_parser_count_rows");

    const char *csv = "h1,h2\n\"line1\nline2\nline3\",simple\n";
    const char *path = write_temp_csv(csv);
    if (!path) { FAIL("failed to create temp file"); return; }

    size_t count = cisv_parser_count_rows(path);
    unlink(path);

    if (count == 2) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 2, got %zu", count);
        FAIL(buf);
    }
}

void test_parse_multiline_multiple_fields(void) {
    TEST("multiple multiline fields in same row");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    const char *csv = "a,b,c\n\"x\ny\",middle,\"p\nq\nr\"\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 6 && row_count == 2) {
        if (strcmp(stored_fields[3], "x\ny") == 0 &&
            strcmp(stored_fields[4], "middle") == 0 &&
            strcmp(stored_fields[5], "p\nq\nr") == 0) {
            PASS();
        } else {
            FAIL("field content mismatch");
        }
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 6 fields, 2 rows; got %d fields, %d rows",
                 field_count, row_count);
        FAIL(buf);
    }
}

void test_parse_multiline_escaped_quotes(void) {
    TEST("multiline with escaped quotes");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    // Field: she said "hi"\nthen left
    const char *csv = "a,b\n\"she said \"\"hi\"\"\nthen left\",ok\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 4 && row_count == 2) {
        if (strcmp(stored_fields[2], "she said \"hi\"\nthen left") == 0) {
            PASS();
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "field mismatch: got '%s'", stored_fields[2]);
            FAIL(buf);
        }
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 4 fields, 2 rows; got %d fields, %d rows",
                 field_count, row_count);
        FAIL(buf);
    }
}

void test_parse_multiline_crlf(void) {
    TEST("multiline with CRLF inside quotes");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    const char *csv = "a,b\r\n\"line1\r\nline2\",c\r\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 4 && row_count == 2) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 4 fields, 2 rows; got %d fields, %d rows",
                 field_count, row_count);
        FAIL(buf);
    }
}

void test_parse_multiline_empty_lines(void) {
    TEST("consecutive newlines inside quotes");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    const char *csv = "a,b\n\"line1\n\n\nline4\",c\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 4 && row_count == 2) {
        if (strcmp(stored_fields[2], "line1\n\n\nline4") == 0) {
            PASS();
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "field mismatch: got '%s'", stored_fields[2]);
            FAIL(buf);
        }
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 4 fields, 2 rows; got %d fields, %d rows",
                 field_count, row_count);
        FAIL(buf);
    }
}

void test_parse_multiline_issue108(void) {
    TEST("issue #108: many multiline fields");

    // Reproduce issue: 1 header + 1 data row with many multiline quoted fields
    // The old bug would count raw \n chars and return 57 instead of 2
    const char *csv =
        "\"h1\",\"h2\",\"h3\",\"h4\",\"h5\"\n"
        "\"a\nb\nc\nd\ne\nf\ng\nh\ni\nj\","
        "\"k\nl\nm\nn\no\np\nq\nr\ns\nt\","
        "\"u\nv\nw\nx\ny\nz\n1\n2\n3\n4\","
        "\"5\n6\n7\n8\n9\n0\na\nb\nc\nd\","
        "\"e\nf\ng\nh\ni\nj\nk\nl\nm\nn\"\n";

    const char *path = write_temp_csv(csv);
    if (!path) { FAIL("failed to create temp file"); return; }

    size_t count = cisv_parser_count_rows(path);
    unlink(path);

    if (count == 2) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 2, got %zu (old bug returned 57)", count);
        FAIL(buf);
    }
}

void test_count_rows_with_config_custom_quote(void) {
    TEST("count_rows_with_config custom quote char");

    // Use ' as quote char instead of "
    const char *csv = "h1,h2\n'line1\nline2',simple\n";
    const char *path = write_temp_csv(csv);
    if (!path) { FAIL("failed to create temp file"); return; }

    cisv_config config;
    cisv_config_init(&config);
    config.quote = '\'';

    size_t count = cisv_parser_count_rows_with_config(path, &config);
    unlink(path);

    if (count == 2) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 2, got %zu", count);
        FAIL(buf);
    }
}

void test_parser_reuse_no_fd_leak(void) {
    TEST("parser reuse does not leak file descriptors");

    const char *csv = "a,b\n1,2\n";
    const char *path = write_temp_csv(csv);
    if (!path) { FAIL("failed to create temp file"); return; }

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { unlink(path); FAIL("failed to create parser"); return; }

    int before = count_open_fds();
    int ok = 1;
    for (int i = 0; i < 25; i++) {
        if (cisv_parser_parse_file(parser, path) < 0) {
            ok = 0;
            break;
        }
    }
    int after = count_open_fds();

    cisv_parser_destroy(parser);
    unlink(path);

    if (ok && after == before) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "fd leak detected: before=%d after=%d", before, after);
        FAIL(buf);
    }
}

void test_streaming_chunk_boundaries(void) {
    TEST("streaming parse across chunk boundaries");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    const char *chunk1 = "a,b";
    const char *chunk2 = "\n1,2";

    cisv_parser_write(parser, (const uint8_t *)chunk1, strlen(chunk1));
    cisv_parser_write(parser, (const uint8_t *)chunk2, strlen(chunk2));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 4 && row_count == 2 &&
        strcmp(stored_fields[0], "a") == 0 &&
        strcmp(stored_fields[1], "b") == 0 &&
        strcmp(stored_fields[2], "1") == 0 &&
        strcmp(stored_fields[3], "2") == 0) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 4 fields/2 rows, got %d/%d", field_count, row_count);
        FAIL(buf);
    }
}

void test_parse_comment_lines(void) {
    TEST("parse with comment line prefix");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.comment = '#';
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    const char *csv = "#meta\na,b\n#ignore this line\n1,2\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 4 && row_count == 2 &&
        strcmp(stored_fields[0], "a") == 0 &&
        strcmp(stored_fields[1], "b") == 0 &&
        strcmp(stored_fields[2], "1") == 0 &&
        strcmp(stored_fields[3], "2") == 0) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected comments skipped, got fields=%d rows=%d", field_count, row_count);
        FAIL(buf);
    }
}

void test_max_row_size_skip_error_lines(void) {
    TEST("max_row_size with skip_lines_with_error");
    reset_test_state();

    cisv_config config;
    cisv_config_init(&config);
    config.max_row_size = 10;
    config.skip_lines_with_error = true;
    config.field_cb = test_field_cb;
    config.row_cb = test_row_cb;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) { FAIL("failed to create parser"); return; }

    // Middle row is intentionally oversized and should be skipped.
    const char *csv = "a,b\nverylongfield,2\n1,2\n";
    cisv_parser_write(parser, (const uint8_t *)csv, strlen(csv));
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    if (field_count == 4 && row_count == 2 &&
        strcmp(stored_fields[0], "a") == 0 &&
        strcmp(stored_fields[1], "b") == 0 &&
        strcmp(stored_fields[2], "1") == 0 &&
        strcmp(stored_fields[3], "2") == 0) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected oversized row skipped, got fields=%d rows=%d", field_count, row_count);
        FAIL(buf);
    }
}

void test_parallel_custom_quote_chunk_split(void) {
    TEST("parallel parse uses custom quote in chunk splitting");

    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_cisv_parallel_quote_%d.csv", getpid());
    FILE *f = fopen(path, "w");
    if (!f) {
        FAIL("failed to create temp file");
        return;
    }

    fprintf(f, "id|text|flag\n");
    for (int i = 0; i < 1200; i++) {
        if (i == 600) {
            fprintf(f, "%d|'line1\nline2'|ok\n", i);
        } else {
            fprintf(f, "%d|'value_%d'|ok\n", i, i);
        }
    }
    fclose(f);

    cisv_config config;
    cisv_config_init(&config);
    config.delimiter = '|';
    config.quote = '\'';

    int result_count = 0;
    cisv_result_t **results = cisv_parse_file_parallel(path, &config, 2, &result_count);
    if (!results || result_count <= 0) {
        unlink(path);
        FAIL("parallel parse failed");
        return;
    }

    size_t total_rows = 0;
    size_t total_fields = 0;
    int found_multiline = 0;
    int had_error = 0;

    for (int i = 0; i < result_count; i++) {
        cisv_result_t *r = results[i];
        if (!r) {
            had_error = 1;
            continue;
        }
        if (r->error_code != 0) {
            had_error = 1;
        }
        total_rows += r->row_count;
        total_fields += r->total_fields;

        for (size_t row_idx = 0; row_idx < r->row_count; row_idx++) {
            cisv_row_t *row = &r->rows[row_idx];
            for (size_t col = 0; col < row->field_count; col++) {
                if (row->field_lengths[col] == 11 &&
                    memcmp(row->fields[col], "line1\nline2", 11) == 0) {
                    found_multiline = 1;
                }
            }
        }
    }

    cisv_results_free(results, result_count);
    unlink(path);

    // header + 1200 data rows
    if (!had_error && total_rows == 1201 && total_fields == 3603 && found_multiline) {
        PASS();
    } else {
        char buf[200];
        snprintf(buf, sizeof(buf),
                 "expected rows=1201 fields=3603 multiline=1, got rows=%zu fields=%zu multiline=%d error=%d",
                 total_rows, total_fields, found_multiline, had_error);
        FAIL(buf);
    }
}

int main(void) {
    printf("CISV Core Library Tests\n");
    printf("========================\n\n");

    // Parser tests
    printf("Parser Tests:\n");
    test_config_init();
    test_parser_lifecycle();
    test_parse_simple();
    test_parse_custom_delimiter();
    test_parse_quoted();

    // Transformer tests
    printf("\nTransformer Tests:\n");
    test_transform_uppercase();
    test_transform_lowercase();
    test_transform_trim();
    test_transform_pipeline();
    test_base64_encode();

    // Writer tests
    printf("\nWriter Tests:\n");
    test_writer_basic();
    test_writer_quoting();

    // Multiline tests (issue #108)
    printf("\nMultiline Tests (Issue #108):\n");
    test_parse_multiline_quoted();
    test_parse_multiline_row_count();
    test_parse_multiline_multiple_fields();
    test_parse_multiline_escaped_quotes();
    test_parse_multiline_crlf();
    test_parse_multiline_empty_lines();
    test_parse_multiline_issue108();
    test_count_rows_with_config_custom_quote();
    test_parser_reuse_no_fd_leak();
    test_streaming_chunk_boundaries();
    test_parse_comment_lines();
    test_max_row_size_skip_error_lines();
    test_parallel_custom_quote_chunk_split();

    // Summary
    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
