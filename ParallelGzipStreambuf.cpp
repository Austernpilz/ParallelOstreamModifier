
#include <streambuf>
#include <ostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <zlib.h>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cassert>

class ParallelGzipStreambuf : public std::streambuf 
{
  public:
    ParallelGzipStreambuf::ParallelGzipStreambuf(std::ostream& os, std::size_t buffer_size, int compression_level, int num_threads)
    : ostream_(os), buffer_(buffer_size), compression_(compression_level), threads_(num_threads)
    {
      setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

    void set_ostream(std::ostream& os)
    {
      ostream_ = &os
    }

    void set_buffer(std::size_t buffer_size)
    {
      buffer_.resize(buffer_size)
    }

    void set_compression(int compression_level)
    {
      compression_ = compression_level
    }

    void set_threads(int num_threads)
    {
      threads_ = num_threads
    }

    void set_chunk_size(std::size_t chunk_size)
    {
      chunk_size_ = chunk_size
    }

    void readjust_buffer_from_threads()
    {
      std::size_t new_buffer_size = buffer_.size() * threads_
      buffer_.resize(new_buffer_size)
    }

  protected:
    int_type overflow(int_type ch) override 
    {
      if (ch != traits_type::eof()) {
          *pptr() = ch;
          pbump(1);
      }
      return (flush() == -1 ) ? traits_type::eof() : ch;
    }

    int sync() override 
    {
      flush();
      {
          std::unique_lock<std::mutex> lock(queueMutex_);
          flushRequested_ = true;
      }
      queueCondVar_.notify_all();
      std::unique_lock<std::mutex> lock(doneMutex_);
      doneCondVar_.wait(lock, [this] { return taskQueue_.empty() && !workerBusy_; });
      return 0;
    }

    std::streamsize xsputn(const char* s, std::streamsize count) override
    {
      std::streamsize space = epptr() - pptr();
      if (count <= space) 
      {
        std::memcpy(pptr(), s, count);
        pbump(static_cast<int>(count));
        return count;
      } 
      else 
      {
        flushBuffer();
        if (count > buffer_.size()) 
        {
          // Directly compress large input
          enqueueTask(std::vector<byte>(s, s + count));
          return count;
        } 
        else 
        {
          std::memcpy(pbase(), s, count);
          setp(pbase(), epptr());
          pbump(static_cast<int>(count));
          return count;
        }
      }
    }
    
  private:
    std::ostream& ostream_;
    std::vector<char> buffer_;
    std::size_t chunk_size_ = 1024 * 32; // max value that is recommendet in zlib

    std::thread worker_;
    std::queue<std::vector<char>> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondVar_;
    bool flushRequested_ = false;
    bool stopWorker_ = false;

    std::mutex doneMutex_;
    std::condition_variable doneCondVar_;
    bool workerBusy_ = false;

    std::mutex writeMutex_;

    int flush()
    {
      std::ptrdiff_t size = pptr() - pbase();
      if (size == 0) return 0;

      std::vector<char> data(pbase(), pbase() + size);
      setp(pbase(), epptr()); // Reset buffer
      enqueueTask(std::move(data));
      return 0;
    }

    void enqueueTask(std::vector<char>&& data) 
    {
      {
          std::lock_guard<std::mutex> lock(queueMutex_);
          taskQueue_.push(std::move(data));
      }
      queueCondVar_.notify_one();
    }

    void workerThread() 
    {
      while (true) 
      {
        std::vector<char> data;
        {
          std::unique_lock<std::mutex> lock(queueMutex_);
          queueCondVar_.wait(lock, [this] {
              return !taskQueue_.empty() || stopWorker_ || flushRequested_;
          });

          if (stopWorker_ && taskQueue_.empty()) break;

          if (!taskQueue_.empty()) 
          {
            data = std::move(taskQueue_.front());
            taskQueue_.pop();
            workerBusy_ = true;
          } 
          else 
          {
            workerBusy_ = false;
            doneCondVar_.notify_all();
            continue;
          }
        }
        std::vector<char> compressed = compressData(data, (taskQueue_.empty() ? true : false));
        {
          std::lock_guard<std::mutex> lock(writeMutex_);
          ostream_.write(compressed.data(), compressed.size());
        }

        {
          std::lock_guard<std::mutex> lock(doneMutex_);
          workerBusy_ = false;
          if (taskQueue_.empty()) 
          {
            doneCondVar_.notify_all();
          }
        }
      }
    }
      
    std::vector<char> compress_data(const std::vector<char>& input, const bool last)
    {
      std::vector<char> compressed_output; // return value
      std::vector<char> zbuffer_output(chunk_size_); // buffer for zstream

      uLongf destLen = compressBound(input.size());
      compressed.resize(destLen);

      z_stream compression_stream;
      compression_stream.zalloc = Z_NULL;
      compression_stream.zfree = Z_NULL;
      compression_stream.opaque = Z_NULL;

      // for parameters see https://neacsum.github.io/zlib/group__adv.html#gae501d2862c68d17b909d6f1c9264815c
      int compression_status = deflateInit2(
        &compression_stream, // strm= 
        compression_, // level = 
          // can be anything between 0..9
          // 0 is no compression, 1 is best speed, 9 is best compression
        Z_DEFLATED, //method = 
        //fixed in this mode
        15+16, // windowBits = 
          // +16 for gzipped | can be anything between 8..15 (+16)
        9, // memLevel = 
          // anything between 1..9, standart is 8
          // 1 is min mem, slow, low compression rate
          // 9 is max mem, optimal speed, high compression rate,
        Z_DEFAULT_STRATEGY // strategy = 
          // depending on the data, this could be optimized, look in documentation
        );
        
      assert(compression_status == Z_OK);

      compression_stream.avail_in = input.size();
      compression_stream.next_in = reinterpret_cast<unsigned char *>(const_cast<char *>(input.data()));
        
        // if (ferror(input)) {
        //     deflateEnd(&compression_stream);
        //     return Z_ERRNO;
        // }
        // int flush;
        // unsigned have;
        // flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
      int mode = last ? Z_FINISH : Z_SYNC_FLUSH;
      do {
        compression_stream.avail_out = zbuffer_output.size();
        compression_stream.next_out = reinterpret_cast<unsigned char *>(const_cast<char *>(zbuffer_output.data()));
        //documentation for deflate https://neacsum.github.io/zlib/group__basic.html#gaedba3a94d6e827d61b660443ae5b9f09
        compression_status = deflate(&compression_stream, mode);
        assert(compression_status != Z_STREAM_ERROR);
        compressed_output.insert(
          compressed_output.end(), 
          zbuffer_output.begin(), 
          zbuffer_output.begin() + (zbuffer_output.size() - compression_stream.avail_out));
        } while (compression_stream.avail_out == 0);

      deflateEnd(&compression_stream);  
      assert(compression_status == Z_STREAM_END);

      return compressed_output;
    }
};


class ParallelGzipOstream : public std::ostream 
{
  public:
    explicit ParallelGzipOstream(std::ostream& os, const int threads = 1)
    : std::ostream(nullptr), buffer_(os) 
    {
      rdbuf(&buffer_);
    }

    explicit ParallelGzipOstream(const std::string& filename, const int threads = 1)
    : file_path_(filename, std::ios::binary), std::ostream(nullptr), buffer_(file_path_, threads_)
    {
      if (!file_path_)
      {
        throw std::ios_base::failure("Failed to open file: " + filename);
      }
      rdbuf(&buffer_);
    }

    // Move constructor
    ParallelGzipOstream(ParallelGzipOstream&& other_os) noexcept
    : std::ostream(std::move(other_os)), buffer_(std::move(other_os.buffer_)), file_path_(std::move(other.file_path_))
    {
      rdbuf(&buffer_);
    }

    // Deleted copy semantics
    ParallelGzipOstream(const ParallelGzipOstream&) = delete;
    ParallelGzipOstream& operator=(const ParallelGzipOstream&) = delete;

    // Move assignment
    ParallelGzipOstream& operator=(ParallelGzipOstream&& other_os) noexcept 
    {
      if (this != &other_os)
      {
        std::ostream::operator=(std::move(other_os));
        buffer_ = std::move(other_os.buffer_);
        file_path_ = std::move(other.file_path_);
        rdbuf(&buffer_);
      }
      return *this;
    }

    ~ParallelGzipOstream() override = default;

  private:
    ParallelGzipStreambuf buffer_;
    std::ofstream file_path_
};



int main() {
  std::ofstream file("compressed_output.gz", std::ios::binary);
  ParallelGzipOstream gzipOut(file);

  for (int i = 0; i < 10000; ++i) {
      gzipOut << "Line " << i << ": This is a test of parallel gzip stream buffer.\n";
  }

  // gzipOut.flush(); // Ensures all data is compressed and written
}
