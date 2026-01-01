#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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

static void test_field_cb(void *user, const char *data, size_t len) {
    (void)user;
    field_count++;
    if (len < sizeof(last_field)) {
        memcpy(last_field, data, len);
        last_field[len] = '\0';
    }
}

static void test_row_cb(void *user) {
    (void)user;
    row_count++;
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

    // Summary
    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
