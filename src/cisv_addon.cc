#include <napi.h>
#include "cisv_parser.h"
#include "cisv_transformer.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

namespace {

// Extended RowCollector that handles transforms
struct RowCollector {
    std::vector<std::string> current;
    std::vector<std::vector<std::string>> rows;
    cisv_transform_pipeline_t* pipeline;
    int current_field_index;

    // JavaScript transforms stored separately
    std::unordered_map<int, Napi::FunctionReference> js_transforms;
    Napi::Env env;

    RowCollector() : pipeline(nullptr), current_field_index(0), env(nullptr) {
        // DON'T create the pipeline here - do it lazily when needed
        pipeline = nullptr;
    }

    ~RowCollector() {
        cleanup();
    }

    void cleanup() {
        if (pipeline) {
            cisv_transform_pipeline_destroy(pipeline);
            pipeline = nullptr;
        }
        js_transforms.clear();
        rows.clear();
        current.clear();
        current_field_index = 0;
        env = nullptr;
    }

    // Lazy initialization of pipeline
    void ensurePipeline() {
        if (!pipeline) {
            pipeline = cisv_transform_pipeline_create(16);
        }
    }

    // Apply both C and JS transforms
    std::string applyTransforms(const char* data, size_t len, int field_index) {
        std::string result(data, len);

        // First apply C transforms
        if (pipeline && pipeline->count > 0) {
            cisv_transform_result_t c_result = cisv_transform_apply(
                pipeline,
                field_index,
                result.c_str(),
                result.length()
            );

            if (c_result.data) {
                result = std::string(c_result.data, c_result.len);
                // IMPORTANT: Free the result if it was allocated
                if (c_result.needs_free) {
                    cisv_transform_result_free(&c_result);
                }
            }
        }

        // Then apply JavaScript transforms if we have an environment
        if (env) {
            // Apply field-specific transform
            auto it = js_transforms.find(field_index);
            if (it != js_transforms.end() && !it->second.IsEmpty()) {
                try {
                    Napi::String input = Napi::String::New(env, result);
                    Napi::Number field = Napi::Number::New(env, field_index);

                    Napi::Value js_result = it->second.Call({input, field});

                    if (js_result.IsString()) {
                        result = js_result.As<Napi::String>().Utf8Value();
                    }
                } catch (...) {
                    // Keep original result if JS transform fails
                }
            }

            // Apply transforms that apply to all fields (-1 index)
            auto it_all = js_transforms.find(-1);
            if (it_all != js_transforms.end() && !it_all->second.IsEmpty()) {
                try {
                    Napi::String input = Napi::String::New(env, result);
                    Napi::Number field = Napi::Number::New(env, field_index);

                    Napi::Value js_result = it_all->second.Call({input, field});

                    if (js_result.IsString()) {
                        result = js_result.As<Napi::String>().Utf8Value();
                    }
                } catch (...) {
                    // Keep original result if JS transform fails
                }
            }
        }

        return result;
    }
};

static void field_cb(void *user, const char *data, size_t len) {
    auto *rc = reinterpret_cast<RowCollector *>(user);

    // Apply all transforms (C and JS)
    std::string transformed = rc->applyTransforms(data, len, rc->current_field_index);
    rc->current.emplace_back(transformed);
    rc->current_field_index++;
}

static void row_cb(void *user) {
    auto *rc = reinterpret_cast<RowCollector *>(user);
    rc->rows.emplace_back(std::move(rc->current));
    rc->current.clear();
    rc->current_field_index = 0;  // Reset field index for next row
}

} // namespace

class CisvParser : public Napi::ObjectWrap<CisvParser> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "cisvParser", {
            InstanceMethod("parseSync", &CisvParser::ParseSync),
            InstanceMethod("parse", &CisvParser::ParseAsync),
            InstanceMethod("parseString", &CisvParser::ParseString),
            InstanceMethod("write", &CisvParser::Write),
            InstanceMethod("end", &CisvParser::End),
            InstanceMethod("getRows", &CisvParser::GetRows),
            InstanceMethod("clear", &CisvParser::Clear),
            InstanceMethod("transform", &CisvParser::Transform),
            InstanceMethod("removeTransform", &CisvParser::RemoveTransform),
            InstanceMethod("clearTransforms", &CisvParser::ClearTransforms),
            InstanceMethod("getStats", &CisvParser::GetStats),
            InstanceMethod("getTransformInfo", &CisvParser::GetTransformInfo),
            InstanceMethod("destroy", &CisvParser::Destroy),
            InstanceMethod("getHeaders", &CisvParser::GetHeaders),
            InstanceMethod("setHeaders", &CisvParser::SetHeaders),
            StaticMethod("countRows", &CisvParser::CountRows)
        });

        exports.Set("cisvParser", func);
        return exports;
    }

    CisvParser(const Napi::CallbackInfo &info) : Napi::ObjectWrap<CisvParser>(info) {
        rc_ = new RowCollector();
        parser_ = cisv_parser_create(field_cb, row_cb, rc_);
        parse_time_ = 0;
        total_bytes_ = 0;
        is_destroyed_ = false;
        has_headers_ = false;
        skip_empty_lines_ = false;
        headers_processed_ = false;

        // Handle constructor options if provided
        if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object options = info[0].As<Napi::Object>();

            // Handle delimiter option
            if (options.Has("delimiter")) {
                Napi::Value delim_val = options.Get("delimiter");
                if (delim_val.IsString()) {
                    std::string delim = delim_val.As<Napi::String>();
                    if (!delim.empty()) {
                        cisv_parser_set_delimiter(parser_, delim[0]);
                    }
                } else if (delim_val.IsNumber()) {
                    // Allow numeric ASCII codes
                    char delim = static_cast<char>(delim_val.As<Napi::Number>().Int32Value());
                    cisv_parser_set_delimiter(parser_, delim);
                }
            }

            // Handle quote option
            if (options.Has("quote")) {
                Napi::Value quote_val = options.Get("quote");
                if (quote_val.IsString()) {
                    std::string quote = quote_val.As<Napi::String>();
                    if (!quote.empty()) {
                        cisv_parser_set_quote(parser_, quote[0]);
                    } else {
                        // Empty string means no quote character
                        cisv_parser_set_quote(parser_, '\0');
                    }
                } else if (quote_val.IsNull() || quote_val.IsUndefined()) {
                    // Null/undefined means no quote character
                    cisv_parser_set_quote(parser_, '\0');
                } else if (quote_val.IsNumber()) {
                    char quote = static_cast<char>(quote_val.As<Napi::Number>().Int32Value());
                    cisv_parser_set_quote(parser_, quote);
                }
            }

            // Handle escape option
            if (options.Has("escape")) {
                Napi::Value escape_val = options.Get("escape");
                if (escape_val.IsString()) {
                    std::string escape = escape_val.As<Napi::String>();
                    if (!escape.empty()) {
                        cisv_parser_set_escape(parser_, escape[0]);
                    } else {
                        // Empty string means no escape character
                        cisv_parser_set_escape(parser_, '\0');
                    }
                } else if (escape_val.IsNull() || escape_val.IsUndefined()) {
                    // Null/undefined means no escape character
                    cisv_parser_set_escape(parser_, '\0');
                } else if (escape_val.IsNumber()) {
                    char escape = static_cast<char>(escape_val.As<Napi::Number>().Int32Value());
                    cisv_parser_set_escape(parser_, escape);
                }
            }

            // Handle headers option
            if (options.Has("headers")) {
                Napi::Value headers_val = options.Get("headers");
                if (headers_val.IsBoolean()) {
                    has_headers_ = headers_val.As<Napi::Boolean>().Value();
                } else if (headers_val.IsNumber()) {
                    // Non-zero number means true
                    has_headers_ = headers_val.As<Napi::Number>().Int32Value() != 0;
                } else if (headers_val.IsArray()) {
                    // Custom headers provided
                    Napi::Array headers_array = headers_val.As<Napi::Array>();
                    headers_.clear();
                    for (uint32_t i = 0; i < headers_array.Length(); i++) {
                        if (headers_array.Get(i).IsString()) {
                            headers_.push_back(headers_array.Get(i).As<Napi::String>().Utf8Value());
                        }
                    }
                    has_headers_ = false;  // Don't skip first row if custom headers provided
                    headers_processed_ = true;  // Mark as already having headers
                }
            }

            // Handle skipEmptyLines option
            if (options.Has("skipEmptyLines")) {
                Napi::Value skip_val = options.Get("skipEmptyLines");
                if (skip_val.IsBoolean()) {
                    skip_empty_lines_ = skip_val.As<Napi::Boolean>().Value();
                    cisv_parser_set_skip_empty_lines(parser_, skip_empty_lines_);
                } else if (skip_val.IsNumber()) {
                    skip_empty_lines_ = skip_val.As<Napi::Number>().Int32Value() != 0;
                    cisv_parser_set_skip_empty_lines(parser_, skip_empty_lines_);
                }
            }

            // Handle comment character option (if supported)
            if (options.Has("comment")) {
                Napi::Value comment_val = options.Get("comment");
                if (comment_val.IsString()) {
                    std::string comment = comment_val.As<Napi::String>();
                    if (!comment.empty()) {
                        cisv_parser_set_comment(parser_, comment[0]);
                    } else {
                        // Disable comments if empty string
                        cisv_parser_set_comment(parser_, '\0');
                    }
                } else if (comment_val.IsNull() || comment_val.IsUndefined()) {
                    // Disable comments
                    cisv_parser_set_comment(parser_, '\0');
                }
            }

            // Handle trim option (trim whitespace from fields)
            if (options.Has("trim")) {
                Napi::Value trim_val = options.Get("trim");
                if (trim_val.IsBoolean()) {
                    bool trim = trim_val.As<Napi::Boolean>().Value();
                    cisv_parser_set_trim(parser_, trim);
                } else if (trim_val.IsNumber()) {
                    bool trim = trim_val.As<Napi::Number>().Int32Value() != 0;
                    cisv_parser_set_trim(parser_, trim);
                }
            }

            // Handle relaxed option (relax column count checking)
            if (options.Has("relaxed") || options.Has("relax")) {
                Napi::Value relax_val = options.Has("relaxed") ?
                    options.Get("relaxed") : options.Get("relax");
                if (relax_val.IsBoolean()) {
                    bool relaxed = relax_val.As<Napi::Boolean>().Value();
                    cisv_parser_set_relaxed(parser_, relaxed);
                } else if (relax_val.IsNumber()) {
                    bool relaxed = relax_val.As<Napi::Number>().Int32Value() != 0;
                    cisv_parser_set_relaxed(parser_, relaxed);
                }
            }

            // Handle maxRowSize option
            if (options.Has("maxRowSize")) {
                Napi::Value max_size_val = options.Get("maxRowSize");
                if (max_size_val.IsNumber()) {
                    size_t max_size = static_cast<size_t>(max_size_val.As<Napi::Number>().Int64Value());
                    cisv_parser_set_max_row_size(parser_, max_size);
                }
            }

            // Handle encoding option (default is UTF-8)
            if (options.Has("encoding")) {
                Napi::Value encoding_val = options.Get("encoding");
                if (encoding_val.IsString()) {
                    encoding_ = encoding_val.As<Napi::String>().Utf8Value();
                    // Store encoding for later use, actual encoding conversion
                    // would be handled during parsing
                }
            }

            // Handle columns option (column definitions for parsing)
            if (options.Has("columns")) {
                Napi::Value columns_val = options.Get("columns");
                if (columns_val.IsBoolean() && columns_val.As<Napi::Boolean>().Value()) {
                    // true means auto-detect columns from first row
                    has_headers_ = true;
                } else if (columns_val.IsArray()) {
                    // Array of column names or definitions
                    Napi::Array columns_array = columns_val.As<Napi::Array>();
                    columns_.clear();
                    for (uint32_t i = 0; i < columns_array.Length(); i++) {
                        Napi::Value col = columns_array.Get(i);
                        if (col.IsString()) {
                            columns_.push_back(col.As<Napi::String>().Utf8Value());
                        } else if (col.IsObject()) {
                            // Could be column definition object with name, type, etc.
                            Napi::Object col_obj = col.As<Napi::Object>();
                            if (col_obj.Has("name")) {
                                columns_.push_back(col_obj.Get("name").As<Napi::String>().Utf8Value());
                            }
                        }
                    }
                }
            }

            // Handle fromLine option (start parsing from specific line)
            if (options.Has("fromLine") || options.Has("from_line")) {
                Napi::Value from_val = options.Has("fromLine") ?
                    options.Get("fromLine") : options.Get("from_line");
                if (from_val.IsNumber()) {
                    from_line_ = from_val.As<Napi::Number>().Int32Value();
                    cisv_parser_set_from_line(parser_, from_line_);
                }
            }

            // Handle toLine option (stop parsing at specific line)
            if (options.Has("toLine") || options.Has("to_line")) {
                Napi::Value to_val = options.Has("toLine") ?
                    options.Get("toLine") : options.Get("to_line");
                if (to_val.IsNumber()) {
                    to_line_ = to_val.As<Napi::Number>().Int32Value();
                    cisv_parser_set_to_line(parser_, to_line_);
                }
            }

            // Handle skipLinesWithError option
            if (options.Has("skipLinesWithError")) {
                Napi::Value skip_err_val = options.Get("skipLinesWithError");
                if (skip_err_val.IsBoolean()) {
                    skip_lines_with_error_ = skip_err_val.As<Napi::Boolean>().Value();
                    cisv_parser_set_skip_lines_with_error(parser_, skip_lines_with_error_);
                }
            }

            // Handle raw option (return raw unparsed lines)
            if (options.Has("raw")) {
                Napi::Value raw_val = options.Get("raw");
                if (raw_val.IsBoolean()) {
                    return_raw_ = raw_val.As<Napi::Boolean>().Value();
                }
            }

            // Handle objectMode option (return objects instead of arrays)
            if (options.Has("objectMode") || options.Has("objects")) {
                Napi::Value obj_val = options.Has("objectMode") ?
                    options.Get("objectMode") : options.Get("objects");
                if (obj_val.IsBoolean()) {
                    object_mode_ = obj_val.As<Napi::Boolean>().Value();
                }
            }
        }
    }

    ~CisvParser() {
        Cleanup();
    }

    // Explicit cleanup method
    void Cleanup() {
        if (!is_destroyed_) {
            if (parser_) {
                cisv_parser_destroy(parser_);
                parser_ = nullptr;
            }
            if (rc_) {
                rc_->cleanup();
                delete rc_;
                rc_ = nullptr;
            }
            is_destroyed_ = true;
        }
    }

    // Explicit destroy method callable from JavaScript
    void Destroy(const Napi::CallbackInfo &info) {
        Cleanup();
    }

    // Synchronous file parsing
    Napi::Value ParseSync(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        std::string path = info[0].As<Napi::String>();

        auto start = std::chrono::high_resolution_clock::now();

        // Clear previous data
        rc_->rows.clear();
        rc_->current.clear();
        rc_->current_field_index = 0;

        // Set environment for JS transforms
        rc_->env = env;

        int result = cisv_parser_parse_file(parser_, path.c_str());

        auto end = std::chrono::high_resolution_clock::now();
        parse_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Clear the environment reference after parsing
        rc_->env = nullptr;

        if (result < 0) {
            throw Napi::Error::New(env, "parse error: " + std::to_string(result));
        }

        return drainRows(env);
    }

    // Parse string content
    Napi::Value ParseString(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected CSV string");
        }

        std::string content = info[0].As<Napi::String>();

        // Clear previous data
        rc_->rows.clear();
        rc_->current.clear();
        rc_->current_field_index = 0;

        // Set environment for JS transforms
        rc_->env = env;

        // Write the string content as chunks
        cisv_parser_write(parser_, (const uint8_t*)content.c_str(), content.length());
        cisv_parser_end(parser_);

        total_bytes_ = content.length();

        // Clear the environment reference after parsing
        rc_->env = nullptr;

        return drainRows(env);
    }

    // Write chunk for streaming
    void Write(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1) {
            throw Napi::TypeError::New(env, "Expected one argument");
        }

        // Set environment for JS transforms
        rc_->env = env;

        if (info[0].IsBuffer()) {
            auto buf = info[0].As<Napi::Buffer<uint8_t>>();
            cisv_parser_write(parser_, buf.Data(), buf.Length());
            total_bytes_ += buf.Length();
            return;
        }

        if (info[0].IsString()) {
            std::string chunk = info[0].As<Napi::String>();
            cisv_parser_write(parser_, reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size());
            total_bytes_ += chunk.size();
            return;
        }

        throw Napi::TypeError::New(env, "Expected Buffer or String");
    }

    void End(const Napi::CallbackInfo &info) {
        if (!is_destroyed_) {
            cisv_parser_end(parser_);
            // Clear the environment reference after ending
            rc_->env = nullptr;
        }
    }

    Napi::Value GetRows(const Napi::CallbackInfo &info) {
        if (is_destroyed_) {
            Napi::Env env = info.Env();
            throw Napi::Error::New(env, "Parser has been destroyed");
        }
        return drainRows(info.Env());
    }

    void Clear(const Napi::CallbackInfo &info) {
        if (!is_destroyed_ && rc_) {
            rc_->rows.clear();
            rc_->current.clear();
            rc_->current_field_index = 0;
            total_bytes_ = 0;
            parse_time_ = 0;
            // Also clear the environment reference
            rc_->env = nullptr;
        }
    }

    // Add transform using native C transformer or JavaScript function
    Napi::Value Transform(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() < 2) {
            throw Napi::TypeError::New(env, "Expected field index and transform type/function");
        }

        if (!info[0].IsNumber()) {
            throw Napi::TypeError::New(env, "Field index must be a number");
        }

        int field_index = info[0].As<Napi::Number>().Int32Value();

        // Ensure pipeline exists (lazy initialization)
        rc_->ensurePipeline();

        // Store the environment
        rc_->env = env;

        // Handle string transform types - using the actual C transformer
        if (info[1].IsString()) {
            std::string transform_type = info[1].As<Napi::String>();
            cisv_transform_type_t type;

            // Map string to C enum
            if (transform_type == "uppercase") {
                type = TRANSFORM_UPPERCASE;
            } else if (transform_type == "lowercase") {
                type = TRANSFORM_LOWERCASE;
            } else if (transform_type == "trim") {
                type = TRANSFORM_TRIM;
            } else if (transform_type == "to_int" || transform_type == "int") {
                type = TRANSFORM_TO_INT;
            } else if (transform_type == "to_float" || transform_type == "float") {
                type = TRANSFORM_TO_FLOAT;
            } else if (transform_type == "hash_sha256" || transform_type == "sha256") {
                type = TRANSFORM_HASH_SHA256;
            } else if (transform_type == "base64_encode" || transform_type == "base64") {
                type = TRANSFORM_BASE64_ENCODE;
            } else {
                throw Napi::Error::New(env, "Unknown transform type: " + transform_type);
            }

            // Create context if provided
            cisv_transform_context_t* ctx = nullptr;
            if (info.Length() >= 3 && info[2].IsObject()) {
                Napi::Object context_obj = info[2].As<Napi::Object>();
                ctx = (cisv_transform_context_t*)calloc(1, sizeof(cisv_transform_context_t));

                // Extract context properties if they exist
                if (context_obj.Has("key")) {
                    Napi::Value key_val = context_obj.Get("key");
                    if (key_val.IsString()) {
                        std::string key = key_val.As<Napi::String>();
                        ctx->key = strdup(key.c_str());
                        ctx->key_len = key.length();
                    }
                }

                if (context_obj.Has("iv")) {
                    Napi::Value iv_val = context_obj.Get("iv");
                    if (iv_val.IsString()) {
                        std::string iv = iv_val.As<Napi::String>();
                        ctx->iv = strdup(iv.c_str());
                        ctx->iv_len = iv.length();
                    }
                }
            }

            // Add to the C transform pipeline
            if (cisv_transform_pipeline_add(rc_->pipeline, field_index, type, ctx) < 0) {
                // Clean up context if adding failed
                if (ctx) {
                    if (ctx->key) free(ctx->key);
                    if (ctx->iv) free(ctx->iv);
                    if (ctx->extra) free(ctx->extra);
                    free(ctx);
                }
                throw Napi::Error::New(env, "Failed to add transform");
            }

        } else if (info[1].IsFunction()) {
            // Handle JavaScript function transforms
            Napi::Function func = info[1].As<Napi::Function>();

            // Store the function reference for this field
            rc_->js_transforms[field_index] = Napi::Persistent(func);

        } else {
            throw Napi::TypeError::New(env, "Transform must be a string type or function");
        }

        return info.This();  // Return this for chaining
    }

    Napi::Value RemoveTransform(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1 || !info[0].IsNumber()) {
            throw Napi::TypeError::New(env, "Expected field index");
        }

        int field_index = info[0].As<Napi::Number>().Int32Value();

        // Remove from JavaScript transforms
        rc_->js_transforms.erase(field_index);

        // TODO: Implement removal of C transforms in cisv_transformer.c
        // For now, this only removes JS transforms

        return info.This();
    }

    Napi::Value ClearTransforms(const Napi::CallbackInfo &info) {
        if (is_destroyed_) {
            Napi::Env env = info.Env();
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        // Clear JavaScript transforms
        rc_->js_transforms.clear();

        // Clear C transforms - destroy and DON'T recreate pipeline yet
        if (rc_->pipeline) {
            cisv_transform_pipeline_destroy(rc_->pipeline);
            rc_->pipeline = nullptr;  // Will be recreated lazily when needed
        }

        return info.This();
    }

    // Async file parsing (returns a Promise)
    Napi::Value ParseAsync(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        std::string path = info[0].As<Napi::String>();

        // Create a promise
        auto deferred = Napi::Promise::Deferred::New(env);

        // For simplicity, we'll use sync parsing here
        // In production, this should use worker threads
        try {
            Napi::Value result = ParseSync(info);
            deferred.Resolve(result);
        } catch (const Napi::Error& e) {
            deferred.Reject(e.Value());
        }

        return deferred.Promise();
    }

    // Get information about registered transforms
    Napi::Value GetTransformInfo(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        Napi::Object result = Napi::Object::New(env);

        // Count C transforms
        size_t c_transform_count = (rc_ && rc_->pipeline) ? rc_->pipeline->count : 0;
        result.Set("cTransformCount", Napi::Number::New(env, c_transform_count));

        // Count JS transforms
        size_t js_transform_count = rc_ ? rc_->js_transforms.size() : 0;
        result.Set("jsTransformCount", Napi::Number::New(env, js_transform_count));

        // List field indices with transforms
        Napi::Array fields = Napi::Array::New(env);
        size_t idx = 0;

        // Add JS transform field indices
        if (rc_) {
            for (const auto& pair : rc_->js_transforms) {
                fields[idx++] = Napi::Number::New(env, pair.first);
            }
        }

        result.Set("fieldIndices", fields);

        return result;
    }

    Napi::Value GetStats(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        Napi::Object stats = Napi::Object::New(env);

        stats.Set("rowCount", Napi::Number::New(env, rc_ ? rc_->rows.size() : 0));
        stats.Set("fieldCount", Napi::Number::New(env,
            (rc_ && !rc_->rows.empty()) ? rc_->rows[0].size() : 0));
        stats.Set("totalBytes", Napi::Number::New(env, total_bytes_));
        stats.Set("parseTime", Napi::Number::New(env, parse_time_));

        return stats;
    }

    // Static method to count rows
    static Napi::Value CountRows(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        std::string path = info[0].As<Napi::String>();
        size_t count = cisv_parser_count_rows(path.c_str());

        return Napi::Number::New(env, count);
    }

private:
    cisv_parser *parser_;
    RowCollector *rc_;
    size_t total_bytes_;
    double parse_time_;
    bool is_destroyed_;

    // Configuration options
    bool has_headers_;
    bool skip_empty_lines_;
    bool headers_processed_;
    bool skip_lines_with_error_;
    bool return_raw_;
    bool object_mode_;
    int from_line_;
    int to_line_;
    std::string encoding_;
    std::vector<std::string> headers_;
    std::vector<std::string> columns_;

    // Modified drainRows method to handle headers and object mode
    Napi::Value drainRows(Napi::Env env) {
        if (!rc_) {
            return Napi::Array::New(env, 0);
        }

        // Handle headers if needed
        size_t start_row = 0;
        if (has_headers_ && !headers_processed_ && !rc_->rows.empty()) {
            // First row contains headers
            headers_ = rc_->rows[0];
            headers_processed_ = true;
            start_row = 1;  // Skip header row in output
        }

        // Return as objects if object_mode is enabled
        if (object_mode_ && !headers_.empty()) {
            Napi::Array result = Napi::Array::New(env, rc_->rows.size() - start_row);

            for (size_t i = start_row; i < rc_->rows.size(); ++i) {
                Napi::Object row_obj = Napi::Object::New(env);
                const auto& row = rc_->rows[i];

                for (size_t j = 0; j < row.size() && j < headers_.size(); ++j) {
                    row_obj.Set(headers_[j], Napi::String::New(env, row[j]));
                }

                // Add any extra fields that don't have headers
                for (size_t j = headers_.size(); j < row.size(); ++j) {
                    row_obj.Set(std::to_string(j), Napi::String::New(env, row[j]));
                }

                result[i - start_row] = row_obj;
            }

            return result;
        }

        // Return as arrays (default)
        Napi::Array rows = Napi::Array::New(env, rc_->rows.size() - start_row);

        for (size_t i = start_row; i < rc_->rows.size(); ++i) {
            Napi::Array row = Napi::Array::New(env, rc_->rows[i].size());
            for (size_t j = 0; j < rc_->rows[i].size(); ++j) {
                row[j] = Napi::String::New(env, rc_->rows[i][j]);
            }
            rows[i - start_row] = row;
        }

        return rows;
    }

    // Add method to get headers
    Napi::Value GetHeaders(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (headers_.empty()) {
            return env.Null();
        }

        Napi::Array headers = Napi::Array::New(env, headers_.size());
        for (size_t i = 0; i < headers_.size(); i++) {
            headers[i] = Napi::String::New(env, headers_[i]);
        }

        return headers;
    }

    // Add method to set headers manually
    void SetHeaders(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() != 1 || !info[0].IsArray()) {
            throw Napi::TypeError::New(env, "Expected array of header names");
        }

        Napi::Array headers_array = info[0].As<Napi::Array>();
        headers_.clear();

        for (uint32_t i = 0; i < headers_array.Length(); i++) {
            if (headers_array.Get(i).IsString()) {
                headers_.push_back(headers_array.Get(i).As<Napi::String>().Utf8Value());
            }
        }

        headers_processed_ = true;
    }

};

// Initialize all exports
Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    CisvParser::Init(env, exports);

    // Add version info
    exports.Set("version", Napi::String::New(env, "1.0.0"));

    // Add transform type constants
    Napi::Object transformTypes = Napi::Object::New(env);
    transformTypes.Set("UPPERCASE", Napi::String::New(env, "uppercase"));
    transformTypes.Set("LOWERCASE", Napi::String::New(env, "lowercase"));
    transformTypes.Set("TRIM", Napi::String::New(env, "trim"));
    transformTypes.Set("TO_INT", Napi::String::New(env, "to_int"));
    transformTypes.Set("TO_FLOAT", Napi::String::New(env, "to_float"));
    transformTypes.Set("HASH_SHA256", Napi::String::New(env, "hash_sha256"));
    transformTypes.Set("BASE64_ENCODE", Napi::String::New(env, "base64_encode"));
    exports.Set("TransformType", transformTypes);

    return exports;
}

NODE_API_MODULE(cisv, InitAll)
