// corrected_program.cpp
#include <chrono>
#include <iostream>
#include <random>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <zlib.h>
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <deque>
#include <limits>

#include "GzipCompressor.h"
#include "ParallelDataModifier.h"



bool delete_file(const char* filename = "TEST.txt")
{
  if (std::remove(filename) == 0)
  {
    std::cout << "File deleted successfully.\n"; 
    return true;
  } 
  else
  {
    std::perror("Error deleting file"); 
    return false;
  }
}

std::vector<unsigned char> gzip_decompress(std::vector<unsigned char> input)
{
  std::vector<unsigned char> output;
  std::vector<unsigned char> zbuffer;
  zbuffer.reserve(256*1024);
  z_stream strm;
  std::size_t CHUNK = 256*1024;
  int ret;

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  strm.next_in = reinterpret_cast<Bytef*>(input.data());
  strm.avail_in = static_cast<uInt>(input.size());

  if (inflateInit2(&strm, 15 + 16) != Z_OK)
  {
    inflateEnd(&strm);
    throw std::runtime_error("inflateInit2 failed");
  }


  do {
    strm.avail_out = static_cast<uInt>(CHUNK);;
    strm.next_out = reinterpret_cast<Bytef*>(zbuffer.data());
;

    ret = inflate(&strm, strm.avail_in ? Z_NO_FLUSH : Z_FINISH);
    if (ret == Z_STREAM_ERROR)
    {
      inflateEnd(&strm);
      throw std::runtime_error("deflate failed");
    }
    
    std::size_t have = CHUNK - strm.avail_out;
    output.insert(output.end(), zbuffer.data(), zbuffer.data() + have);

  } while (ret != Z_STREAM_END);

  /* clean up and return */
  (void)inflateEnd(&strm);
  return output;
}

std::vector<unsigned char> read_file(const std::string& path)
{
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) { throw std::runtime_error("Cannot open file: " + path); }

  std::streamsize size = in.tellg();
  if (size < 0) throw std::runtime_error("Bad file size: " + path);
  in.seekg(0, std::ios::beg);

  std::vector<unsigned char> data(static_cast<std::size_t>(size));
  if (size > 0) 
  {
    if (!in.read(reinterpret_cast<char*>(data.data()), size)) { throw std::runtime_error("Failed to read file: " + path); }
  }
  in.close();
  return data;
}

std::vector<unsigned char> random_bytes(std::size_t size, std::mt19937_64& rng)
{
  std::vector<unsigned char> random_data(size);
  std::uniform_int_distribution<int> distribution(0, (1<<20)*10);
  for (auto &c : random_data) 
  {
    c = static_cast<unsigned char>(distribution(rng));
  }
  return random_data;
}
bool check_for_validity(const std::vector<unsigned char>& original, const std::vector<unsigned char>& data2)
{
  std::vector<unsigned char> decompressed = gzip_decompress(data2);
  bool test = true;
  if (original.size() != decompressed.size())
  {
    std::cout << "Size mismatch: original " << original.size()
              << ", decompressed " << decompressed.size() << "\n";
    test = false;
  }

  std::size_t check_len = std::min(original.size(), static_cast<std::size_t>(10000));
  for (std::size_t i = 0; i < check_len; ++i)
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

struct test_config
{
  std::size_t data_size{0};
  std::size_t chunk_size{0};
  std::size_t buffer_size{0};
  double comp_time{0};
  int threads{1};
  bool valid{false};
};
struct span
{
  std::size_t min_size{ 10 };
  std::size_t max_size{ 0 };
  double mean_size{0};
  std::size_t chunk_min{ 10 };
  std::size_t chunk_max{ 0 };
  double chunk_mean{0};
  std::size_t buffer_min{ 10 };
  std::size_t buffer_max{ 0 };
  double buffer_mean{0};
  int minthreads{ 10 };
  int maxthreads{ 0 };
  double meanthreads{0};
  size_t n_{0};

  void fill_data(const test_config& tc)
  {
    if (n_ == 0) return; // avoid division by zero
    min_size = std::min(min_size, tc.data_size);
    max_size = std::max(max_size, tc.data_size);
    mean_size += static_cast<double>(tc.data_size) / static_cast<double>(n_);
    chunk_min = std::min(chunk_min, tc.chunk_size);
    chunk_max = std::max(chunk_max, tc.chunk_size);
    chunk_mean += static_cast<double>(tc.chunk_size) / static_cast<double>(n_);
    buffer_min = std::min(buffer_min, tc.buffer_size);
    buffer_max = std::max(buffer_max, tc.buffer_size);
    buffer_mean += static_cast<double>(tc.buffer_size) / static_cast<double>(n_);
    minthreads = std::min(minthreads, tc.threads);
    maxthreads = std::max(maxthreads, tc.threads);
    meanthreads += static_cast<double>(tc.threads) / static_cast<double>(n_);
  }
};

std::vector<test_config> build_test_cases(std::deque<std::vector<unsigned char>> &data, std::size_t n)
{
  std::cout<<"finding_somthign. ";
  data.clear();
  std::vector<test_config> output;
  output.reserve(n);

  std::random_device rd;
  std::mt19937_64 randomachine(rd());
  std::uniform_int_distribution<int> distribution(2, 100);
  span something;
  something.n_ = n ? n : 1;

  // clamp sizes to avoid insane allocations (adjust as you want)
  std::size_t SIZE_CLAMP_MAX = (1<<10);
  for (std::size_t i = 0; i < n; ++i)
  {
    test_config experiment;
    // original code used shifts; to keep similar variability but safe we use multiplication
    std::size_t a = static_cast<std::size_t>(distribution(randomachine));
    std::size_t b = static_cast<std::size_t>(distribution(randomachine));
    experiment.data_size = std::max( std::max((a-b),(b-a))+1 ,SIZE_CLAMP_MAX);
    a = static_cast<std::size_t>(distribution(randomachine));
    b = static_cast<std::size_t>(distribution(randomachine));
    experiment.chunk_size = std::min(std::max(a,b) * std::max(a,b), SIZE_CLAMP_MAX);
    a = static_cast<std::size_t>(distribution(randomachine));
    b = static_cast<std::size_t>(distribution(randomachine));
    experiment.buffer_size = std::min(((b*a) + 1) + ((b*a) +1), SIZE_CLAMP_MAX);
    experiment.threads = std::max(5, distribution(randomachine));

    std::vector<unsigned char> datapoint = random_bytes(experiment.data_size, randomachine);
    data.push_back(std::move(datapoint));
    output.push_back(experiment);
    something.fill_data(output.back());
      std::cout << "data_size=" << experiment.data_size
          << " chunk_size=" << experiment.chunk_size
          << " buffer_size=" << experiment.buffer_size << "\n";
  }

  std::cout << " created " << n << " datapoints, where (min,max,mean) " << '\n'
            << " data_size " << something.min_size << " | " << something.max_size << " | " << something.mean_size << '\n'
            << " chunk_size: " << something.chunk_min << " | " << something.chunk_max << " | " << something.chunk_mean << '\n'
            << " buffer_size: " << something.buffer_min << " | " << something.buffer_max << " | " << something.buffer_mean << '\n'
            << " threads: " << something.minthreads << " | " << something.maxthreads << " | " << something.meanthreads << '\n';
  return output;
}

void run_test(std::vector<unsigned char>&& data, test_config &tc)
{

  // write original data to OG.txt
  {
    std::ofstream check_for_true("OG.txt", std::ios::out | std::ios::binary);
    if (!check_for_true.is_open()) { throw std::runtime_error("Cannot open OG.txt for writing"); }
    std::streamsize size = static_cast<std::streamsize>(data.size());
    if (size > 0) check_for_true.write(reinterpret_cast<const char*>(data.data()), size);
  }

  std::cout << "First Test\n";
  try
  {
    GzipCompressor gc;
    std::vector<unsigned char> result;
    std::size_t data_chunk_size = std::max<std::size_t>(1, data.size() / static_cast<std::size_t>(tc.threads));
    gc.set_chunk_size(tc.chunk_size);

    std::size_t index = 0;
    while (index < data.size())
    {
      std::size_t len = std::min(data_chunk_size, data.size() - index);
      std::vector<unsigned char> chunk(data.begin() + index, data.begin() + index + len);
      index += len;
      std::vector<unsigned char> task = gc(std::move(chunk));
      result.insert(result.end(), task.begin(), task.end());
    }

    std::vector<unsigned char> check1 = read_file("OG.txt");

    bool test_data_changed = check_for_validity(data, result);
    bool test_vali = check_for_validity(check1, result);
    tc.valid = test_vali && test_data_changed;
    if (tc.valid) { std::cout << "compressing works\n"; }
    else { std::cout << "compressing failed, but maybe the compression is not const\n"; }
  }
  catch (...)
  {
    tc.valid = false;
  }

  std::cout << "2nd Test with PDM\n";
  try
  {
    ParallelDataModifier pdm(static_cast<int>(tc.threads + 5));
    GzipCompressor gc;
    delete_file("TEST.txt");
    pdm.set_mod_function(gc);

    std::ofstream test_file("TEST.txt", std::ios::out | std::ios::binary);
    if (!test_file.is_open()) { std::cout << "can't open file\n"; }

    pdm.flush_to(test_file, true);
    if (!tc.valid) { std::cout << "not successful: the last one was corrupted\n"; }

    std::vector<unsigned char> test2 = read_file("OG.txt");
    if (data.size() != test2.size()) { std::cout << "data was modified, not const\n"; }

    std::size_t buffer = std::max<std::size_t>(1, tc.buffer_size);
    std::size_t index = 0;

    while (index < data.size())
    {
      std::size_t len = std::min(buffer, data.size() - index);
      std::vector<unsigned char> chunk(data.begin() + index, data.begin() + index + len);
      index += len;
      pdm.enqueue_task(std::move(chunk));
      pdm.flush_to(test_file, true);
    }
    test_file.close();

    std::vector<unsigned char> test1 = read_file("TEST.txt");
    std::vector<unsigned char> testog = read_file("OG.txt");

    // Note: check_for_validity expects (original, compressed). Here we try to validate round-trip; swap as needed.
    bool test_data_changed = check_for_validity(testog, test1);

    tc.valid = test_data_changed;
    if (tc.valid) { std::cout << "compressing works\n"; }
    else { std::cout << "compressing failed, but maybe the compression\n"; }
  }
  catch (...)
  {
    tc.valid = false;
  }
}
    
    
// --- Main ---

int main(int argc, char* argv[])
{
  std::deque<std::vector<unsigned char>> data_queue;
  std::random_device rd;
  std::mt19937_64 randomachine(rd());

  for (std::size_t i = 1; i <= 10; ++i) // reduced loop count for runtime sanity; change as needed
  {
    std::cout<< i << " so groß sollen die erstmal werden";
    std::size_t n = i * 10;
    std::vector<test_config> fun = build_test_cases(data_queue, n);
    if (fun.empty()) continue;

    std::uniform_int_distribution<std::size_t> distribution(0, fun.size() - 1);
    while (!data_queue.empty())
    {
      std::vector<unsigned char> work_task = std::move(data_queue.front());
      data_queue.pop_front();
      int j = static_cast<int>(distribution(randomachine));
      run_test(std::move(work_task), fun[j]);
    }
  }
  return 0;
}





  // ParallelOstreamModifier pom(test_file, tc.threads);
  // ParallelStreambufModifier psm();
  
  
  





