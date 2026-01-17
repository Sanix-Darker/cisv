/**
 * CISV Python Bindings using nanobind
 *
 * High-performance bindings that use the batch API to eliminate
 * per-field callback overhead. This provides 10-100x speedup over
 * ctypes-based bindings.
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <stdexcept>
#include <string>

extern "C" {
#include "cisv/parser.h"
}

namespace nb = nanobind;

/**
 * Parse a CSV file and return all rows at once.
 *
 * Uses the batch API to collect all data in C, then converts to Python
 * in a single pass. This eliminates the per-field callback overhead
 * that makes ctypes bindings slow.
 */
static nb::list parse_file(
    const std::string &path,
    const std::string &delimiter = ",",
    const std::string &quote = "\"",
    bool trim = false,
    bool skip_empty_lines = false
) {
    // Validate inputs
    if (path.empty()) {
        throw std::invalid_argument("Path cannot be empty");
    }
    if (delimiter.empty() || delimiter.size() > 1) {
        throw std::invalid_argument("Delimiter must be a single character");
    }
    if (quote.empty() || quote.size() > 1) {
        throw std::invalid_argument("Quote must be a single character");
    }

    // Setup config
    cisv_config config;
    cisv_config_init(&config);
    config.delimiter = delimiter[0];
    config.quote = quote[0];
    config.trim = trim;
    config.skip_empty_lines = skip_empty_lines;

    // Parse file using batch API
    cisv_result_t *result = cisv_parse_file_batch(path.c_str(), &config);

    if (!result) {
        throw std::runtime_error("Failed to parse file: " + std::string(strerror(errno)));
    }

    if (result->error_code != 0) {
        std::string msg = result->error_message;
        cisv_result_free(result);
        throw std::runtime_error(msg);
    }

    // Convert to Python list (single conversion pass)
    nb::list rows;
    for (size_t i = 0; i < result->row_count; i++) {
        nb::list row;
        cisv_row_t *r = &result->rows[i];
        for (size_t j = 0; j < r->field_count; j++) {
            // Create Python string from field data
            row.append(nb::str(r->fields[j], r->field_lengths[j]));
        }
        rows.append(row);
    }

    cisv_result_free(result);
    return rows;
}

/**
 * Parse a CSV string and return all rows at once.
 */
static nb::list parse_string(
    const std::string &data,
    const std::string &delimiter = ",",
    const std::string &quote = "\"",
    bool trim = false,
    bool skip_empty_lines = false
) {
    if (delimiter.empty() || delimiter.size() > 1) {
        throw std::invalid_argument("Delimiter must be a single character");
    }
    if (quote.empty() || quote.size() > 1) {
        throw std::invalid_argument("Quote must be a single character");
    }

    // Setup config
    cisv_config config;
    cisv_config_init(&config);
    config.delimiter = delimiter[0];
    config.quote = quote[0];
    config.trim = trim;
    config.skip_empty_lines = skip_empty_lines;

    // Parse string using batch API
    cisv_result_t *result = cisv_parse_string_batch(data.c_str(), data.size(), &config);

    if (!result) {
        throw std::runtime_error("Failed to parse string: " + std::string(strerror(errno)));
    }

    if (result->error_code != 0) {
        std::string msg = result->error_message;
        cisv_result_free(result);
        throw std::runtime_error(msg);
    }

    // Convert to Python list
    nb::list rows;
    for (size_t i = 0; i < result->row_count; i++) {
        nb::list row;
        cisv_row_t *r = &result->rows[i];
        for (size_t j = 0; j < r->field_count; j++) {
            row.append(nb::str(r->fields[j], r->field_lengths[j]));
        }
        rows.append(row);
    }

    cisv_result_free(result);
    return rows;
}

/**
 * Parse a CSV file using multiple threads for maximum performance.
 *
 * Returns a single merged list of all rows. The parsing is done in
 * parallel, but the results are merged in order.
 */
static nb::list parse_file_parallel(
    const std::string &path,
    int num_threads = 0,
    const std::string &delimiter = ",",
    const std::string &quote = "\"",
    bool trim = false,
    bool skip_empty_lines = false
) {
    if (path.empty()) {
        throw std::invalid_argument("Path cannot be empty");
    }
    if (delimiter.empty() || delimiter.size() > 1) {
        throw std::invalid_argument("Delimiter must be a single character");
    }
    if (quote.empty() || quote.size() > 1) {
        throw std::invalid_argument("Quote must be a single character");
    }

    // Setup config
    cisv_config config;
    cisv_config_init(&config);
    config.delimiter = delimiter[0];
    config.quote = quote[0];
    config.trim = trim;
    config.skip_empty_lines = skip_empty_lines;

    // Release GIL during C parsing
    int result_count = 0;
    cisv_result_t **results = nullptr;

    {
        nb::gil_scoped_release release;
        results = cisv_parse_file_parallel(path.c_str(), &config, num_threads, &result_count);
    }

    if (!results) {
        throw std::runtime_error("Failed to parse file in parallel: " + std::string(strerror(errno)));
    }

    // Merge all results into a single list
    nb::list rows;
    for (int chunk = 0; chunk < result_count; chunk++) {
        cisv_result_t *result = results[chunk];
        if (!result) continue;

        if (result->error_code != 0) {
            std::string msg = result->error_message;
            cisv_results_free(results, result_count);
            throw std::runtime_error(msg);
        }

        for (size_t i = 0; i < result->row_count; i++) {
            nb::list row;
            cisv_row_t *r = &result->rows[i];
            for (size_t j = 0; j < r->field_count; j++) {
                row.append(nb::str(r->fields[j], r->field_lengths[j]));
            }
            rows.append(row);
        }
    }

    cisv_results_free(results, result_count);
    return rows;
}

/**
 * Count the number of rows in a CSV file without full parsing.
 * This is very fast as it only scans for newlines.
 */
static size_t count_rows(const std::string &path) {
    if (path.empty()) {
        throw std::invalid_argument("Path cannot be empty");
    }
    return cisv_parser_count_rows(path.c_str());
}

// Module definition
NB_MODULE(_core, m) {
    m.doc() = "High-performance CSV parser with SIMD optimizations";

    m.def("parse_file", &parse_file,
          nb::arg("path"),
          nb::arg("delimiter") = ",",
          nb::arg("quote") = "\"",
          nb::arg("trim") = false,
          nb::arg("skip_empty_lines") = false,
          "Parse a CSV file and return all rows as a list of lists.\n\n"
          "Args:\n"
          "    path: Path to the CSV file\n"
          "    delimiter: Field delimiter character (default: ',')\n"
          "    quote: Quote character (default: '\"')\n"
          "    trim: Whether to trim whitespace from fields\n"
          "    skip_empty_lines: Whether to skip empty lines\n\n"
          "Returns:\n"
          "    List of rows, where each row is a list of field values");

    m.def("parse_string", &parse_string,
          nb::arg("data"),
          nb::arg("delimiter") = ",",
          nb::arg("quote") = "\"",
          nb::arg("trim") = false,
          nb::arg("skip_empty_lines") = false,
          "Parse a CSV string and return all rows as a list of lists.\n\n"
          "Args:\n"
          "    data: CSV content as a string\n"
          "    delimiter: Field delimiter character (default: ',')\n"
          "    quote: Quote character (default: '\"')\n"
          "    trim: Whether to trim whitespace from fields\n"
          "    skip_empty_lines: Whether to skip empty lines\n\n"
          "Returns:\n"
          "    List of rows, where each row is a list of field values");

    m.def("parse_file_parallel", &parse_file_parallel,
          nb::arg("path"),
          nb::arg("num_threads") = 0,
          nb::arg("delimiter") = ",",
          nb::arg("quote") = "\"",
          nb::arg("trim") = false,
          nb::arg("skip_empty_lines") = false,
          "Parse a CSV file using multiple threads for maximum performance.\n\n"
          "Args:\n"
          "    path: Path to the CSV file\n"
          "    num_threads: Number of threads to use (0 = auto-detect)\n"
          "    delimiter: Field delimiter character (default: ',')\n"
          "    quote: Quote character (default: '\"')\n"
          "    trim: Whether to trim whitespace from fields\n"
          "    skip_empty_lines: Whether to skip empty lines\n\n"
          "Returns:\n"
          "    List of rows, where each row is a list of field values");

    m.def("count_rows", &count_rows,
          nb::arg("path"),
          "Count the number of rows in a CSV file without full parsing.\n\n"
          "This is very fast as it only scans for newlines using SIMD.\n\n"
          "Args:\n"
          "    path: Path to the CSV file\n\n"
          "Returns:\n"
          "    Number of rows in the file");
}
