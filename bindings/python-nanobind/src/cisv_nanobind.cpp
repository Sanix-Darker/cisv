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
#include <nanobind/ndarray.h>
#include <stdexcept>
#include <string>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

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
 * Ultra-fast parallel parsing that returns raw data for maximum performance.
 * Returns a tuple of (data_bytes, field_offsets, field_lengths, row_offsets)
 * where field data can be extracted via: data[offset:offset+length].decode()
 *
 * This avoids creating millions of Python string objects upfront.
 */
static nb::tuple parse_file_raw(
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

    // Setup config
    cisv_config config;
    cisv_config_init(&config);
    config.delimiter = delimiter[0];
    config.quote = quote[0];
    config.trim = trim;
    config.skip_empty_lines = skip_empty_lines;

    // Parse file with parallel processing
    int result_count = 0;
    cisv_result_t **results = nullptr;

    {
        nb::gil_scoped_release release;
        results = cisv_parse_file_parallel(path.c_str(), &config,
                                           num_threads > 0 ? num_threads : 0, &result_count);
    }

    if (!results) {
        throw std::runtime_error("Failed to parse file: " + std::string(strerror(errno)));
    }

    // Calculate total sizes
    size_t total_data_size = 0;
    size_t total_fields = 0;
    size_t total_rows = 0;

    for (int chunk = 0; chunk < result_count; chunk++) {
        if (!results[chunk]) continue;
        if (results[chunk]->error_code != 0) {
            std::string msg = results[chunk]->error_message;
            cisv_results_free(results, result_count);
            throw std::runtime_error(msg);
        }
        total_data_size += results[chunk]->field_data_size;
        total_fields += results[chunk]->total_fields;
        total_rows += results[chunk]->row_count;
    }

    // Allocate arrays
    uint8_t *data_buf = new uint8_t[total_data_size];
    uint64_t *field_offsets = new uint64_t[total_fields];
    uint32_t *field_lengths = new uint32_t[total_fields];
    uint64_t *row_offsets = new uint64_t[total_rows + 1];  // +1 for end sentinel

    // Parallel data combining using OpenMP-style threading
    // First pass: calculate per-chunk offsets
    std::vector<size_t> chunk_data_offsets(result_count + 1);
    std::vector<size_t> chunk_field_offsets(result_count + 1);
    std::vector<size_t> chunk_row_offsets(result_count + 1);

    chunk_data_offsets[0] = 0;
    chunk_field_offsets[0] = 0;
    chunk_row_offsets[0] = 0;

    for (int chunk = 0; chunk < result_count; chunk++) {
        cisv_result_t *r = results[chunk];
        if (r) {
            chunk_data_offsets[chunk + 1] = chunk_data_offsets[chunk] + r->field_data_size;
            chunk_field_offsets[chunk + 1] = chunk_field_offsets[chunk] + r->total_fields;
            chunk_row_offsets[chunk + 1] = chunk_row_offsets[chunk] + r->row_count;
        } else {
            chunk_data_offsets[chunk + 1] = chunk_data_offsets[chunk];
            chunk_field_offsets[chunk + 1] = chunk_field_offsets[chunk];
            chunk_row_offsets[chunk + 1] = chunk_row_offsets[chunk];
        }
    }

    // Second pass: parallel copy using threads
    std::vector<std::thread> workers;
    workers.reserve(result_count);

    for (int chunk = 0; chunk < result_count; chunk++) {
        cisv_result_t *r = results[chunk];
        if (!r) continue;

        workers.emplace_back([=, &data_buf, &field_offsets, &field_lengths, &row_offsets]() {
            size_t data_pos = chunk_data_offsets[chunk];
            size_t field_idx = chunk_field_offsets[chunk];
            size_t row_idx = chunk_row_offsets[chunk];

            // Copy field data
            memcpy(data_buf + data_pos, r->field_data, r->field_data_size);

            // Build indices for this chunk
            for (size_t i = 0; i < r->row_count; i++) {
                row_offsets[row_idx++] = field_idx;
                cisv_row_t *row = &r->rows[i];

                for (size_t j = 0; j < row->field_count; j++) {
                    size_t field_offset_in_chunk = row->fields[j] - r->field_data;
                    field_offsets[field_idx] = data_pos + field_offset_in_chunk;
                    field_lengths[field_idx] = (uint32_t)row->field_lengths[j];
                    field_idx++;
                }
            }
        });
    }

    // Wait for all workers
    for (auto &w : workers) {
        w.join();
    }

    row_offsets[total_rows] = total_fields;  // End sentinel

    cisv_results_free(results, result_count);

    // Create numpy arrays that own the data
    nb::capsule data_owner(data_buf, [](void *p) noexcept { delete[] (uint8_t*)p; });
    nb::capsule offsets_owner(field_offsets, [](void *p) noexcept { delete[] (uint64_t*)p; });
    nb::capsule lengths_owner(field_lengths, [](void *p) noexcept { delete[] (uint32_t*)p; });
    nb::capsule rows_owner(row_offsets, [](void *p) noexcept { delete[] (uint64_t*)p; });

    size_t data_shape[1] = {total_data_size};
    size_t fields_shape[1] = {total_fields};
    size_t rows_shape[1] = {total_rows + 1};

    auto data_arr = nb::ndarray<nb::numpy, uint8_t, nb::shape<-1>>(
        data_buf, 1, data_shape, data_owner);
    auto offsets_arr = nb::ndarray<nb::numpy, uint64_t, nb::shape<-1>>(
        field_offsets, 1, fields_shape, offsets_owner);
    auto lengths_arr = nb::ndarray<nb::numpy, uint32_t, nb::shape<-1>>(
        field_lengths, 1, fields_shape, lengths_owner);
    auto rows_arr = nb::ndarray<nb::numpy, uint64_t, nb::shape<-1>>(
        row_offsets, 1, rows_shape, rows_owner);

    return nb::make_tuple(data_arr, offsets_arr, lengths_arr, rows_arr);
}

/**
 * Ultra-fast parallel parsing that only returns row/field counts.
 * This is used for benchmarking raw parsing speed without data marshaling.
 */
static nb::tuple parse_file_count_only(
    const std::string &path,
    int num_threads = 0,
    const std::string &delimiter = ",",
    const std::string &quote = "\""
) {
    if (path.empty()) {
        throw std::invalid_argument("Path cannot be empty");
    }

    // Setup config
    cisv_config config;
    cisv_config_init(&config);
    config.delimiter = delimiter[0];
    config.quote = quote[0];

    // Parse file with parallel processing
    int result_count = 0;
    cisv_result_t **results = nullptr;

    {
        nb::gil_scoped_release release;
        results = cisv_parse_file_parallel(path.c_str(), &config,
                                           num_threads > 0 ? num_threads : 0, &result_count);
    }

    if (!results) {
        throw std::runtime_error("Failed to parse file: " + std::string(strerror(errno)));
    }

    // Just count totals, don't copy data
    size_t total_fields = 0;
    size_t total_rows = 0;
    size_t first_row_cols = 0;

    for (int chunk = 0; chunk < result_count; chunk++) {
        if (!results[chunk]) continue;
        if (results[chunk]->error_code != 0) {
            std::string msg = results[chunk]->error_message;
            cisv_results_free(results, result_count);
            throw std::runtime_error(msg);
        }
        if (total_rows == 0 && results[chunk]->row_count > 0) {
            first_row_cols = results[chunk]->rows[0].field_count;
        }
        total_fields += results[chunk]->total_fields;
        total_rows += results[chunk]->row_count;
    }

    cisv_results_free(results, result_count);

    return nb::make_tuple(total_rows, total_fields, first_row_cols);
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

/**
 * Row-by-row iterator for streaming CSV parsing.
 *
 * Provides fgetcsv-style iteration with minimal memory footprint.
 * Supports early exit - no wasted work when breaking mid-file.
 */
class CisvIterator {
private:
    cisv_iterator_t *it_;
    bool closed_;
    std::string path_;

public:
    CisvIterator(const std::string &path,
                 const std::string &delimiter = ",",
                 const std::string &quote = "\"",
                 bool trim = false,
                 bool skip_empty_lines = false)
        : it_(nullptr), closed_(false), path_(path)
    {
        if (path.empty()) {
            throw std::invalid_argument("Path cannot be empty");
        }
        if (delimiter.empty() || delimiter.size() > 1) {
            throw std::invalid_argument("Delimiter must be a single character");
        }
        if (quote.empty() || quote.size() > 1) {
            throw std::invalid_argument("Quote must be a single character");
        }

        cisv_config config;
        cisv_config_init(&config);
        config.delimiter = delimiter[0];
        config.quote = quote[0];
        config.trim = trim;
        config.skip_empty_lines = skip_empty_lines;

        it_ = cisv_iterator_open(path.c_str(), &config);
        if (!it_) {
            throw std::runtime_error("Failed to open file: " + path);
        }
    }

    ~CisvIterator() {
        close();
    }

    // Disable copy
    CisvIterator(const CisvIterator&) = delete;
    CisvIterator& operator=(const CisvIterator&) = delete;

    /**
     * Get the next row, or None if at end of file.
     */
    nb::object next() {
        if (closed_ || !it_) {
            return nb::none();
        }

        const char **fields;
        const size_t *lengths;
        size_t field_count;

        int result = cisv_iterator_next(it_, &fields, &lengths, &field_count);

        if (result == CISV_ITER_EOF) {
            return nb::none();
        }
        if (result == CISV_ITER_ERROR) {
            throw std::runtime_error("Error reading CSV row from: " + path_);
        }

        nb::list row;
        for (size_t i = 0; i < field_count; i++) {
            row.append(nb::str(fields[i], lengths[i]));
        }
        return row;
    }

    /**
     * Close the iterator and release resources.
     */
    void close() {
        if (!closed_ && it_) {
            cisv_iterator_close(it_);
            it_ = nullptr;
            closed_ = true;
        }
    }

    /**
     * Check if the iterator is closed.
     */
    bool is_closed() const {
        return closed_;
    }

    // Python iterator protocol - __iter__
    CisvIterator& iter() {
        return *this;
    }

    // Python iterator protocol - __next__
    nb::object iternext() {
        nb::object row = next();
        if (row.is_none()) {
            throw nb::stop_iteration();
        }
        return row;
    }

    // Context manager protocol - __enter__
    CisvIterator& enter() {
        return *this;
    }

    // Context manager protocol - __exit__
    // Note: Returns false to indicate exception should propagate (if any)
    bool exit(nb::object /*exc_type*/, nb::object /*exc_val*/, nb::object /*exc_tb*/) {
        close();
        return false;  // Don't suppress exceptions
    }
};

// Module definition
NB_MODULE(_core, m) {
    m.doc() = "High-performance CSV parser with SIMD optimizations";

    // Register the CisvIterator class
    nb::class_<CisvIterator>(m, "CisvIterator",
        "Row-by-row iterator for streaming CSV parsing.\n\n"
        "Provides fgetcsv-style iteration with minimal memory footprint.\n"
        "Supports early exit - no wasted work when breaking mid-file.\n\n"
        "Example:\n"
        "    with CisvIterator('/path/to/file.csv') as reader:\n"
        "        for row in reader:\n"
        "            print(row)  # List[str]\n"
        "            if row[0] == 'stop':\n"
        "                break  # Early exit - no wasted work")
        .def(nb::init<const std::string &, const std::string &,
                      const std::string &, bool, bool>(),
             nb::arg("path"),
             nb::arg("delimiter") = ",",
             nb::arg("quote") = "\"",
             nb::arg("trim") = false,
             nb::arg("skip_empty_lines") = false,
             "Create a new CSV iterator.\n\n"
             "Args:\n"
             "    path: Path to the CSV file\n"
             "    delimiter: Field delimiter character (default: ',')\n"
             "    quote: Quote character (default: '\"')\n"
             "    trim: Whether to trim whitespace from fields\n"
             "    skip_empty_lines: Whether to skip empty lines")
        .def("next", &CisvIterator::next,
             "Get the next row as a list of strings, or None if at end of file.")
        .def("close", &CisvIterator::close,
             "Close the iterator and release resources.")
        .def_prop_ro("closed", &CisvIterator::is_closed,
             "Whether the iterator has been closed.")
        .def("__iter__", &CisvIterator::iter, nb::rv_policy::reference)
        .def("__next__", &CisvIterator::iternext)
        .def("__enter__", &CisvIterator::enter, nb::rv_policy::reference)
        .def("__exit__", [](CisvIterator &self, nb::args) {
            self.close();
            return false;  // Don't suppress exceptions
        });

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

    m.def("parse_file_raw", &parse_file_raw,
          nb::arg("path"),
          nb::arg("num_threads") = 0,
          nb::arg("delimiter") = ",",
          nb::arg("quote") = "\"",
          nb::arg("trim") = false,
          nb::arg("skip_empty_lines") = false,
          "Ultra-fast parallel parsing that returns raw numpy arrays.\n\n"
          "This is the fastest parsing mode, avoiding Python string creation.\n"
          "Returns (data, field_offsets, field_lengths, row_offsets) where:\n"
          "  - data: uint8 array of all field bytes\n"
          "  - field_offsets: uint64 array of byte offsets for each field\n"
          "  - field_lengths: uint32 array of byte lengths for each field\n"
          "  - row_offsets: uint64 array of field indices for each row\n\n"
          "To access field i: data[field_offsets[i]:field_offsets[i]+field_lengths[i]].tobytes().decode()\n"
          "To access row r fields: range(row_offsets[r], row_offsets[r+1])");

    m.def("parse_file_count_only", &parse_file_count_only,
          nb::arg("path"),
          nb::arg("num_threads") = 0,
          nb::arg("delimiter") = ",",
          nb::arg("quote") = "\"",
          "Ultra-fast parallel parsing that only returns counts (for benchmarking).\n\n"
          "Returns (row_count, field_count, first_row_cols) without data marshaling.");

    m.def("count_rows", &count_rows,
          nb::arg("path"),
          "Count the number of rows in a CSV file without full parsing.\n\n"
          "This is very fast as it only scans for newlines using SIMD.\n\n"
          "Args:\n"
          "    path: Path to the CSV file\n\n"
          "Returns:\n"
          "    Number of rows in the file");
}
