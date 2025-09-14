#include <streambuf>
#include <ostream>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <memory>
#include <limits>
#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <algorithm>
#include <zlib.h>
#include <unordered_map>


#ifdef _OPENMP
  #include <omp.h>
#endif

class StreamBuffer_t;  // forward declaration

struct DataChunk
{
  char* data_;
  size_t  size_;
  size_t  id_;
};

class ThreadWorkerHive 
{
  using Task_function_ = std::function<size_t(char*, std::size_t, char*& out, std::size_t& out_size)>;
  public:
    ThreadWorkerHive(StreamBuffer_t* t_cache, std::streambuf* out, size_t threads, Task_function_ fn)
      : task_manager_(t_cache), writing_sink_(out), func_(std::move(fn))
      {
        start_workers(threads);
      }

    ~ThreadWorkerHive() { stop(); }

    // Copy is dangerous: delete
    ThreadWorkerHive(const ThreadWorkerHive&) = delete;
    ThreadWorkerHive& operator=(const ThreadWorkerHive&) = delete;

    // Move also dangerous because threads can’t be moved safely
    // try not to move ....
    ThreadWorkerHive(ThreadWorkerHive&& other) = delete;
    // {
    //   stop();
    //   other.stop();
    //   task_manager_ = other.task_manager_;
    //   writing_sink_ = other.writing_sink_;
    //   func_ = std::move(other.func_);
    //   write_id_ = other.write_id_;
    //   start_workers();
    // }

    ThreadWorkerHive& operator=(ThreadWorkerHive&& other) = delete;
    // noexcept 
    // {
    //   if (this != &other) 
    //   {
    //     stop();
    //     other.stop();
    //     task_manager_ = other.task_manager_;
    //     writing_sink_ = other.writing_sink_;
    //     func_ = std::move(other.func_);
    //     write_id_ = other.write_id_;
    //     start_workers();
    //   }
    //   return *this;
    // }
    bool is_results_empty()
    {
      std::unique_lock<std::mutex> lock(bee_stop_);
      return results_.empy();
    }

    void start_workers(size_t threads);

    void reset(size_t threads, Task_function_&& fn);

    void stop();

  private:
    std::vector<std::thread> worker_bees_;
    std::thread queen_bee_writing_;
    Task_function_ func_;

    std::unordered_map<size_t, std::pair<char*, size_t>> results_;
    
    std::condition_variable bee_go_;
    std::mutex bee_stop_;

    alignas(64) size_t write_id_;

    StreamBuffer_t* task_manager_;
    std::streambuf* writing_sink_;
    
    bool stopping_{true};

    void worker_loop();

    void writer_loop();

}; // end class ThreadHive





struct GzipCompressor
{
  size_t  chunk_size_ = 256 * 1024; //256kb
  int compression_level_ = Z_DEFAULT_COMPRESSION;
  int compression_strategy_ = Z_DEFAULT_STRATEGY;
  int compression_memory_level_ = 8;

  void set_chunk_size(size_t  chunk) { chunk_size_ = chunk; }

  void set_compression_level(int lvl) { compression_level_ = lvl; }

  void set_strategy(int s) { compression_strategy_ = s; }

  void set_mem_level(int lvl) { compression_memory_level_ = lvl; }

  size_t operator()(char* in, size_t  size_in, char* out, size_t  size_out);

};


//streambuf or basic_streambuf?
class StreamBuffer_t : public std::basic_streambuf<char>
{
  using Task_function_ = std::function<size_t(char*, std::size_t, char*& out, std::size_t& out_size)>;

  public:
    // Constructor
    StreamBuffer_t(std::ostream* os, size_t buffer_size, size_t pool_size, int threads, Task_function_ func)
      : source_os_(os), writing_sink_(os->rdbuf()), 
      buffer_size_(buffer_size), pool_size_(pool_size), next_id_(0), threads_(threads),
      stopping_(false), HRF_hive_(this, writing_sink_, threads, func)
    {
      // for (size_t  i = 0; i < pool_size; ++i)
      // {
      //   empty_buffer_queue_.push_back(std::make_unique<char[]>(buffer_size));
      // }
      for (size_t i = 0; i < pool_size; ++i) 
      {
        char* buf = new char[buffer_size];
        empty_buffer_queue_.push_back(buf);
      }
      current_buffer_ = get_empty_buffer();
      setp(current_buffer_, current_buffer_ + buffer_size);
      source_os_->rdbuf(this);

      
    }

    // delete copy
    StreamBuffer_t(const StreamBuffer_t&) = delete;
    StreamBuffer_t& operator=(const StreamBuffer_t&) = delete;

    // Move should also be avoided, but is possible
    StreamBuffer_t(StreamBuffer_t&& other) = delete;
    // noexcept
    //   : source_os_(other.source_os_), writing_sink_(other.writing_sink_),
    //   HRF_hive_(std::move(other.HRF_hive_)),
    //   buffer_size_(other.buffer_size_), current_buffer_(other.current_buffer_) 
    // {
    //   //reset pointers
    //   char* new_base = current_buffer_;
    //   setp(new_base, new_base + buffer_size_);
    //   pbump(static_cast<int>(other.pptr() - other.pbase()));
    //   source_os_->rdbuf(this);
    //   // invalidate old pointers
    //   other.current_buffer_ = nullptr;
    //   other.writing_sink_ = nullptr;
    //   other.source_os_ = nullptr;
    // }

    StreamBuffer_t& operator=(StreamBuffer_t&& other) = delete;
    // noexcept 
    // {
    //   if (this != &other) 
    //   {
    //     sync();
    //     source_os_ = other.source_os_;
    //     writing_sink_ = other.writing_sink_;
    //     HRF_hive_ = std::move(other.HRF_hive_);
    //     current_buffer_ = other.current_buffer_;
        
    //     buffer_size_ = other.buffer_size_;
    //     current_buffer_ = other.current_buffer_;
        
    //     other.current_buffer_ = nullptr;
    //     other.writing_sink_ = nullptr;
    //     other.source_os_ = nullptr;
    //   }
    //   return *this;
    // }

    ~StreamBuffer_t()
    {
      sync();
      stop();
      for (auto* buf : empty_buffer_queue_) {delete[] buf; }
      source_os_->rdbuf(writing_sink_);
    }

    char* get_data()  { return current_buffer_; }
    size_t  get_size()  { return pptr() - pbase(); }
    std::streamsize get_available_space()  { return epptr() - pptr(); }
    size_t get_buffer_size()  { return buffer_size_; }
    bool is_buffer_empty() { return get_size() == buffer_size_; }
    
    bool is_task_queue_empty()
    { 
      std::unique_lock<std::mutex> lock(task_lock_);
      return task_queue_.empty()
    }

char* get_empty_buffer();

  void enqueue_task(const char* s, std::streamsize count);

  void enqueue_task(char* buf, size_t len);

  bool get_task(DataChunk& task);

  void give_back_buffer(char* buf);
  void finish();

  protected:
        
    int sync() override 
    {
      if (flush_buffer()) { return 0;}
      return -1;
    }

    int_type overflow(int_type ch) override
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
      else
      {
        flush_buffer();
        while (count >= buffer_size_)
        {
          enqueue_task(s, buffer_size_);
          s += buffer_size_;
          count -= buffer_size_;
          written += buffer_size_;
        }
        if (count > 0)
        {
          std::memcpy(pptr(), s, count);
          pbump(static_cast<int>(count));
          written += count;
        }
        return written;
      }
    }

  

private:

  bool flush_buffer();
  void stop();
  void start();

  //member var
  std::ostream* source_os_;
  std::streambuf* writing_sink_;

  std::deque<DataChunk> task_queue_;
  std::deque<char*> empty_buffer_queue_;

  std::mutex task_lock_;
  std::mutex buffer_lock_;

  std::condition_variable task_go_;
  std::condition_variable buffer_free_;

  //Honey Result Factory :D
  ThreadWorkerHive HRF_hive_;

  size_t  buffer_size_;
  size_t  pool_size_;
  size_t  next_id_;
  int threads_;

  char* current_buffer_;
  std::atomic<bool> stopping_{true};

}; // end class streambuffer_t



// ThreadWorkerHive: 
// worker_bees_ + queen_bee_writing_(writer)

// template <typename Functor>
// class Streambuf_tFactory 
// {
//   public:
//     using Buffer = StreamBuffer_t<Functor>;

//     std::unique_ptr<Buffer> make(std::ostream& os, Functor fn)
//     {
//       if (!build_internally()) {throw std::runtime_error("Config not valid");}
//       return std::make_unique<Buffer>(
//           &os, config_.buffer_size, config_.pool_size, config_.threads, fn
//       );
//     }

//   private:
//     Config config_;
//   };

// // Factory set up, that handles all input and set up
// template <typename Functor>
// class Streambuf_tFactory 
// {
//   public:
//     Streambuf_tFactory() = default;
//     ~Streambuf_tFactory() = default;

//     // get/set defaults
//     Config get_config() { return config_; }
//     void set_config(Config c) { config_ = c; }

//     size_t  get_mem_size() { return config_.memory_size; }
//     void set_mem_size(size_t  ms) { config_.memory_size = ms; }

//     size_t  get_buffer_size() { return config_.buffer_size; }
//     void set_buffer_size(size_t  bs) { config_.buffer_size = bs; }

//     size_t  get_pool_size() { return config_.pool_size; }
//     void set_pool_size(size_t  ps) { config_.pool_size = ps; }

//     int get_threads() { return config_.threads; }
//     void set_threads(int t) { config_.threads = t; }

//     Functor get_function() { return function_; }
//     void set_function(Functor f) { function_ = f; }
//     void set_function(const char* flag[]);


//     std::ostream make(std::ostream& os);
//     std::ostream make(std::ostream& os, Functor fn);
//     std::ostream make(std::ostream& os, Functor fn, Config& cfg);
//     std::unique_ptr<StreamBuffer_t<Functor>> make(std::ostream& os);
//     std::unique_ptr<StreamBuffer_t<Functor>> make(std::ostream& os, Functor fn);
//     std::unique_ptr<StreamBuffer_t<Functor>> make(std::ostream& os, Functor fn, Config& cfg); 
    
//     // factory make function
//     std::unique_ptr<StreamBuffer_t<Functor>> make(std::ostream& os, Functor fn, Config& cfg) 
//     {
//         return std::make_unique<StreamBuffer_t<Functor>>(
//             os, cfg.buffer_size, cfg.threads, cfg.pool_size, fn
//         );
//     }

    

    
//     private:
//       struct Config 
//       {
//         // 0 so i can check for initialization
//         size_t  memory_size{0};
//         size_t  buffer_size{0}; 
//         size_t  pool_size{0};
//         int threads{0};
//       };
//       Config config_{};
//       Functor func_;
     
//       bool init()
//       {
//         if(config_.memory_size > 0 &&
//             config_.buffer_size > 0 &&
//             config_.pool_size > 0 &&
//             config_.threads > 0)
//         {
//           return true;
//         }
//         else { return build_internally(); }
//       }

//       bool build_internally()
//       {
//         if (config_.threads == 0) { set_num_threads_from_external(); }

//         if (config_.pool_size == 0) { set_pool_size_from_external(); }

//         if (config_.memory_size == 0) { set_memory_size_from_external(); }

//         if (config_.buffer_size == 0) { set_buffer_size_from_external(); }
        
//         return config_.memory_size > 0 && config_.buffer_size > 0 && config_.pool_size > 0 && config_.threads > 0;
//       }

//       void set_memory_size_from_external()
//       {
//         if (const char* env = std::getenv("STREAMBUF_T_MEM_SIZE_KB"))
//         {
//           config_.memory_size = std::stoul(env) * 1024;
//           return;
//         }

//         if (config_.buffer_size && (config_.pool_size || config_.threads))
//         {
//           if (!config_.pool_size) { set_pool_size_from_external(); }
//           config_.memory_size = config_.buffer_size * config_.pool_size;
//         }
//         else { config_.memory_size = 10 * 1024 * 1024; } //10mb as a standard
//       }

//       void set_num_threads_from_external()
//       {
//         if (const char* env = std::getenv("STREAMBUF_T_THREADS"))
//         {
//           config_.threads = std::stoi(env);
//           return;
//         }
//         #ifdef _OPENMP
//           config_.threads = omp_get_num_threads();
//         #else
//           config_.threads = std::max(std::thread::hardware_concurrency(), 1);
//         #endif
//       }

//       void set_pool_size_from_external()
//       {
//         if (const char* env = std::getenv("STREAMBUF_T_BUFFER_POOL"))
//         {
//           config_.pool_size = std::stoul(env);
//           return;
//         }
//         else
//         {
//           // one for every thread, plus 1 for streambuf, 2 for cycling
//           config_.pool_size = static_cast<size_t >(config_.threads) + 3;
//         }
//       }

//       void set_buffer_size_from_external()
//       {
//         if (const char* env = std::getenv("STREAMBUF_T_BUFFER_SIZE"))
//         {
//           config_.buffer_size = std::stoul(env);
//           return;
//         }
//         else { config_.memory_size = 10 * 1024 * 1024;} //10mb as a standard
//       }
      
//     struct Gzip {
//         int level;
//         std::vector<unsigned char> operator()(std::vector<unsigned char>&& in) {
//             GzipCompressor c(level);
//             return c(std::move(in));
//         }
//     };
// };

  
//   // std::numeric_limits<int>::max(); // is the maximung buffer_size
//   // StreamBuffer_t make_processing_ostream_zero_copy(std::ostream &os, size_t buffer_size_, size_t threads, size_t pool_size, Functor func)
//   // {
//   //   return std::make_unique<StreamBuffer_t<Functor>>(os, buffer_size_, threads, pool_size, func_);
//   // }

