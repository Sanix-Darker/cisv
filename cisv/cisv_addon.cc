#include <napi.h>
#include "cisv_parser.h"
#include "cisv_transformer.h"
#include <vector>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <chrono>

namespace {

// Zero-copy field view
struct FieldView {
    const char* data;
    size_t len;

    FieldView(const char* d, size_t l) : data(d), len(l) {}

    // Convert to string only when needed
    std::string toString() const {
        return std::string(data, len);
    }
};

// Optimized RowCollector using zero-copy where possible
struct RowCollector {
    // For zero-copy mode - store field views
    std::vector<FieldView> current_views;
    std::vector<std::vector<FieldView>> row_views;

    // For transformed data that needs ownership
    std::vector<std::string> current_owned;
    std::vector<std::vector<std::string>> rows_owned;

    // Keep the original buffer alive for zero-copy
    std::string buffer_copy;

    cisv_transform_pipeline_t* pipeline;
    int current_field_index;
    bool has_transforms;
    bool zero_copy_mode;

    // JavaScript transforms stored separately
    std::unordered_map<int, Napi::FunctionReference> js_transforms;
    Napi::Env env;

    RowCollector() : pipeline(nullptr), current_field_index(0),
                     has_transforms(false), zero_copy_mode(true), env(nullptr) {
        // Pre-reserve for performance
        current_views.reserve(32);
        row_views.reserve(10000);
        current_owned.reserve(32);
        rows_owned.reserve(10000);
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
        row_views.clear();
        current_views.clear();
        rows_owned.clear();
        current_owned.clear();
        buffer_copy.clear();
        current_field_index = 0;
        env = nullptr;
        has_transforms = false;
        zero_copy_mode = true;
    }

    inline void ensurePipeline() {
        if (!pipeline) {
            pipeline = cisv_transform_pipeline_create(16);
        }
    }

    void updateTransformStatus() {
        has_transforms = (pipeline && pipeline->count > 0) || !js_transforms.empty();
        zero_copy_mode = !has_transforms;
    }

    // Zero-copy field addition
    inline void addFieldZeroCopy(const char* data, size_t len) {
        current_views.emplace_back(data, len);
    }

    // Field with transforms
    inline void addFieldTransformed(const char* data, size_t len, int field_index) {
        std::string result(data, len);

        // Apply C transforms
        if (pipeline && pipeline->count > 0) {
            cisv_transform_result_t c_result = cisv_transform_apply(
                pipeline, field_index, result.c_str(), result.length()
            );

            if (c_result.data) {
                result.assign(c_result.data, c_result.len);
                if (c_result.needs_free) {
                    cisv_transform_result_free(&c_result);
                }
            }
        }

        // Apply JS transforms
        if (env && !js_transforms.empty()) {
            auto it = js_transforms.find(field_index);
            if (it != js_transforms.end() && !it->second.IsEmpty()) {
                try {
                    Napi::String input = Napi::String::New(env, result);
                    Napi::Value js_result = it->second.Call({input});
                    if (js_result.IsString()) {
                        result = js_result.As<Napi::String>().Utf8Value();
                    }
                } catch (...) {}
            }

            auto it_all = js_transforms.find(-1);
            if (it_all != js_transforms.end() && !it_all->second.IsEmpty()) {
                try {
                    Napi::String input = Napi::String::New(env, result);
                    Napi::Value js_result = it_all->second.Call({input});
                    if (js_result.IsString()) {
                        result = js_result.As<Napi::String>().Utf8Value();
                    }
                } catch (...) {}
            }
        }

        current_owned.emplace_back(std::move(result));
    }

    inline void finishRow() {
        if (zero_copy_mode) {
            row_views.emplace_back(std::move(current_views));
            current_views.clear();
            current_views.reserve(32);
        } else {
            rows_owned.emplace_back(std::move(current_owned));
            current_owned.clear();
            current_owned.reserve(32);
        }
        current_field_index = 0;
    }
};

// Optimized callbacks
static void field_cb_zero_copy(void *user, const char *data, size_t len) {
    auto *rc = reinterpret_cast<RowCollector *>(user);

    if (rc->zero_copy_mode) {
        rc->addFieldZeroCopy(data, len);
    } else {
        rc->addFieldTransformed(data, len, rc->current_field_index);
    }
    rc->current_field_index++;
}

static void row_cb_zero_copy(void *user) {
    auto *rc = reinterpret_cast<RowCollector *>(user);
    rc->finishRow();
}

static void error_cb(void *user, int line, const char *msg) {
    fprintf(stderr, "CSV Parse Error at line %d: %s\n", line, msg);
}

} // namespace

class CisvParser : public Napi::ObjectWrap<CisvParser> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "cisvParser", {
            InstanceMethod<&CisvParser::ParseSync>("parseSync"),
            InstanceMethod<&CisvParser::ParseAsync>("parse"),
            InstanceMethod<&CisvParser::ParseString>("parseString"),
            InstanceMethod<&CisvParser::Write>("write"),
            InstanceMethod<&CisvParser::End>("end"),
            InstanceMethod<&CisvParser::GetRows>("getRows"),
            InstanceMethod<&CisvParser::Clear>("clear"),
            InstanceMethod<&CisvParser::Transform>("transform"),
            InstanceMethod<&CisvParser::RemoveTransform>("removeTransform"),
            InstanceMethod<&CisvParser::ClearTransforms>("clearTransforms"),
            InstanceMethod<&CisvParser::GetStats>("getStats"),
            InstanceMethod<&CisvParser::GetTransformInfo>("getTransformInfo"),
            InstanceMethod<&CisvParser::Destroy>("destroy"),
            InstanceMethod<&CisvParser::SetConfig>("setConfig"),
            InstanceMethod<&CisvParser::GetConfig>("getConfig"),

            StaticMethod<&CisvParser::CountRows>("countRows"),
            StaticMethod<&CisvParser::CountRowsWithConfig>("countRowsWithConfig")
        });

        exports.Set("cisvParser", func);
        return exports;
    }

    CisvParser(const Napi::CallbackInfo &info) : Napi::ObjectWrap<CisvParser>(info) {
        rc_ = new RowCollector();
        parse_time_ = 0;
        total_bytes_ = 0;
        is_destroyed_ = false;

        cisv_config_init(&config_);

        if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object options = info[0].As<Napi::Object>();
            ApplyConfigFromObject(options);
        }

        config_.field_cb = field_cb_zero_copy;
        config_.row_cb = row_cb_zero_copy;
        config_.error_cb = error_cb;
        config_.user = rc_;

        parser_ = cisv_parser_create_with_config(&config_);
    }

    ~CisvParser() {
        Cleanup();
    }

    void ApplyConfigFromObject(Napi::Object options) {
        if (options.Has("delimiter")) {
            Napi::Value delim = options.Get("delimiter");
            if (delim.IsString()) {
                std::string delim_str = delim.As<Napi::String>();
                if (!delim_str.empty()) {
                    config_.delimiter = delim_str[0];
                }
            }
        }

        if (options.Has("quote")) {
            Napi::Value quote = options.Get("quote");
            if (quote.IsString()) {
                std::string quote_str = quote.As<Napi::String>();
                if (!quote_str.empty()) {
                    config_.quote = quote_str[0];
                }
            }
        }

        if (options.Has("skipEmptyLines")) {
            config_.skip_empty_lines = options.Get("skipEmptyLines").As<Napi::Boolean>();
        }

        if (options.Has("trim")) {
            config_.trim = options.Get("trim").As<Napi::Boolean>();
        }

        if (options.Has("fromLine")) {
            config_.from_line = options.Get("fromLine").As<Napi::Number>().Int32Value();
        }

        if (options.Has("toLine")) {
            config_.to_line = options.Get("toLine").As<Napi::Number>().Int32Value();
        }
    }

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

        if (parser_) {
            cisv_parser_destroy(parser_);
        }

        config_.field_cb = field_cb_zero_copy;
        config_.row_cb = row_cb_zero_copy;
        config_.error_cb = error_cb;
        config_.user = rc_;

        parser_ = cisv_parser_create_with_config(&config_);
    }

    Napi::Value GetConfig(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        Napi::Object config = Napi::Object::New(env);
        config.Set("delimiter", Napi::String::New(env, std::string(1, config_.delimiter)));
        config.Set("quote", Napi::String::New(env, std::string(1, config_.quote)));
        config.Set("skipEmptyLines", Napi::Boolean::New(env, config_.skip_empty_lines));
        config.Set("trim", Napi::Boolean::New(env, config_.trim));
        config.Set("fromLine", Napi::Number::New(env, config_.from_line));
        config.Set("toLine", Napi::Number::New(env, config_.to_line));

        return config;
    }

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

    void Destroy(const Napi::CallbackInfo &info) {
        Cleanup();
    }

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

        // Clear and prepare
        rc_->row_views.clear();
        rc_->row_views.reserve(10000);
        rc_->rows_owned.clear();
        rc_->current_views.clear();
        rc_->current_owned.clear();
        rc_->current_field_index = 0;
        rc_->env = env;

        int result = cisv_parser_parse_file(parser_, path.c_str());

        auto end = std::chrono::high_resolution_clock::now();
        parse_time_ = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

        rc_->env = nullptr;

        if (result < 0) {
            throw Napi::Error::New(env, "parse error: " + std::to_string(result));
        }

        return drainRowsFast(env);
    }

    Napi::Value ParseString(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected CSV string");
        }

        std::string content = info[0].As<Napi::String>();

        // For zero-copy, we need to keep the buffer alive
        rc_->buffer_copy = content;

        // Clear and prepare
        rc_->row_views.clear();
        rc_->row_views.reserve(content.length() / 20);
        rc_->rows_owned.clear();
        rc_->current_views.clear();
        rc_->current_owned.clear();
        rc_->current_field_index = 0;
        rc_->env = env;

        cisv_parser_write(parser_, (const uint8_t*)rc_->buffer_copy.c_str(), rc_->buffer_copy.length());
        cisv_parser_end(parser_);

        total_bytes_ = content.length();
        rc_->env = nullptr;

        return drainRowsFast(env);
    }

    void Write(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1) {
            throw Napi::TypeError::New(env, "Expected one argument");
        }

        rc_->env = env;

        if (info[0].IsBuffer()) {
            auto buf = info[0].As<Napi::Buffer<uint8_t>>();
            // For streaming, we need to copy to keep data alive
            rc_->buffer_copy.append(reinterpret_cast<const char*>(buf.Data()), buf.Length());
            cisv_parser_write(parser_, reinterpret_cast<const uint8_t*>(rc_->buffer_copy.data() + total_bytes_), buf.Length());
            total_bytes_ += buf.Length();
            return;
        }

        if (info[0].IsString()) {
            std::string chunk = info[0].As<Napi::String>();
            rc_->buffer_copy.append(chunk);
            cisv_parser_write(parser_, reinterpret_cast<const uint8_t*>(rc_->buffer_copy.data() + total_bytes_), chunk.size());
            total_bytes_ += chunk.size();
            return;
        }

        throw Napi::TypeError::New(env, "Expected Buffer or String");
    }

    void End(const Napi::CallbackInfo &info) {
        if (!is_destroyed_) {
            cisv_parser_end(parser_);
        }
    }

    Napi::Value GetRows(const Napi::CallbackInfo &info) {
        if (is_destroyed_) {
            Napi::Env env = info.Env();
            throw Napi::Error::New(env, "Parser has been destroyed");
        }
        return drainRowsFast(info.Env());
    }

    void Clear(const Napi::CallbackInfo &info) {
        if (!is_destroyed_ && rc_) {
            rc_->row_views.clear();
            rc_->rows_owned.clear();
            rc_->current_views.clear();
            rc_->current_owned.clear();
            rc_->current_field_index = 0;
            rc_->buffer_copy.clear();
            total_bytes_ = 0;
            parse_time_ = 0;
            rc_->env = nullptr;
        }
    }

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

        rc_->ensurePipeline();
        rc_->env = env;

        if (info[1].IsString()) {
            std::string transform_type = info[1].As<Napi::String>();
            cisv_transform_type_t type;

            if (transform_type == "uppercase") {
                type = TRANSFORM_UPPERCASE;
            } else if (transform_type == "lowercase") {
                type = TRANSFORM_LOWERCASE;
            } else if (transform_type == "trim") {
                type = TRANSFORM_TRIM;
            } else {
                throw Napi::Error::New(env, "Unknown transform type: " + transform_type);
            }

            if (cisv_transform_pipeline_add(rc_->pipeline, field_index, type, nullptr) < 0) {
                throw Napi::Error::New(env, "Failed to add transform");
            }

            rc_->updateTransformStatus();

        } else if (info[1].IsFunction()) {
            Napi::Function func = info[1].As<Napi::Function>();
            rc_->js_transforms[field_index] = Napi::Persistent(func);
            rc_->updateTransformStatus();
        }

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
        rc_->js_transforms.erase(field_index);
        rc_->updateTransformStatus();

        return info.This();
    }

    Napi::Value ClearTransforms(const Napi::CallbackInfo &info) {
        if (is_destroyed_) {
            Napi::Env env = info.Env();
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        rc_->js_transforms.clear();

        if (rc_->pipeline) {
            cisv_transform_pipeline_destroy(rc_->pipeline);
            rc_->pipeline = nullptr;
        }

        rc_->updateTransformStatus();
        return info.This();
    }

    Napi::Value ParseAsync(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        auto deferred = Napi::Promise::Deferred::New(env);

        try {
            Napi::Value result = ParseSync(info);
            deferred.Resolve(result);
        } catch (const Napi::Error& e) {
            deferred.Reject(e.Value());
        }

        return deferred.Promise();
    }

    Napi::Value GetTransformInfo(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        Napi::Object result = Napi::Object::New(env);
        size_t c_transform_count = (rc_ && rc_->pipeline) ? rc_->pipeline->count : 0;
        result.Set("cTransformCount", Napi::Number::New(env, c_transform_count));
        size_t js_transform_count = rc_ ? rc_->js_transforms.size() : 0;
        result.Set("jsTransformCount", Napi::Number::New(env, js_transform_count));

        return result;
    }

    Napi::Value GetStats(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (is_destroyed_) {
            throw Napi::Error::New(env, "Parser has been destroyed");
        }

        Napi::Object stats = Napi::Object::New(env);

        size_t row_count = rc_ ? (rc_->zero_copy_mode ? rc_->row_views.size() : rc_->rows_owned.size()) : 0;
        size_t field_count = 0;

        if (rc_ && row_count > 0) {
            field_count = rc_->zero_copy_mode ?
                (rc_->row_views.empty() ? 0 : rc_->row_views[0].size()) :
                (rc_->rows_owned.empty() ? 0 : rc_->rows_owned[0].size());
        }

        stats.Set("rowCount", Napi::Number::New(env, row_count));
        stats.Set("fieldCount", Napi::Number::New(env, field_count));
        stats.Set("totalBytes", Napi::Number::New(env, total_bytes_));
        stats.Set("parseTime", Napi::Number::New(env, parse_time_));
        stats.Set("currentLine", Napi::Number::New(env, parser_ ? cisv_parser_get_line_number(parser_) : 0));

        return stats;
    }

    static Napi::Value CountRows(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() != 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        std::string path = info[0].As<Napi::String>();
        size_t count = cisv_parser_count_rows(path.c_str());

        return Napi::Number::New(env, count);
    }

    static Napi::Value CountRowsWithConfig(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(env, "Expected file path string");
        }

        std::string path = info[0].As<Napi::String>();

        cisv_config config;
        cisv_config_init(&config);

        if (info.Length() > 1 && info[1].IsObject()) {
            Napi::Object options = info[1].As<Napi::Object>();

            if (options.Has("delimiter")) {
                std::string delim = options.Get("delimiter").As<Napi::String>();
                if (!delim.empty()) config.delimiter = delim[0];
            }

            if (options.Has("quote")) {
                std::string quote = options.Get("quote").As<Napi::String>();
                if (!quote.empty()) config.quote = quote[0];
            }
        }

        size_t count = cisv_parser_count_rows_with_config(path.c_str(), &config);

        return Napi::Number::New(env, count);
    }

private:
    // Ultra-fast row draining with minimal allocations
    inline Napi::Value drainRowsFast(Napi::Env env) {
        if (!rc_) {
            return Napi::Array::New(env, 0);
        }

        if (rc_->zero_copy_mode) {
            // Zero-copy path
            const size_t row_count = rc_->row_views.size();
            Napi::Array rows = Napi::Array::New(env, row_count);

            for (size_t i = 0; i < row_count; ++i) {
                const auto& src_row = rc_->row_views[i];
                const size_t col_count = src_row.size();
                Napi::Array row = Napi::Array::New(env, col_count);

                for (size_t j = 0; j < col_count; ++j) {
                    // Create string directly from pointer and length
                    row[j] = Napi::String::New(env, src_row[j].data, src_row[j].len);
                }
                rows[i] = row;
            }

            return rows;
        } else {
            // Transformed data path
            const size_t row_count = rc_->rows_owned.size();
            Napi::Array rows = Napi::Array::New(env, row_count);

            for (size_t i = 0; i < row_count; ++i) {
                const auto& src_row = rc_->rows_owned[i];
                const size_t col_count = src_row.size();
                Napi::Array row = Napi::Array::New(env, col_count);

                for (size_t j = 0; j < col_count; ++j) {
                    row[j] = Napi::String::New(env, src_row[j]);
                }
                rows[i] = row;
            }

            return rows;
        }
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
    exports.Set("version", Napi::String::New(env, "1.1.0"));
    return exports;
}

NODE_API_MODULE(cisv, InitAll)
