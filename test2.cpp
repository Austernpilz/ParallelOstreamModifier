#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <zlib.h>
#include <cassert>

// --- Utilities ---

std::vector<char> read_file(const std::string& path)
{
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) throw std::runtime_error("Cannot open file: " + path);

  std::streamsize size = in.tellg();
  in.seekg(0, std::ios::beg);

  std::vector<char> data(size);
  if (!in.read(data.data(), size))
    throw std::runtime_error("Failed to read file: " + path);

  return data;
}

int gzip_decompress(FILE *source, FILE *dest)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned CHUNK = 256*1024;
    unsigned char in[256*1024];
    unsigned char out[256*1024];

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (inflateInit2(&strm, 15 + 16) != Z_OK)
        throw std::runtime_error("inflateInit2 failed");

    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}


// Validate buffers and print first mismatch
bool validate_buffers(const std::vector<char>& original, const std::vector<char>& decompressed)
{
  bool test = true;
  if (original.size() != decompressed.size())
  {
    std::cout << "Size mismatch: original " << original.size()
              << ", decompressed " << decompressed.size() << "\n";
    test = false;
  }
  for (std::size_t i = 0; i < original.size(); ++i)
  {
    if (original[i] != decompressed[i])
    {
      std::cout << "Mismatch at byte " << i
                << ": original=" << int(original[i])
                << ", decompressed=" << int(decompressed[i]) << "\n";
      test = false;
    }
  }
  return test;
}

struct TestSample
{
  std::size_t data_size;
  std::size_t chunk_size;
  int threads;
  std::size_t buffer_size;
};

// --- Test function ---
bool compress_file_through_parallel_ostream(const std::string& input_path,
                                            const std::string& compressed_path,
                                            int threads = 4,
                                            std::size_t buffer_size = 1024*1024)
{
    auto original = read_file(input_path);
    bool test = true;
    // Output file stream
    std::ofstream compressed_file(compressed_path, std::ios::binary);
    if (!compressed_file)
        throw std::runtime_error("Cannot open file for writing: " + compressed_path);

    ParallelOstreamModifier parallel_out(compressed_file, threads);
    parallel_out.set_buffer_size(buffer_size);
    GzipCompressor gzip;
    
    // Set the functor object directly
    parallel_out.set_mod_function(gzip);

    // Feed original data through << operator
    auto start = std::chrono::high_resolution_clock::now();
    for (auto& d : original)
    {
      parallel_out << d;
    }
    

    // Flush all tasks and write to file
    parallel_out.flush();
    auto end = std::chrono::high_resolution_clock::now();
    compressed_file.close();

    // --- Validation ---
    gzip_decompress(compressed_path, "test.MzML")
    std::vector<char> still_original = read_file("test.MzML");

    if (original.size() != still_original.size()) {
        std::cout << "Size mismatch: original " << original.size()
                  << ", decompressed " << decompressed.size() << "\n";
        test = false;
    }

    for (size_t i = 0; i < original.size(); ++i) {
        if (original[i] != still_original[i]) {
            std::cerr << "Mismatch at byte " << i
                      << ": original=" << int(original[i])
                      << ", decompressed=" << int(still_original[i]) << "\n";
            test= false;
        }
    }
    std::cout << start << "Start Time" << end << "EndTime";
    return test;
}

// --- Main ---
int main(int argc, char* argv[]) {

    std::string input_path = argv[1];
    std::string compressed_path = "test.MzML.gzip";

    bool ok = compress_file_through_parallel_ostream(input_path, compressed_path, 40, 1*1024*1024);

    std::cout << (ok ? "Compression and validation succeeded" :  "FAILED") << "\n";

    return ok ? 0 : 1;
}



// using std::vector<char> = std::vector<char>;

// // Read an entire file into a vector<char>
// std::vector<char> read_file(const std::string& path) {
//     std::ifstream in(path, std::ios::binary | std::ios::ate);
//     if (!in) throw std::runtime_error("Cannot open file: " + path);

//     std::streamsize size = in.tellg();
//     in.seekg(0, std::ios::beg);

//     std::vector<char> data(size);
//     if (!in.read(data.data(), size))
//         throw std::runtime_error("Failed to read file: " + path);
//     return data;
// }

// // Simple gzip decompression
// std::vector<char> gzip_decompress(const std::string& path)
// {
//   std::vector<char> out(1024*1024);
//   z_stream strm{};
//   if (inflateInit2(&strm, 15 + 16) != Z_OK)
//       throw std::runtime_error("inflateInit2 failed");

//   strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
//   strm.avail_in = static_cast<uInt>(compressed.size());

//   bytes decompressed;
//   int ret;
//   do {
//     if (strm.total_out >= out.size()) out.resize(out.size() * 2);
//     strm.next_out = reinterpret_cast<Bytef*>(out.data() + strm.total_out);
//     strm.avail_out = static_cast<uInt>(out.size() - strm.total_out);

//     ret = inflate(&strm, Z_NO_FLUSH);
//     if (ret < 0) { inflateEnd(&strm); throw std::runtime_error("inflate failed"); }
//   } while (ret != Z_STREAM_END);

//   decompressed.assign(out.begin(), out.begin() + strm.total_out);
//   inflateEnd(&strm);
//   return decompressed;
// }


// // Random data generator
// std::vector<char> random_bytes(std::size_t size, std::mt19937_64& rng)
// {
//   std::vector<char> random_data(size);
//   std::uniform_int_distribution<int> distribution(0, 255);
//   for (auto &c : random_data) 
//   {
//     c = static_cast<char>(distribution(rng));
//   }
//   return random_data;
// }




// void run_test(const TestConfig& cfg, std::mt19937_64& rng) 
// {
//   std::fstream compressed_stream("test.txt");
//   ParallelOstreamModifier parallel_out(compressed_stream, cfg.threads);

//   GzipCompressor gzip;
//   gzip.set_chunk_size(cfg.chunk_size);
//   parallel_out.set_mod_function([gzip](const std::vector<char>& input)
//   {
//     return gzip(input);
//   });

//   std::vector<char> original;
//   original.reserve(cfg.data_size);

//   auto start = std::chrono::high_resolution_clock::now();

//   for (std::size_t done = 0; done < cfg.data_size; done += cfg.chunk_size)
//   {
//     std::vector<char> buf = random_bytes(std::min(cfg.chunk_size, cfg.data_size - done), rng);
//     original.insert(original.end(), buf.begin(), buf.end());
//     parallel_out << buf;
//   }

//   parallel_out.flush();

//   auto mid = std::chrono::high_resolution_clock::now();

//   std::vector<char> compressed(compressed_stream.str().begin(), compressed_stream.str().end());
//   std::vector<char> decompressed = gzip_decompress(compressed);
//   auto end = std::chrono::high_resolution_clock::now();

//   bool ok = validate_buffers(original, decompressed);
//   std::cout << "Test (threads=" << cfg.threads
//             << ", buffer=" << cfg.buffer_size
//             << ", size=" << (cfg.data_size >> 20) << "MB) "
//             << (ok ? "passed" : "FAILED") << "\n";

//   double comp_time = std::chrono::duration<double>(mid - start).count();
//   double decomp_time = std::chrono::duration<double>(end - mid).count();

//   std::cout << "Compression speed: " << double(cfg.data_size)/(1<<20)/comp_time << " MB/s, "
//             << "Decompression speed: " << double(cfg.data_size)/(1<<20)/decomp_time << " MB/s\n";
// }

// int main() {
//   std::mt19937_64 rng(42);

//   // 10 test configurations
//   std::vector<TestConfig> configs = {
//       {1000<<20, 512<<20, 5, 1<<20},
//       {500<<20, 2<<20, 20, 2<<20},
//       {100<<20, 32<<20, 40, 4<<20},
//       {200<<20, 256<<20, 80, 8<<20},
//       {500<<20, 16<<20, 10, 16<<20},
//       {1000<<20, 32<<20, 100, 1<<20},
//       {50<<20, 512<<10, 50, 512<<10},
//       {200<<20, 64<<20, 200, 1<<20},
//       {30<<20, 512<<10, 20, 512<<10},
//       {1<<20, 256<<10, 10, 256<<10},
//   };

//   for (auto &cfg : configs)
//       run_test(cfg, rng);

//   // Stress test: 10000 repetitions, 10MB data, 1MB buffer, 100 threads
//   constexpr int stress_reps = 10000;
//   TestConfig stress_cfg{10<<20, 1<<20, 100, 1<<20};
//   std::cout << "\nStarting stress test: " << stress_reps << " repetitions\n";

//   auto stress_start = std::chrono::high_resolution_clock::now();
//   for (int i = 0; i < stress_reps; ++i) {
//       run_test(stress_cfg, rng);
//   }

//   auto stress_end = std::chrono::high_resolution_clock::now();

//   double stress_time = std::chrono::duration<double>(stress_end - stress_start).count();
//   std::cout << "Stress test completed in " << stress_time
//             << " s, " << stress_reps << " repetitions\n";

//   return 0;
// }


// // --- Test function ---
// bool compress_file_and_validate(const std::string& input_file, int threads = 4, std::size_t buffer_size = 1024*1024) {
//     auto original = read_file(input_file);

//     std::ostringstream compressed_stream(std::ios::binary);
//     ParallelOstreamModifier parallel_out(compressed_stream, threads);

//     GzipCompressor gzip;
//     gzip.set_chunk_size(buffer_size);

//     parallel_out.set_mod_function(gzip);

//     // Write the file through ostream using << operator
//     parallel_out << original;

//     parallel_out.flush(); // make sure all data is processed

//     // Get compressed data
//     std::vector<char> compressed(compressed_stream.str().begin(), compressed_stream.str().end());

//     // Decompress
//     auto decompressed = gzip_decompress(compressed);

//     // Compare
//     if (original.size() != decompressed.size()) {
//         std::cerr << "Size mismatch: original " << original.size()
//                   << ", decompressed " << decompressed.size() << "\n";
//         return false;
//     }

//     for (size_t i = 0; i < original.size(); ++i) {
//         if (original[i] != decompressed[i]) {
//             std::cerr << "Mismatch at byte " << i
//                       << ": original=" << int(original[i])
//                       << ", decompressed=" << int(decompressed[i]) << "\n";
//             return false;
//         }
//     }

//     return true;
// }

// // --- Main ---
// int main(int argc, char* argv[]) {
//     if (argc < 2) {
//         std::cerr << "Usage: " << argv[0] << " <file1> [file2 ...]\n";
//         return 1;
//     }

//     for (int i = 1; i < argc; ++i) {
//         std::string file = argv[i];
//         bool ok = compress_file_and_validate(file, 4, 1024*1024);
//         std::cout << file << " -> " << (ok ? "passed" : "FAILED") << "\n";
//     }

//     return 0;
// }