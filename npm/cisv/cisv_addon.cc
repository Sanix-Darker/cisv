#include <napi.h>
#include "../../lib/cisv_parser.h"
#include "../../lib/cisv_transformer.h"
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

static void error_cb(void *user, int line, const char *msg) {
    // Log errors to stderr for now
    fprintf(stderr, "CSV Parse Error at line %d: %s\n", line, msg);
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
            InstanceMethod("setConfig", &CisvParser::SetConfig),
            InstanceMethod("getConfig", &CisvParser::GetConfig),
            InstanceMethod("transformByName", &CisvParser::TransformByName),
            InstanceMethod("setHeaderFields", &CisvParser::SetHeaderFields),
            InstanceMethod("removeTransformByName", &CisvParser::RemoveTransformByName),

            StaticMethod("countRows", &CisvParser::CountRows),
            StaticMethod("countRowsWithConfig", &CisvParser::CountRowsWithConfig)
        });

        exports.Set("cisvParser", func);
        return exports;
    }

    CisvParser(const Napi::CallbackInfo &info) : Napi::ObjectWrap<CisvParser>(info) {
        rc_ = new RowCollector();
        parse_time_ = 0;
        total_bytes_ = 0;
        is_destroyed_ = false;

        // Initialize configuration with defaults
        cisv_config_init(&config_);

        config_.max_row_size = 0;

        // Handle constructor options if provided
        if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object options = info[0].As<Napi::Object>();
            ApplyConfigFromObject(options);
        }

        // Set callbacks
        config_.field_cb = field_cb;
        config_.row_cb = row_cb;
        config_.error_cb = error_cb;
        config_.user = rc_;

        // Create parser with configuration
        parser_ = cisv_parser_create_with_config(&config_);
    }

    ~CisvParser() {
        Cleanup();
    }

    // Apply configuration from JavaScript object
    void ApplyConfigFromObject(Napi::Object options) {
        // Delimiter
        if (options.Has("delimiter")) {
            Napi::Value delim = options.Get("delimiter");
            if (delim.IsString()) {
                std::string delim_str = delim.As<Napi::String>();
                if (!delim_str.empty()) {
                    config_.delimiter = delim_str[0];
                }
            }
        }

        // Quote character
        if (options.Has("quote")) {
            Napi::Value quote = options.Get("quote");
            if (quote.IsString()) {
                std::string quote_str = quote.As<Napi::String>();
                if (!quote_str.empty()) {
                    config_.quote = quote_str[0];
                }
            }
        }

        // Escape character
        if (options.Has("escape")) {
            Napi::Value escape = options.Get("escape");
            if (escape.IsString()) {
                std::string escape_str = escape.As<Napi::String>();
                if (!escape_str.empty()) {
                    config_.escape = escape_str[0];
                }
            } else if (escape.IsNull() || escape.IsUndefined()) {
                config_.escape = 0; // RFC4180 style
            }
        }

        // Comment character
        if (options.Has("comment")) {
            Napi::Value comment = options.Get("comment");
            if (comment.IsString()) {
                std::string comment_str = comment.As<Napi::String>();
                if (!comment_str.empty()) {
                    config_.comment = comment_str[0];
                }
            }
        }

        // Boolean options
        if (options.Has("skipEmptyLines")) {
            config_.skip_empty_lines = options.Get("skipEmptyLines").As<Napi::Boolean>();
        }

        if (options.Has("trim")) {
            config_.trim = options.Get("trim").As<Napi::Boolean>();
        }

        if (options.Has("relaxed")) {
            config_.relaxed = options.Get("relaxed").As<Napi::Boolean>();
        }

        if (options.Has("skipLinesWithError")) {
            config_.skip_lines_with_error = options.Get("skipLinesWithError").As<Napi::Boolean>();
        }

        // Numeric options
        if (options.Has("maxRowSize")) {
            Napi::Value val = options.Get("maxRowSize");
            if (!val.IsNull() && !val.IsUndefined()) {
                config_.max_row_size = val.As<Napi::Number>().Uint32Value();
            }
        }

        if (options.Has("fromLine")) {
            config_.from_line = options.Get("fromLine").As<Napi::Number>().Int32Value();
        }

        if (options.Has("toLine")) {
            config_.to_line = options.Get("toLine").As<Napi::Number>().Int32Value();
        }
    }

    // Set configuration after creation
    void SetConfig(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1 || !info[0].IsObject()) {
            throw Napi::TypeError::New(env, "Expected configuration object");
        }

        Napi::Object options = info[0].As<Napi::Object>();
        ApplyConfigFromObject(options);

        // Recreate parser with new configuration
        if (parser_) {
            cisv_parser_destroy(parser_);
        }

        config_.field_cb = field_cb;
        config_.row_cb = row_cb;
        config_.error_cb = error_cb;
        config_.user = rc_;

        parser_ = cisv_parser_create_with_config(&config_);
    }

    // Get current configuration
    Napi::Value GetConfig(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        Napi::Object config = Napi::Object::New(env);

        // Character configurations
        config.Set("delimiter", Napi::String::New(env, std::string(1, config_.delimiter)));
        config.Set("quote", Napi::String::New(env, std::string(1, config_.quote)));

        if (config_.escape) {
            config.Set("escape", Napi::String::New(env, std::string(1, config_.escape)));
        } else {
            config.Set("escape", env.Null());
        }

        if (config_.comment) {
            config.Set("comment", Napi::String::New(env, std::string(1, config_.comment)));
        } else {
            config.Set("comment", env.Null());
        }

        // Boolean options
        config.Set("skipEmptyLines", Napi::Boolean::New(env, config_.skip_empty_lines));
        config.Set("trim", Napi::Boolean::New(env, config_.trim));
        config.Set("relaxed", Napi::Boolean::New(env, config_.relaxed));
        config.Set("skipLinesWithError", Napi::Boolean::New(env, config_.skip_lines_with_error));

        // Numeric options
        config.Set("maxRowSize", Napi::Number::New(env, config_.max_row_size));
        config.Set("fromLine", Napi::Number::New(env, config_.from_line));
        config.Set("toLine", Napi::Number::New(env, config_.to_line));

        return config;
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
            // FIXME: the transformer may need this
            // rc_->env = nullptr;
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
                    if (ctx->key) free((void*)ctx->key);
                    if (ctx->iv) free((void*)ctx->iv);
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

Napi::Value TransformByName(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (is_destroyed_) {
        throw Napi::Error::New(env, "Parser has been destroyed");
    }

    if (info.Length() < 2) {
        throw Napi::TypeError::New(env, "Expected field name and transform type/function");
    }

    if (!info[0].IsString()) {
        throw Napi::TypeError::New(env, "Field name must be a string");
    }

    std::string field_name = info[0].As<Napi::String>();

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

        // Add to the C transform pipeline by name
        if (cisv_transform_pipeline_add_by_name(rc_->pipeline, field_name.c_str(), type, ctx) < 0) {
            // Clean up context if adding failed
            if (ctx) {
                if (ctx->key) free((void*)ctx->key);
                if (ctx->iv) free((void*)ctx->iv);
                if (ctx->extra) free(ctx->extra);
                free(ctx);
            }
            throw Napi::Error::New(env, "Failed to add transform for field: " + field_name);
        }

    } else if (info[1].IsFunction()) {
        // Handle JavaScript function transforms by name
        Napi::Function func = info[1].As<Napi::Function>();

        // Add to the C transform pipeline by name
        if (cisv_transform_pipeline_add_js_by_name(rc_->pipeline, field_name.c_str(), &func) < 0) {
            throw Napi::Error::New(env, "Failed to add JS transform for field: " + field_name);
        }

    } else {
        throw Napi::TypeError::New(env, "Transform must be a string type or function");
    }

    return info.This();  // Return this for chaining
}

void SetHeaderFields(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (is_destroyed_) {
        throw Napi::Error::New(env, "Parser has been destroyed");
    }

    if (info.Length() != 1 || !info[0].IsArray()) {
        throw Napi::TypeError::New(env, "Expected array of field names");
    }

    Napi::Array field_names = info[0].As<Napi::Array>();
    size_t field_count = field_names.Length();

    // Convert to C array of strings
    const char** c_field_names = (const char**)malloc(field_count * sizeof(char*));
    if (!c_field_names) {
        throw Napi::Error::New(env, "Memory allocation failed");
    }

    for (size_t i = 0; i < field_count; i++) {
        Napi::Value field_val = field_names[i];
        if (!field_val.IsString()) {
            free(c_field_names);
            throw Napi::TypeError::New(env, "Field names must be strings");
        }
        std::string field_str = field_val.As<Napi::String>();
        c_field_names[i] = strdup(field_str.c_str());
    }

    // Ensure pipeline exists
    rc_->ensurePipeline();

    // Set header fields in the pipeline
    if (cisv_transform_pipeline_set_header(rc_->pipeline, c_field_names, field_count) < 0) {
        // Clean up
        for (size_t i = 0; i < field_count; i++) {
            free((void*)c_field_names[i]);
        }
        free(c_field_names);
        throw Napi::Error::New(env, "Failed to set header fields");
    }

    // Clean up temporary array (the pipeline makes copies)
    for (size_t i = 0; i < field_count; i++) {
        free((void*)c_field_names[i]);
    }
    free(c_field_names);
}

// Add this method to remove transforms by field name
Napi::Value RemoveTransformByName(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (is_destroyed_) {
        throw Napi::Error::New(env, "Parser has been destroyed");
    }

    if (info.Length() != 1 || !info[0].IsString()) {
        throw Napi::TypeError::New(env, "Expected field name");
    }

    std::string field_name = info[0].As<Napi::String>();

    // Remove from JavaScript transforms by finding the field index
    if (rc_->pipeline && rc_->pipeline->header_fields) {
        for (size_t i = 0; i < rc_->pipeline->header_count; i++) {
            if (strcmp(rc_->pipeline->header_fields[i], field_name.c_str()) == 0) {
                rc_->js_transforms.erase(i);
                break;
            }
        }
    }

    // TODO: Implement removal of C transforms by name in cisv_transformer.c
    // For now, this only removes JS transforms

    return info.This();
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
        // FIXME: In production, this should use worker threads
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
        stats.Set("currentLine", Napi::Number::New(env,
            parser_ ? cisv_parser_get_line_number(parser_) : 0));

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

    // Static method to count rows with configuration
    static Napi::Value CountRowsWithConfig(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        std::string path = info[0].As<Napi::String>();

        cisv_config config;
        cisv_config_init(&config);

        // Apply configuration if provided
        if (info.Length() > 1 && info[1].IsObject()) {
            Napi::Object options = info[1].As<Napi::Object>();

            // Apply same configuration parsing logic
            if (options.Has("delimiter")) {
                std::string delim = options.Get("delimiter").As<Napi::String>();
                if (!delim.empty()) config.delimiter = delim[0];
            }

            if (options.Has("quote")) {
                std::string quote = options.Get("quote").As<Napi::String>();
                if (!quote.empty()) config.quote = quote[0];
            }

            if (options.Has("comment")) {
                std::string comment = options.Get("comment").As<Napi::String>();
                if (!comment.empty()) config.comment = comment[0];
            }

            if (options.Has("skipEmptyLines")) {
                config.skip_empty_lines = options.Get("skipEmptyLines").As<Napi::Boolean>();
            }

            if (options.Has("fromLine")) {
                config.from_line = options.Get("fromLine").As<Napi::Number>().Int32Value();
            }

            if (options.Has("toLine")) {
                config.to_line = options.Get("toLine").As<Napi::Number>().Int32Value();
            }
        }

        size_t count = cisv_parser_count_rows_with_config(path.c_str(), &config);

        return Napi::Number::New(env, count);
    }

private:
    Napi::Value drainRows(Napi::Env env) {
        if (!rc_) {
            return Napi::Array::New(env, 0);
        }

        Napi::Array rows = Napi::Array::New(env, rc_->rows.size());

        for (size_t i = 0; i < rc_->rows.size(); ++i) {
            Napi::Array row = Napi::Array::New(env, rc_->rows[i].size());
            for (size_t j = 0; j < rc_->rows[i].size(); ++j) {
                row[j] = Napi::String::New(env, rc_->rows[i][j]);
            }
            rows[i] = row;
        }

        // Don't clear here if we want to keep data for multiple reads
        // rc_->rows.clear();

        return rows;
    }

    cisv_parser *parser_;
    cisv_config config_;
    RowCollector *rc_;
    size_t total_bytes_;
    double parse_time_;
    bool is_destroyed_;
};

// Initialize all exports
Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    CisvParser::Init(env, exports);

    // Add version info
    exports.Set("version", Napi::String::New(env, "1.1.0"));

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
