#include <napi.h>
#include "cisv_parser.h"
#include "cisv_transformer.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace {

// Extended RowCollector that handles transforms
struct RowCollector {
    std::vector<std::string> current;
    std::vector<std::vector<std::string>> rows;
    cisv_transform_pipeline_t* pipeline;
    int current_field_index;

    RowCollector() : pipeline(nullptr), current_field_index(0) {
        pipeline = cisv_transform_pipeline_create(16);
    }

    ~RowCollector() {
        if (pipeline) {
            cisv_transform_pipeline_destroy(pipeline);
        }
    }
};

static void field_cb(void *user, const char *data, size_t len) {
    auto *rc = reinterpret_cast<RowCollector *>(user);

    // Apply transforms using the C transformer
    if (rc->pipeline && rc->pipeline->count > 0) {
        cisv_transform_result_t result = cisv_transform_apply(
            rc->pipeline,
            rc->current_field_index,
            data,
            len
        );

        if (result.data) {
            rc->current.emplace_back(result.data, result.len);

            // Free if the transformer allocated memory
            if (result.needs_free) {
                free(result.data);
            }
        } else {
            rc->current.emplace_back(data, len);
        }
    } else {
        rc->current.emplace_back(data, len);
    }

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

        // Handle constructor options if provided
        if (info.Length() > 0 && info[0].IsObject()) {
            // Parse options object here if needed
        }
    }

    ~CisvParser() {
        if (parser_) {
            cisv_parser_destroy(parser_);
        }
        delete rc_;
    }

    // Synchronous file parsing
    Napi::Value ParseSync(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        std::string path = info[0].As<Napi::String>();

        auto start = std::chrono::high_resolution_clock::now();

        // Clear previous data
        rc_->rows.clear();
        rc_->current.clear();
        rc_->current_field_index = 0;

        int result = cisv_parser_parse_file(parser_, path.c_str());

        auto end = std::chrono::high_resolution_clock::now();
        parse_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (result < 0) {
            throw Napi::Error::New(env, "Parse error: " + std::to_string(result));
        }

        return drainRows(env);
    }

    // Async file parsing (returns a Promise)
    Napi::Value ParseAsync(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

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

    // Parse string content
    Napi::Value ParseString(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected CSV string");
        }

        std::string content = info[0].As<Napi::String>();

        // Clear previous data
        rc_->rows.clear();
        rc_->current.clear();
        rc_->current_field_index = 0;

        // Write the string content as chunks
        cisv_parser_write(parser_, (const uint8_t*)content.c_str(), content.length());
        cisv_parser_end(parser_);

        total_bytes_ = content.length();

        return drainRows(env);
    }

    // Write chunk for streaming
    void Write(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() != 1) {
            throw Napi::TypeError::New(env, "Expected one argument");
        }

        if (info[0].IsBuffer()) {
            auto buf = info[0].As<Napi::Buffer<uint8_t>>();
            cisv_parser_write(parser_, buf.Data(), buf.Length());
            total_bytes_ += buf.Length();
        } else if (info[0].IsString()) {
            std::string str = info[0].As<Napi::String>();
            cisv_parser_write(parser_, (const uint8_t*)str.c_str(), str.length());
            total_bytes_ += str.length();
        } else {
            throw Napi::TypeError::New(env, "Expected Buffer or String");
        }
    }

    void End(const Napi::CallbackInfo &info) {
        cisv_parser_end(parser_);
    }

    Napi::Value GetRows(const Napi::CallbackInfo &info) {
        return drainRows(info.Env());
    }

    void Clear(const Napi::CallbackInfo &info) {
        rc_->rows.clear();
        rc_->current.clear();
        rc_->current_field_index = 0;
        total_bytes_ = 0;
        parse_time_ = 0;
    }

    // Add transform using native C transformer
    Napi::Value Transform(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() < 2) {
            throw Napi::TypeError::New(env, "Expected field index and transform type");
        }

        if (!info[0].IsNumber()) {
            throw Napi::TypeError::New(env, "Field index must be a number");
        }

        int field_index = info[0].As<Napi::Number>().Int32Value();

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
                if (ctx) {
                    if (ctx->key) free(ctx->key);
                    if (ctx->iv) free(ctx->iv);
                    free(ctx);
                }
                throw Napi::Error::New(env, "Failed to add transform");
            }

        } else if (info[1].IsFunction()) {
            // For JavaScript functions, we need to store them and call back
            // This is complex and requires storing JS callbacks
            // For now, throw an error or implement a callback system
            throw Napi::Error::New(env,
                "JavaScript function transforms require using the JS wrapper. "
                "Use string transform types for native performance.");
        } else {
            throw Napi::TypeError::New(env, "Transform must be a string type or function");
        }

        return info.This();  // Return this for chaining
    }

    Napi::Value RemoveTransform(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() != 1 || !info[0].IsNumber()) {
            throw Napi::TypeError::New(env, "Expected field index");
        }

        // This would need implementation in cisv_transformer.c
        // For now, we can only clear all transforms

        return info.This();
    }

    Napi::Value ClearTransforms(const Napi::CallbackInfo &info) {
        if (rc_->pipeline) {
            cisv_transform_pipeline_destroy(rc_->pipeline);
            rc_->pipeline = cisv_transform_pipeline_create(16);
        }
        return info.This();
    }

    Napi::Value GetStats(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        Napi::Object stats = Napi::Object::New(env);

        stats.Set("rowCount", Napi::Number::New(env, rc_->rows.size()));
        stats.Set("fieldCount", Napi::Number::New(env,
            rc_->rows.empty() ? 0 : rc_->rows[0].size()));
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
    Napi::Value drainRows(Napi::Env env) {
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
    RowCollector *rc_;
    size_t total_bytes_;
    double parse_time_;
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
