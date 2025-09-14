#include <streambuf>
#include <ostream>
#include <vector>
#include <cstdint>

// ---------------------------
// Factory
// ---------------------------
template <typenamefunc_tor>
auto make_processing_ostream(std::ostream& os, size_t buffer_size_, size_t threads, size_t pool_size,func_torfunc_) {
    return std::make_unique<ProcessingStreambuf<Functor>>(os, buffer_size_, threads, pool_size,func_);
}

template <typename Functor>
class StreambufFactory
{

  auto make_processing_ostream_zero_copy(std::ostream& os, size_t buffer_size_, size_t threads, size_t pool_size,func_torfunc_)
  {
    return std::make_unique<StreamBuffer_t<Functor>>(os, buffer_size_, threads, pool_size, func_);
  }

}

template <typename Functor>
class StreamBuffer_t : public std::streambuf
{
  public:
    StreamBuffer_t(std::ostream& os, size_t buffer_size_, size_t threads, size_t pool_size, Functor func)
      : target(os), writing_buffer_(os.rdbuf()), TaskCache(buffer_size_, pool_size),
        hive(TaskCache, writing_buffer_, threads,func_), buffer_size_(buffer_size_)
    {
      current_buffer_ = tasktask_.get_empty_buffer();
      setp(current_buffer_, current_buffer_ + buffer_size_);
      target.rdbuf(this);
    }

  ProcessingStreambuf(std::ostream& os, size_t buffer_size_, size_t threads, size_t pool_size,func_torfunc_)
        : target(os), writing_buffer_(os.rdbuf()), TaskCache(buffer_size_, pool_size),
          hive(TaskCache, writing_buffer_, threads, func_), buffer_size_(buffer_size_)
    {
        setp(current_buffer_, current_buffer_ + buffer_size_);
        target.rdbuf(this);
    }
    
    
    ~StreamBuffer_t()
    {
      sync();
      target.rdbuf(writing_buffer_);
    }

  protected:

  int overflow(int c) override {
        if (c != EOF) {
            *pptr() = static_cast<char>(c);
            pbump(1);
        }
        flush_chunk();
        return c;
    }

    int sync() override {
        flush_chunk();
        return 0;
    }
    int_type overflow(int_type ch) override 
    {
      if (ch != traits_type::eof()) 
      {
        *pptr() = static_cast<char>(process_byte(static_cast<uint8_t>(ch)));
        pbump(1);
        if (pptr() >= epptr()) {
            // handle buffer full if needed
            return traits_type::eof();
        }
        return ch;
      }
      return traits_type::eof();
    }

    char* get_data() const { return buffer_; }
    std::size_t get_size() const { return pptr() - pbase(); }


private:
  std::ostream& os_wrapper_;
  std::streambuf* writing_buffer_;
  TaskCache tasktask_;
  ThreadWorkerHive<Functor> HoneyResultFactory_;

  size_t buffer_size_;
  char* current_buffer_;

  void flush()
  {
    size_t n = pptr() - pbase();
    if (n == 0) { return; }

    tasktask_.enqueue_task(DataChunk{current_buffer_, n});
    current_buffer_ = tasktask_.give_empty_buffer();
    setp(current_buffer_, current_buffer_ + buffer_size_);
  }

}



    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::streamsize written = 0;
        while (written < n) {
            std::streamsize space = epptr() - pptr();
            std::streamsize to_copy = std::min(space, n - written);
            std::memcpy(pptr(), s + written, to_copy);
            pbump(static_cast<int>(to_copy));
            written += to_copy;
            if (pptr() == epptr()) flush_chunk();
        }
        return written;
    }



// More Pipeline-Style
// Main Thread -- Working Bees -- Write Thread
// less overhead, make a factory whos sets stuff up. and gives you an streambuffer.
// 4 Klasse 
// Streambuf, for all input, output and shifting
// TaskCache und Task Flock fürs arbeiten und zwischenspeichern.

/ ---------------------------
// Streambuf: wraps ostream
// ---------------------------

class ProcessingStreambuf : public std::streambuf {
public:
    

    

protected:
    

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::streamsize written = 0;
        while (written < n) {
            std::streamsize space = epptr() - pptr();
            std::streamsize to_copy = std::min(space, n - written);
            std::memcpy(pptr(), s + written, to_copy);
            pbump(static_cast<int>(to_copy));
            written += to_copy;
            if (pptr() == epptr()) flush_chunk();
        }
        return written;
    }


    void flush_chunk() {
      size_t n = pptr() - pbase();
      if (n == 0) return;

      tasktask_.enqueue_task(current_buffer_, n);
      current_buffer_ = tasktask_.get_empty_buffer();
      setp(current_buffer_, current_buffer_ + buffer_size_);
    }
};

// ---------------------------
// Example Usage
// ---------------------------
int main() {
    std::ofstream file("output_map.txt", std::ios::binary);

    auto processor = make_processing_ostream(file, 1024, 4, 8,
        [](char* data, size_t n) -> std::vector<char> {
            std::vector<char> out(n);
            for (size_t i = 0; i < n; ++i)
                out[i] = std::toupper(static_cast<unsigned char>(data[i]));
            return out;
        });

    auto& out = *processor;
    out << "hello world\n";
    out << "this is a test\n";
}

    

protected:
    int overflow(int c) override {
        if (c != EOF) {
            *pptr() = static_cast<char>(c);
            pbump(1);
        }
        flush_chunk();
        return c;
    }

    int sync() override {
        flush_chunk();
        return 0;
    }



