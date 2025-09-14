#include <iostream>
#include <fstream>
#include <streambuf>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <algorithm>

struct DataChunk
{
  char* data_;
  std::size_t size_;
  std::size_t id_;
  bool external{false};
};

// ---------------------------
// TaskCache: buffer rotation
// ---------------------------
class TaskCache 
{
  public:
    TaskCache(std::size_t buf_size, std::size_t n_buffer)
      : buffer_size_(buf_size), buffer_n_(n_buffer),next_id_(0)
    {
      for (std::size_t i = 0; i < n_buffer; ++i)
      {
        empty_buffer_queue_.push_back(std::unique_ptr<char[buf_size]>);
      }
    }

    // No copy (shared queues + mutexes unsafe)
    TaskCache(const TaskCache&) = delete;
    TaskCache& operator=(const TaskCache&) = delete;

    // Move is safe: transfer queues, invalidate source
    TaskCache(TaskCache&& other) noexcept 
    {
      other.stop();
      std::scoped_lock lock(other.task_lock_, other.buffer_lock_);
      task_queue_ = std::move(other.task_queue_);
      empty_buffer_queue_ = std::move(other.empty_buffer_queue_);
      buffer_size_ = other.buffer_size_;
      buffer_n_ = other.buffer_n_;
      next_id_ = other.next_id_;
    }

    TaskCache& operator=(TaskCache&& other) noexcept 
    {
      if (this != &other) 
      {
        stop();
        other.stop();
        //becuse after me hive will be moved, and hive automatically calls stop
        std::scoped_lock lock(task_lock_, buffer_lock_, other.task_lock_, other.buffer_lock_);
        task_queue_ = std::move(other.task_queue_);
        empty_buffer_queue_ = std::move(other.empty_buffer_queue_);
        buffer_size_ = other.buffer_size_;
        next_id_ = other.next_id_;
        start();
      }
      return *this;
    }

  ~TaskCache()
  { 
    std::unique_lock<std::mutex> lock(buffer_lock_);
    if (!stopping_) { stop(); }
    for (auto buf : empty_buffer_queue_) { delete[] buf; }
  }

  char* get_empty_buffer()
  {
    std::unique_lock lock(buffer_lock_);
    buffer_lock_.wait(lock, [this]
    {
      return !empty_buffer_queue_.empty() || stopping; 
    });

    if (stopping_) { return nullptr; }

    char* buf = empty_buffer_queue_.front();
    empty_buffer_queue_.pop_front();

    return buf;
  }
  void enqueue_task(const char* s, std::streamsize size_s)
  {
    
    task_queue_.push_back(DataChunk{const_cast<char*>(s), len, next_id_++});
  }

  void enqueue_task(char* buf, size_t len)
  {
    std::unique_lock lock(task_lock_);
    task_queue_.push_back(DataChunk{buf, len, next_id_++});
    task_go_.notify_all();
  }

  bool get_task(DataChunk& task)
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

  void give_back_buffer(char* buf)
  {
    std::unique_lock lock(buffer_lock_);
    empty_buffer_queue_.push_back(buf);
    buffer_free_.notify_all();
  }

  void stop()
  {
    std::lock_guard lock(buffer_lock_);
    stopping_ = true;
    buffer_free_.notify_all();
  }

  void start()
  {
    std::lock_guard lock(buffer_lock_);
    stopping_ = false;
    buffer_free_.notify_all();
  }

private:
    std::queue<DataChunk> task_queue_;
    std::queue<std::unique_ptr<char[]>> empty_buffer_queue_;
  
    std::mutex task_lock_;
    std::mutex buffer_lock_;

    std::condition_variable task_go_;
    std::condition_variable buffer_free_;

    std::atomic<bool> stopping_{false};

    std::size_t buffer_size_;
    std::size_t buffer_n_;
    std::size_t next_id_;
};


// ThreadWorkerHive: 
// worker_bees_ + queen_bee_writing_(writer)
template <typename Functor>
class ThreadWorkerHive 
{
  public:
    ThreadWorkerHive(TaskCache& t_cache, std::streambuf* out, size_t threads, Functor fn)
      : task_manager_(cache), writing_sink_(out), func_(std::move(fn))
      {
        start_workers(threads);
      }

    ~ThreadWorkerHive() { stop(); }

    // Copy is dangerous: delete
    ThreadWorkerHive(const ThreadWorkerHive&) = delete;
    ThreadWorkerHive& operator=(const ThreadWorkerHive&) = delete;

    // Move also dangerous because threads can’t be moved safely
    // try not to move ....
    ThreadWorkerHive(ThreadWorkerHive&& other)
    {
      stop();
      other.stop();

      task_manager_ = other.task_manager_;
      writing_sink_ = other.writing_sink_;

      func_ = std::move(other.func_);
      write_id_ = other.write_id_;

      other.clear();
      start_workers();
    }

    ThreadWorkerHive& operator=(ThreadWorkerHive&& other) noexcept 
    {
      if (this != &other) 
      {
        stop();
        other.stop();

        task_manager_ = other.task_manager_;
        writing_sink_ = other.writing_sink_;

        func_ = std::move(other.func_);
        write_id_ = other.write_id_;

        other.clear();
        start_workers();
      }
      return *this;
    }

    void restart(size_t threads)
    {
      stop();
      start_workers(threads);
    }

    void start_workers(size_t threads)
    {
      TaskCache.start();
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

    void reset(size_t threads, Functor&& fn)
    {
      stop();
      results_map_.clear();
      write_id_ = 0;
      func_ = std::move(fn);
      start_workers(threads);
    }

    void stop() 
    {
      task_manager_.stop();
      {
        std::unique_lock lock(bee_stop_);
        stopping_ = true;
        bee_go_.notify_all();
      }
      for (auto& t : worker_bees_) { if (t.joinable()) { t.join(); } }
      
      worker_bees_.clear();

      if (queen_bee_writing_.joinable()) { queen_bee_writing_.join(); }
    }

  private:
    std::vector<std::thread> worker_bees_;
    std::thread queen_bee_writing_;
    Functor func;

    std::unordered_map<std::size_t, <std::unique_ptr<char[], std::size_t>> results_map_;
    
    std::condition_variable bee_go_;
    std::mutex bee_stop_;

    alignas(64) size_t write_id_;

    TaskCache& TaskCache;
    std::streambuf* writing_sink_;
    
    std::atomic<bool> stopping_{true};

    void worker_loop() 
    {
      DataChunk task;
      while (task_manager_.get_task(task)) 
      {
        char* res = task_manager_.get_empty_buffer();
        func_(task.data_, task.size_, res);
        
        {
          std::lock_guard lock(bee_stop_);
          results_map_[task.id_] = std::make_pair(res, task.size_);
          bee_go_.notify_one();
        }
        task_manager_.give_back_buffer(task.data_);
      }
    }

    void writer_loop()
    {
      while (true)
      {
        std::pair<char*, std::size_t> res;
        {
          std::unique_lock lock(bee_stop_);
          bee_go_.wait(lock, [this]
          {
            return stopping_ || results_map_.count(write_id_); 
          });

          if (stopping_ && results_map_.empty()) { break; }

          auto it = results_map_.find(write_id_);
          if (it == results_map_.end()) { continue;}

          res = std::move(it->second);
          results_map_.erase(it);
          write_id_++;
        }

        writing_sink_->sputn(res.first, res.second);
        task_manager_.give_back_buffer(res.first);
      }
      writing_sink_->pubsync();
    }

}; // end class

