#include <napi.h>
#include "cisv_parser.h"
#include <vector>

namespace {
struct RowCollector {
    std::vector<std::string> current;
    std::vector<std::vector<std::string>> rows;
};

static void field_cb(void *user, const char *data, size_t len) {
    auto *rc = reinterpret_cast<RowCollector *>(user);
    rc->current.emplace_back(data, len);
}

static void row_cb(void *user) {
    auto *rc = reinterpret_cast<RowCollector *>(user);
    rc->rows.emplace_back(std::move(rc->current));
    rc->current.clear();
}
}

class cisvParser : public Napi::ObjectWrap<cisvParser> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "cisvParser", {
            InstanceMethod("parseSync", &cisvParser::ParseSync),
            InstanceMethod("write", &cisvParser::Write),
            InstanceMethod("end", &cisvParser::End),
            // NOTE: this is not optimal for now, may need to optimize it.
            InstanceMethod("getRows", &cisvParser::GetRows)
        });
        exports.Set("cisvParser", func);
        return exports;
    }

    cisvParser(const Napi::CallbackInfo &info) : Napi::ObjectWrap<cisvParser>(info) {
        rc_ = new RowCollector();
        parser_ = cisv_parser_create(field_cb, row_cb, rc_);
    }

    ~cisvParser() {
        cisv_parser_destroy(parser_);
        delete rc_;
    }

    Napi::Value ParseSync(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (info.Length() != 1 || !info[0].IsString())
            throw Napi::TypeError::New(env, "Expected file path string");
        std::string path = info[0].As<Napi::String>();
        if (cisv_parser_parse_file(parser_, path.c_str()) < 0)
            throw Napi::Error::New(env, "parse error");
        return drainRows(env);
    }

    void Write(const Napi::CallbackInfo &info) {
        if (info.Length() != 1 || !info[0].IsBuffer())
            throw Napi::TypeError::New(info.Env(), "Expected Buffer");
        auto buf = info[0].As<Napi::Buffer<uint8_t>>();
        cisv_parser_write(parser_, buf.Data(), buf.Length());
    }

    void End(const Napi::CallbackInfo &info) {
        cisv_parser_end(parser_);
    }

    Napi::Value GetRows(const Napi::CallbackInfo &info) {
        return drainRows(info.Env());
    }

private:
    Napi::Value drainRows(Napi::Env env) {
        Napi::Array rows = Napi::Array::New(env, rc_->rows.size());
        for (size_t i = 0; i < rc_->rows.size(); ++i) {
            Napi::Array row = Napi::Array::New(env, rc_->rows[i].size());
            for (size_t j = 0; j < rc_->rows[i].size(); ++j)
                row[j] = Napi::String::New(env, rc_->rows[i][j]);
            rows[i] = row;
        }
        rc_->rows.clear(); // We need to clear the internal vector after returning the data
        return rows;
    }

    cisv_parser *parser_;
    RowCollector *rc_;
};

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    return cisvParser::Init(env, exports);
}

NODE_API_MODULE(cisv, InitAll)
