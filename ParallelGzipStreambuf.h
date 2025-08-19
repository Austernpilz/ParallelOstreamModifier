#pragma once 

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
    ParallelGzipStreambuf(std::ostream& os,
                          std::size_t buffer_size = (1024*1024), // ~10mb
                          int compression_level = Z_DEFAULT_COMPRESSION,
                          int num_threads = 1);

    ParallelGzipStreambuf(const ParallelGzipStreamBuf&) = delete;
    ParallelGzipStreambuf& operator=(const ParallelGzipStreamBuf&) = delete;
    ~ParallelGzipStreambuf() override = default;

    void set_ostream(std::ostream& os);
    void set_buffer(std::size_t buffer_size);
    void set_compression(int compression_level);
    void set_threads(int num_threads);
    void set_chunk_size(std::size_t chunk_size_);
    void readjust_buffer_from_threads();
    
  protected:
    int_type overflow(int_type ch) override;
    int sync() override;

  private:
    int flush();
    void start_thread();
    void worker_thread();
    std::vector<char> compress_data(const std::vector<char>& data);

    std::ostream* ostream_ = nullptr;
    std::vector<char> buffer_;
    std::size_t chunk_size_ = 1024 * 32;
    int compression_;
    int threads_;
    bool stop_worker_ = false;

    std::mutex queue_mutex_;
    std::condition_variable queue_cond_;
    std::queue<std::vector<char>> work_queue_;
    std::vector<std::thread> workers_;
    std::mutex writeMutex_;  // Only in ostream
  };
    

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

    int flushBuffer()
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
      
    std::vector<char> compressData(const std::vector<char>& input, const bool last)
    {
      std::vector<char> compressed_output; // output data
      std::vector<char> output_buffer;
      output_buffer.resize(chunk_size_);
      // uLongf destLen = compressBound(input.size());
      // compressed.resize(destLen);
        
      z_stream comp_stream;
      // unsigned char comp_input[CHUNK];
      comp_stream.zalloc = Z_NULL;
      comp_stream.zfree = Z_NULL;
      comp_stream.opaque = Z_NULL;

      // for parameters see https://neacsum.github.io/zlib/group__adv.html#gae501d2862c68d17b909d6f1c9264815c
      int comp_obj = deflateInit2(
        // strm= 
        &comp_stream, 
        // can be anything between 0..9
        // 0 is no compression, 1 is best speed, 9 is best compression
        // level = 
        9, 
        //fixed
        //method = 
        Z_DEFLATED, 
        // +16 for gzipped | 15 can be anything between 8..15
        // windowBits = 
        15+16,  
        // anything between 1..9, standart is 8
        // 1 is min mem, slow, low compression rate
        // 9 is max mem, optimal speed, high compression rate,
        // memLevel = 
        9,
        // depending on the data, this could be optimized, look in documentation
        // strategy = 
        Z_DEFAULT_STRATEGY
        );
        

      assert(comp_obj == Z_OK);
      comp_stream.avail_in = input.size();
      comp_stream.next_in = reinterpret_cast<unsigned char *>(const_cast<char *>(input.data()));
        
        // if (ferror(input)) {
        //     deflateEnd(&comp_stream);
        //     return Z_ERRNO;
        // }
        // int flush;
        // unsigned have;
        // flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
      int mode = last ? Z_FINISH : Z_SYNC_FLUSH;
      do {
        comp_stream.avail_out = output_buffer.size();
        comp_stream.next_out = reinterpret_cast<unsigned char *>(const_cast<char *>(output_buffer.data()));
        //documentation for deflate https://neacsum.github.io/zlib/group__basic.html#gaedba3a94d6e827d61b660443ae5b9f09
        // have = chunk_size_ - comp_stream.avail_out ;
        
        comp_obj = deflate(&comp_stream, mode);
        assert(comp_obj != Z_STREAM_ERROR);
        compressed_output.insert(
          compressed_output.end(), 
          output_buffer.begin(), 
          output_buffer.begin() + (output_buffer.size() - comp_stream.avail_out));
        } while (comp_stream.avail_out == 0);
        // have = chunk_size_ - comp_stream.avail_out;
        // if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
        //     (void)deflateEnd(&comp_stream);
        //     return Z_ERRNO;
        // }
      // assert(comp_stream.avail_in == 0);   
      deflateEnd(&comp_stream);  
      // assert(comp_obj == Z_STREAM_END);

        // if (compress(reinterpret_cast<const Bytef*>(input.data())(compressed.data()), &destLen,
        //             reinterpret_cast<const Bytef*>(input.data())(input.data()), input.size()) != Z_OK) {
        //     throw std::runtime_error("Compression failed");
        // }
      
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



