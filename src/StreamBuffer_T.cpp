#include "StreamBuffer_t.h"

// ThreadWorkerHive: 
// worker_bees_ + queen_bee_writing_(writer)
void ThreadWorkerHive::start_workers(size_t threads)
{
  {
    std::unique_lock lock(bee_stop_);
    stopping_ = false;
    bee_go_.notify_all();
  }

  //off_set the size, because one thread is dedicated for writing
  for (size_t i = 1; i < threads; ++i)
  {
      worker_bees_.emplace_back([this]{ worker_loop(); });
  }
  queen_bee_writing_ = std::thread([this]{ writer_loop(); });
}


void ThreadWorkerHive::reset(size_t threads, Task_function_&& fn)
{
  stop();
  results_.clear();
  write_id_ = 0;
  func_ = std::move(fn);
  start_workers(threads);
}

void ThreadWorkerHive::stop() 
{
  {
    std::unique_lock lock(bee_stop_);
    bee_go_.wait(lock, [this]
    {
      return results_.empty();
    });
    stopping_ = true;
    bee_go_.notify_all();
  }
  for (auto& t : worker_bees_) { if (t.joinable()) { t.join(); } }
  worker_bees_.clear();

  if (queen_bee_writing_.joinable()) { queen_bee_writing_.join(); }
}

void ThreadWorkerHive::worker_loop() 
{
  DataChunk task;
  while (task_manager_->get_task(task)) 
  {
    char* res = task_manager_->get_empty_buffer();
    size_t res_size = task_manager_->get_buffer_size();
    
    res_size = func_(task.data_, task.size_, res, res_size);
    {
      std::lock_guard lock(bee_stop_);
      results_[task.id_] = std::make_pair(res, res_size);
      bee_go_.notify_one();
    }
    task_manager_->give_back_buffer(task.data_);
  }
}

void ThreadWorkerHive::writer_loop()
{
  while (true)
  {
    std::pair<char*, size_t > res;
    {
      std::unique_lock lock(bee_stop_);
      bee_go_.wait(lock, [this]
      {
        return stopping_ || results_.count(write_id_); 
      });

      if (stopping_ && results_.empty()) { break; }

      auto it = results_.find(write_id_);
      if (it == results_.end()) { continue;}

      res = std::move(it->second);
      results_.erase(it);
      write_id_++;
    }

    writing_sink_->sputn(res.first, res.second);
    task_manager_->give_back_buffer(res.first);
  }
  writing_sink_->pubsync();
}

size_t  GzipCompressor::operator()(char* in, size_t  size_in, char* out, size_t  size_out)
{
  if (compressBound(size_in) > size_out) { throw std::runtime_error("can't safely compress"); }
  
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  size_t  read_in = 0;
  size_t  written_out = 0;
  size_t  next_block;
  int flush;
  int ret = deflateInit2(&strm, compression_level_, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
  
  if (ret != Z_OK)
  {
    deflateEnd(&strm); 
    throw std::runtime_error("deflateInit2 failed");
  }
  do
  {
    next_block = std::min(chunk_size_, size_in - read_in);
    flush = (next_block < chunk_size_) ? Z_FINISH : Z_NO_FLUSH;

    strm.next_in = reinterpret_cast<Bytef*>(in + read_in);
    strm.avail_in = static_cast<uInt>(next_block);
    read_in += next_block;
    do
    {
      strm.next_out = reinterpret_cast<Bytef*>(out + written_out);
      strm.avail_out = static_cast<uInt>(chunk_size_);

      ret = deflate(&strm, flush);
      if (ret == Z_STREAM_ERROR)
      {
        deflateEnd(&strm); 
        throw std::runtime_error("deflate failed");
      }

      written_out += (chunk_size_ - strm.avail_out);

    }
    while (strm.avail_out == 0);
  }
  while (ret != Z_STREAM_END);

  deflateEnd(&strm);

  return written_out;
}

bool StreamBuffer_t::flush_buffer()
{
  size_t size = get_size();
  if (size == 0) { return true; }
  enqueue_task(current_buffer_, size);
  current_buffer_ = get_empty_buffer();
  if (!current_buffer_) { return false; }
  setp(current_buffer_, current_buffer_ + buffer_size_);
  return true;
}

char* StreamBuffer_t::get_empty_buffer()
{
  std::unique_lock lock(buffer_lock_);
  buffer_free_.wait(lock, [this]
  {
    return !empty_buffer_queue_.empty() || stopping_; 
  });
  if (stopping_ && HRF_hive_.is_results_empty()) { return nullptr; }

  char* buf = empty_buffer_queue_.front();
  empty_buffer_queue_.pop_front();

  return buf;
}

void StreamBuffer_t::enqueue_task(const char* s, std::streamsize count)
{
  std::unique_lock<std::mutex> lock(task_lock_);
  task_queue_.push_back(DataChunk{const_cast<char*>(s), static_cast<size_t>(count), next_id_++});
  task_go_.notify_all();
}

void StreamBuffer_t::enqueue_task(char* buf, size_t len)
{
  std::unique_lock lock(task_lock_);
  task_queue_.push_back(DataChunk{buf, len, next_id_++});
  task_go_.notify_all();
}

void StreamBuffer_t::finish()
{
  flush_buffer();  
  stop();
  for (auto* buf : empty_buffer_queue_) { delete[] buf; }
  empty_buffer_queue_.clear();
}

bool StreamBuffer_t::get_task(DataChunk& task)
{
  std::unique_lock lock(task_lock_);
  task_go_.wait(lock, [this]
  { 
    return stopping_ || !task_queue_.empty(); 
  });

  if (task_queue_.empty()) { return false; }

  task = task_queue_.front();
  task_queue_.pop_front();
  return true;
}

void StreamBuffer_t::give_back_buffer(char* buf)
{
  std::unique_lock lock(buffer_lock_);
  if (empty_buffer_queue_.size() > pool_size_)
  {
    delete[] buf;
    return;
  }
  empty_buffer_queue_.push_back(buf);
  buffer_free_.notify_all();
}

void StreamBuffer_t::stop()
{
  {
    std::unique_lock<std::mutex> lock(task_lock_);
    stopping_ = true;
    task_go_.notify_all();
  }
  HRF_hive_.stop();
}

void StreamBuffer_t::start()
{
  std::lock_guard lock(buffer_lock_);
  stopping_ = false;
  HRF_hive_.start_workers(threads_);
  buffer_free_.notify_all();
}




// template <typename Task_function_>
// class Streambuf_tFactory 
// {
//   public:
//     using Buffer = StreamBuffer_t<Task_function_>;

//     std::unique_ptr<Buffer> make(std::ostream& os, Task_function_ fn)
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
// template <typename Task_function_>
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

//     Task_function_ get_function() { return function_; }
//     void set_function(Task_function_ f) { function_ = f; }
//     void set_function(const char* flag[]);


//     std::ostream make(std::ostream& os);
//     std::ostream make(std::ostream& os, Task_function_ fn);
//     std::ostream make(std::ostream& os, Task_function_ fn, Config& cfg);
//     std::unique_ptr<StreamBuffer_t<Task_function_>> make(std::ostream& os);
//     std::unique_ptr<StreamBuffer_t<Task_function_>> make(std::ostream& os, Task_function_ fn);
//     std::unique_ptr<StreamBuffer_t<Task_function_>> make(std::ostream& os, Task_function_ fn, Config& cfg); 
    
//     // factory make function
//     std::unique_ptr<StreamBuffer_t<Task_function_>> make(std::ostream& os, Task_function_ fn, Config& cfg) 
//     {
//         return std::make_unique<StreamBuffer_t<Task_function_>>(
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
//       Task_function_ func_;
     
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
//   // StreamBuffer_t make_processing_ostream_zero_copy(std::ostream &os, size_t buffer_size_, size_t threads, size_t pool_size, Task_function_ func)
//   // {
//   //   return std::make_unique<StreamBuffer_t<Task_function_>>(os, buffer_size_, threads, pool_size, func_);
//   // }
