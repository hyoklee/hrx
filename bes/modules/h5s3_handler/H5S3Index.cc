// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Index implementation: read/write index.parquet with Apache Arrow/Parquet.

#include "H5S3Index.h"

#include <stdexcept>
#include <sys/stat.h>

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

using namespace std;

namespace h5s3 {

// Turn a failing arrow::Status into a C++ exception with context.
static void check(const arrow::Status &st, const string &what)
{
    if (!st.ok())
        throw runtime_error("h5s3 index: " + what + ": " + st.ToString());
}

bool H5S3Index::exists(const string &path)
{
    struct stat sb;
    return ::stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode);
}

void H5S3Index::write(const string &path, const vector<IndexEntry> &entries)
{
    arrow::StringBuilder key_b;
    arrow::UInt64Builder size_b;
    arrow::StringBuilder mtime_b;

    for (const auto &e : entries) {
        check(key_b.Append(e.key), "append key");
        check(size_b.Append(e.size), "append size");
        check(mtime_b.Append(e.last_modified), "append last_modified");
    }

    shared_ptr<arrow::Array> key_a, size_a, mtime_a;
    check(key_b.Finish(&key_a), "finish key");
    check(size_b.Finish(&size_a), "finish size");
    check(mtime_b.Finish(&mtime_a), "finish last_modified");

    auto schema = arrow::schema({
        arrow::field("key", arrow::utf8()),
        arrow::field("size", arrow::uint64()),
        arrow::field("last_modified", arrow::utf8()),
    });

    auto table = arrow::Table::Make(schema, {key_a, size_a, mtime_a});

    shared_ptr<arrow::io::FileOutputStream> out;
    auto out_res = arrow::io::FileOutputStream::Open(path);
    if (!out_res.ok())
        throw runtime_error("h5s3 index: open output " + path + ": " + out_res.status().ToString());
    out = *out_res;

    check(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), out, /*chunk_size*/ 65536),
          "write table");
    check(out->Close(), "close output");
}

vector<IndexEntry> H5S3Index::read(const string &path)
{
    auto in_res = arrow::io::ReadableFile::Open(path);
    if (!in_res.ok())
        throw runtime_error("h5s3 index: open input " + path + ": " + in_res.status().ToString());
    shared_ptr<arrow::io::ReadableFile> in = *in_res;

    unique_ptr<parquet::arrow::FileReader> reader;
    check(parquet::arrow::OpenFile(in, arrow::default_memory_pool(), &reader), "open parquet");

    shared_ptr<arrow::Table> table;
    check(reader->ReadTable(&table), "read table");

    auto key_col = table->GetColumnByName("key");
    auto size_col = table->GetColumnByName("size");
    auto mtime_col = table->GetColumnByName("last_modified");
    if (!key_col || !size_col || !mtime_col)
        throw runtime_error("h5s3 index: " + path + " missing expected columns");

    vector<IndexEntry> out;
    out.reserve(table->num_rows());

    // The table may have multiple chunks; iterate chunk by chunk.
    int n_chunks = key_col->num_chunks();
    for (int c = 0; c < n_chunks; ++c) {
        auto keys = static_pointer_cast<arrow::StringArray>(key_col->chunk(c));
        auto sizes = static_pointer_cast<arrow::UInt64Array>(size_col->chunk(c));
        auto mtimes = static_pointer_cast<arrow::StringArray>(mtime_col->chunk(c));
        for (int64_t i = 0; i < keys->length(); ++i) {
            IndexEntry e;
            e.key = keys->GetString(i);
            e.size = sizes->Value(i);
            e.last_modified = mtimes->IsNull(i) ? string() : mtimes->GetString(i);
            out.push_back(std::move(e));
        }
    }
    return out;
}

} // namespace h5s3
