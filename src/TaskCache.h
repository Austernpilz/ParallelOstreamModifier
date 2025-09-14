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
};

// ---------------------------
// TaskCache: buffer rotation
// ---------------------------
class TaskCache 
{
public:
  TaskCache(std::size_t buf_size, std::size_t n_buffer)
    : buffer_size__(buf_size), next_id_(0)
  {
    for (std::size_t i = 0; i < n_buffer; ++i)
    {
      empty_buffer_queue_.push_back(new char[buf_size]);
      empty_result_queue_.push_back(new char[buf_size]);
    }
  }

  ~TaskCache()
  { 
    for (auto buf : empty_buffer_queue_) { delete[] buf; }
    for (auto res : empty_result_queue_) { delete[] res; }
  }

  char* get_empty_buffer()
  {
    std::unique_lock lock(cache_lock_);
    cache_go_.wait(lock, [this]
    { 
      return !empty_buffer_queue_.empty() || stopping; 
    });

    if (stopping_) { return nullptr; }

    char* buf = empty_buffer_queue_.front();
    empty_buffer_queue_.pop_front();

    return buf;
  }

  void enqueue_task(char* buf, size_t len)
  {
    std::unique_lock lock(cache_lock_);
    task_queue_.push_back(DataChunk{buf, len, next_id_++});
    cache_go_.notify_all();
  }

  bool get_task(DataChunk& task)
  {
    std::unique_lock lock(cache_lock_);
    cache_go_.wait(lock, [this]
    { 
      return stopping_ || !task_queue_.empty(); 
    });

    if (task_queue_.empty()) { return false; }

    job = task_queue_.front();
    task_queue_.pop_front();
    return true;
  }

  void give_back_buffer(char* buf)
  {
    std::unique_lock lock(cache_lock_);
    empty_buffer_queue_.push_back(buf);
    cache_go_.notify_all();
  }

  void stop()
  {
    std::lock_guard lock(cache_lock_);
    stopping_ = true;
    cache_go_.notify_all();
  }

private:
    std::deque<DataChunk> task_queue_;
    std::deque<char*> empty_buffer_queue_;

    std::mutex cache_lock_;
    std::condition_variable cache_go_;
    std::atomic<bool> stopping_{false};

    size_t buffer_size_;
    size_t next_id_;
};


// ThreadWorkerHive: 
// worker_bees_ + queen_bee_writing_(writer)
template <typename Functor>
class ThreadWorkerHive 
{
  public:
    ThreadWorkerHive(TaskCache& t_cache, std::streambuf* streambuf_out, size_t threads,Functor fn)
      : task_manager_(t_cache), writing_buffer_(streambuf_out),func_(std::move(fn)), write_id_(0)
    {
      //start worker threads
      for (size_t i = 0; i < threads; ++i)
      {
        worker_bees_.emplace_back([this]{ worker_loop(); });
      }

      // start writer thread
      queen_bee_writing = std::thread([this]{ writer_loop(); });
    }

    ~ThreadWorkerHive()
    {
      TaskCache.stop();
      finish_up();

      for (auto& t : worker_bees_) 
      {
        if t.joinable() { t.join();}
      }
      queen_bee_writing_.join();
    }

  private:
    std::vector<std::thread> worker_bees_;
    std::thread queen_bee_writing_;
    Functor func;

    std::map<std::size_t, std::pari<char*, std::size_t>> results_map_;
    TaskCache& TaskCache;
    std::streambuf* writing_buffer_;
    
    std::condition_variable bee_go_;
    std::mutex bee_stop_;
    
    size_t write_id_;
    bool stopping_{false};

    void finish_up()
    {
      std::unique_lock lock(bee_stop_);
      stopping_ = true;
      bee_go_.notify_all();
    }

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
          bee_go_.notify_all();
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

        writing_buffer_->sputn(res.first, res.second);
        task_manager_.give_back_buffer(res.first);
      }
      writing_buffer_->pubsync();
    }

}; // end class

