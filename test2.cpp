#include "src/modifier/ParallelDataModifier.h"  // your parallel class
// #include "src/modifier/GzipCompressor.h"
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <zlib.h>

// simple gunzip function for validation
std::vector<char> gzip_decompress(const std::vector<char>& compressed) {
    std::vector<char> out(1024 * 1024); // start with 1MB
    z_stream strm{};
    if (inflateInit2(&strm, 15 + 16) != Z_OK)
        throw std::runtime_error("inflateInit2 failed");

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    strm.avail_in = static_cast<uInt>(compressed.size());

    std::vector<char> decompressed;
    int ret;
    do {
        if (strm.total_out >= out.size()) out.resize(out.size() * 2);
        strm.next_out = reinterpret_cast<Bytef*>(out.data() + strm.total_out);
        strm.avail_out = static_cast<uInt>(out.size() - strm.total_out);

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            throw std::runtime_error("inflate failed");
        }
    } while (ret != Z_STREAM_END);

    decompressed.assign(out.begin(), out.begin() + strm.total_out);
    inflateEnd(&strm);
    return decompressed;
}

int main() {
    using bytes = ParallelDataModifier::bytes_;
    constexpr std::size_t total_size = 1ull << 30; // 1 GB
    constexpr std::size_t chunk_size = 1ull << 20; // 1 MB
    constexpr int threads = 4;

    std::ostringstream compressed_stream(std::ios::binary);
    ParallelDataModifier pdm(compressed_stream, threads);

    // Set to gzip
    pdm.set_mod_function(GzipCompressor());

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    bytes original;
    original.reserve(total_size);

    std::cout << "Generating and compressing 1 GB of random data...\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t done = 0; done < total_size; done += chunk_size) {
        bytes buf(chunk_size);
        for (auto &c : buf) c = static_cast<char>(dist(rng));
        original.insert(original.end(), buf.begin(), buf.end());
        pdm.enqueue_task(std::move(buf));
    }

    pdm.flush_to(compressed_stream);
    auto mid = std::chrono::high_resolution_clock::now();

    bytes compressed(compressed_stream.str().begin(), compressed_stream.str().end());
    std::cout << "Compressed size: " << (compressed.size() >> 20) << " MB\n";

    // decompress and check
    auto decompressed = gzip_decompress(compressed);
    auto end = std::chrono::high_resolution_clock::now();

    if (decompressed == original) {
        std::cout << "✅ Integrity check passed\n";
    } else {
        std::cerr << "❌ Integrity check FAILED\n";
    }

    double comp_time   = std::chrono::duration<double>(mid - start).count();
    double decomp_time = std::chrono::duration<double>(end - mid).count();

    std::cout << "Compression speed: " << (double(total_size) / (1<<20) / comp_time) << " MB/s\n";
    std::cout << "Decompression speed: " << (double(total_size) / (1<<20) / decomp_time) << " MB/s\n";

    return 0;
}
