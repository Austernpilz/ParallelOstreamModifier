#include <streambuf>
#include <ostream>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <memory>
#include <limits>

#ifdef _OPENMP
  #include <omp.h>
#endif

// Factory set up, that handles all input and set up
template <typename Functor>
class Streambuf_tFactory 
{
  public:
    Streambuf_tFactory() = default;
    ~Streambuf_tFactory() = default;

    
    

    // get/set defaults
    Config get_config() { return config_; }
    void set_config(Config c) { config_ = c; }

    std::size_t get_mem_size() { return config_.memory_size; }
    void set_mem_size(std::size_t ms) { config_.memory_size = ms; }

    std::size_t get_buffer_size() { return config_.buffer_size; }
    void set_buffer_size(std::size_t bs) { config_.buffer_size = bs; }

    std::size_t get_pool_size() { return config_.pool_size; }
    void set_pool_size(std::size_t ps) { config_.pool_size = ps; }

    int get_threads() { return config_.threads; }
    void set_threads(int t) { config_.threads = t; }

    Functor get_function() { return function_; }
    void set_function(Functor f) { function_ = f; }
    void set_function(const char* flag[]);

    bool init()
    {
      if(config_.memory_size > 0 &&
         config_.buffer_size > 0 &&
         conifig_.pool_size > 0 &&
         config._threads > 0)
      {
        return true;
      }
      else { return build_internally(); }
    }

    bool build_internally()
    {
      if (config_.threads == 0) { set_num_threads_from_external(); }

      if (config_.pool_size == 0) { set_pool_size_from_external(); }

      if (config_.memory_size == 0) { set_memory_size_from_external(); }

      if (config_.buffer_size == 0) { set_buffer_size_from_external(); }
      
      return config_.memory_size > 0 && config_.buffer_size > 0 && conifig_.pool_size > 0 && config._threads > 0;
    }

     

    // factory make function
    std::unique_ptr<StreamBuffer_t<Functor>> make(
        std::ostream& os, Functor fn, const Config& cfg = Config{}) const 
    {
        return std::make_unique<StreamBuffer_t<Functor>>(
            os, cfg.buffer_size, cfg.threads, cfg.pool_size, fn
        );
    }

    // convenience overload: read env first
    std::unique_ptr<StreamBuffer_t<Functor>> make_from_env(
        std::ostream& os, Functor fn) const 
    {
        return make(os, fn, get_config());
    }

    // // Example of "internal functors"
    // struct Identity 
    // {
    //   std::vector<unsigned char> operator()(std::vector<unsigned char>&& in) {
    //       return std::move(in); // no-op
    // }
    // };
    private:
      struct Config 
      {
        // 0 so i can check for initialization
        std::size_t memory_size{0};
        std::size_t buffer_size{0}; 
        std::size_t pool_size{0};
        int threads{0};
      };
      Config config_{};

      void set_memory_size_from_external()
      {
        if (const char* env = std::getenv("STREAMBUF_T_MEM_SIZE_KB"))
        {
          config_.memory_size = std::stoul(env) * 1024;
          return;
        }

        if (config.buffer_size && (config_.pool_size || config_.threads))
        {
          if (!config_.pool_size) { set_pool_size_from_external(); }
          config_.memory_size = config.buffer_size * config_.pool_size;
        }
        else { config_.memory_size = 10 * 1024 * 1024;} //10mb as a standard
      }

      void set_num_threads_from_external()
      {
        if (const char* env = std::getenv("STREAMBUF_T_THREADS"))
        {
          config_.threads = std::stoi(env);
          return;
        }
        #ifdef _OPENMP
          config._threads = omp_get_num_threads();
        #else
          config._threads = std::max(std::thread::hardware_concurrency(), 1);
        #endif
      }

      void set_pool_size_from_external()
      {
        if (const char* env = std::getenv("STREAMBUF_T_BUFFER_POOL"))
        {
          config_.pool_size = std::stoul(env);
          return;
        }
        else
        {
          // one for every thread, plus 1 for streambuf, 2 for cycling
          config_.pool_size = static_cast<std::size_t>(config_.threads) + 3
        }
      }

      void set_buffer_size_from_external()
      {
        if (const char* env = std::getenv("STREAMBUF_T_BUFFER_SIZE"))
        {
          config_.buffer_size = std::stoul(env);
          return;
        }
        if (config.memory_size &&)
        {
          if (!config_.pool_size) { set_pool_size_from_external(); }
          config_.memory_size = config.buffer_size * config_.pool_size;
        }
        else { config_.memory_size = 10 * 1024 * 1024;} //10mb as a standard
      }
      
    struct Gzip {
        int level;
        std::vector<unsigned char> operator()(std::vector<unsigned char>&& in) {
            GzipCompressor c(level);
            return c(std::move(in));
        }
    };
};

template <typename Functor>
class Streambuf_tFactory
{
  Streambuf_tFactory() = default;
  
  set();
  make();
  
  std::numeric_limits<int>::max(); // is the maximung buffer_size
  StreamBuffer_t make_processing_ostream_zero_copy(std::ostream &os, size_t buffer_size_, size_t threads, size_t pool_size, Functor func)
  {
    return std::make_unique<StreamBuffer_t<Functor>>(os, buffer_size_, threads, pool_size, func_);
  }


std::vector<unsigned char> GzipCompressor::operator()(std::vector<unsigned char> &&input) 
{
  std::vector<unsigned char> compressed_output;
  std::vector<unsigned char> zbuffer(compression_chunk_size_);

  z_stream strm{};
  if (deflateInit2(&strm, compression_level_, Z_DEFLATED,
                    15 + 16, // +16 = gzip header/trailer
                    compression_memory_level_,
                    compression_strategy_) != Z_OK)
  {
    throw std::runtime_error("deflateInit2 failed");
  }

  strm.next_in = reinterpret_cast<Bytef*>(input.data());
  strm.avail_in = static_cast<uInt>(input.size());

  int ret;
  do {
    strm.next_out = reinterpret_cast<Bytef*>(zbuffer.data());
    strm.avail_out = static_cast<uInt>(zbuffer.size());

    ret = deflate(&strm, strm.avail_in ? Z_NO_FLUSH : Z_FINISH);
    if (ret == Z_STREAM_ERROR)
    {
      deflateEnd(&strm);
      throw std::runtime_error("deflate failed");
    }
    std::size_t have = zbuffer.size() - strm.avail_out;
    compressed_output.insert(compressed_output.end(),
                              zbuffer.data(), zbuffer.data() + have);
  } while (ret != Z_STREAM_END);

  deflateEnd(&strm);
  return compressed_output;
}

}

template <typename Functor>
//streambuf or basic_streambuf?
class StreamBuffer_t : public std::streambuf
{
  public:
    // Constructor
    StreamBuffer_t(std::ostream* os, size_t buffer_size_, size_t pool_size, int threads, Functor func)
      : source_os_(os), writing_sink_(os->rdbuf()), tasktask_(buffer_size_, pool_size),
      hive_(tasktask_, writing_sink_, threads, func), buffer_size_(buffer_size_)
    {
      current_buffer_ = tasktask_.get_empty_buffer();
      setp(current_buffer_, current_buffer_ + buffer_size_);
      source_os_.rdbuf(this);
    }

    // delete copy
    StreamBuffer_t(const StreamBuffer_t&) = delete;
    StreamBuffer_t& operator=(const StreamBuffer_t&) = delete;

    // Move should also be avoided, but is possible
    StreamBuffer_t(StreamBuffer_t&& other) noexcept
      : source_os_(other.source_os_), writing_sink_(other.writing_sink_),
      tasktask_(std::move(other.tasktask_)), hive_(std::move(other.hive_)),
      buffer_size_(other.buffer_size_), current_buffer_(other.current_buffer_) 
    {
      //reset pointers
      char* new_base = current_buffer_;
      setp(new_base, new_base + buffer_size_);
      pbump(static_cast<int>(other.pptr() - other.pbase()));
      source_os_->rdbuf(this);
      // invalidate old pointers
      other.current_buffer_ = nullptr;
      other.writing_sink_ = nullptr;
      other.source_os_ = nullptr;
    }

    StreamBuffer_t& operator=(StreamBuffer_t&& other) noexcept 
    {
      if (this != &other) 
      {
        sync();
        source_os_ = other.source_os_;
        writing_sink_ = other.writing_sink_;
        tasktask_ = std::move(other.tasktask_);
        hive_ = std::move(other.hive_);
        current_buffer_ = other.current_buffer_;
        
        buffer_size_ = other.buffer_size_;
        current_buffer_ = other.current_buffer_;
        
        other.current_buffer_ = nullptr;
        other.writing_sink_ = nullptr;
        other.source_os_ = nullptr;
      }
      return *this;
    }

  ~StreamBuffer_t()
  {
    sync();
    source_os_.rdbuf(writing_sink_);
  }

  protected:
    
    int sync() override 
    {
        flush_buffer();
        return 0;
    }

    std::streambuf::int_type StreamBuffer_t::overflow(int_type ch) override
    {
      if (ch != traits_type::eof()) 
      {
        if (pptr() == epptr()) { flush_buffer(); }
        *pptr() = static_cast<char>(ch);
        pbump(1);
      } 
      else { flush_buffer(); }
      return ch;
    }

    char* get_data() const { return buffer_; }
    std::size_t get_size() const { return pptr() - pbase(); }
    std::streamsize get_available_space() const { return epptr() - pptr(); }

private:
  std::ostream* source_os_;
  std::streambuf* writing_sink_;
  TaskCache tasktask_;
  ThreadWorkerhive_<Functor> HoneyResultFactory_;

  size_t buffer_size_;
  char* current_buffer_;

  void flush_buffer()
  {
    size_t size = get_size();
    if (size == 0) { return; }
    tasktask_.enqueue_task(DataChunk{current_buffer_, size});
    current_buffer_ = tasktask_.get_empty_buffer();
    setp(current_buffer_, current_buffer_ + buffer_size_);
  }


    std::streamsize xsputn(const char* s, std::streamsize count) override 
    {
      std::streamsize written = 0;
      std::streamsize available_space = get_available_space();

      if (count <= available_space)
      {
        std::memcpy(pptr(), s, count);
        pbump(static_cast<int>(count));
        if (get_available_space() == 0) { flush_buffer(); }
        return count;
      }

      if (available_space > 0) 
      {
        std::memcpy(pptr(), s, available_space);
        pbump(static_cast<int>(available_space));
        flush_buffer();
        s += remain;
        count -= remain;
        written += remain;
      }

      while (count >= static_cast<std::streamsize>(buffer_size_))
      {
        tasktask_.enqueue_task(s, buffer_size_);
        s += buffer_size_;
        count -= buffer_size_;
        written += buffer_size_;
      }

      if (count > 0)
      {
        std::memcpy(pptr(), s, n);
        pbump(static_cast<int>(count));
        written += count;
      }
      return written;
    }

}; // end class streambuffer_t

    




