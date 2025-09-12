
#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <ostream>
#include <streambuf>
#include <utility>
#include <vector>
#include "GzipCompressor.h"
#include <atomic>

class ParallelDataModifier
{
  public:
    using vec_ch = std::vector<char>;
    // using mod_fn_ = std::function<bytes_(const bytes_&)>;
    
    ParallelDataModifier(int threads = 1) 
      : threads_(std::max(1, threads)),
        os_(os)
    {
      current_id_ = 0;
      next_id_ = 1;
    }

    ~ParallelDataModifier()
    {
      finish_up();
    };
    // std::function<std::vector<char>()> get_mod_function() const
    // {
    //   return compute_fn_;
    // }
    void set_threads(const int threads);
    void set_mod_function(std::function<vec_ch(const vec_ch&&)> mod_fn);
    void set_threads(int threads)  {threads_ = threds;}
    // void get_threads(int threads) const {return threads_}
    void enqueue_task(vec_ch &&data);

    void flush_to(std::ostream &os);

    void flush_to(std::ostream &os, bool continues_write);

    vec_ch flush();
    void finish_up();

  private:
    void worker_thread();
    void writer_thread();

    struct ThreadTask
    {
      ThreadTask() = default;

      ThreadTask(ThreadTask&& other_tt)
        : id_(other_tt.id), task_(std::move(other_tt.task_))
      {}
    
      ThreadTask(uint64_t id, std::packaged_task<vec_ch()>&& task)
        : id_(id), task_(std::move(std::move(task)))
      {}
      
      std::size_t id_{0};
      std::packaged_task<vec_ch()> task_;
    };

    struct ThreadFuture
    {
      ThreadFuture() = default;

      ThreadFuture(ThreadFuture&& other_tf)
        : id_(other_tf.id), task_(std::move(other_tf.task_))
      {}
    
      ThreadFuture(uint64_t id, std::packaged_task<vec_ch()>&& task)
        : id_(id), task_(std::move(std::move(task)))
      {}
      
      std::size_t id_{0};
      std::future<vec_ch> result_;
    };
    
    struct Worker
    {
      std::atomic<bool> running{false};
      std::thread thread;
    };

    std::deque<ThreadTask> tasks_;
    std::deque<ThreadFuture> results_;
    std::vector<Worker> workers_;
    std::vector<Worker> writers_;

    std::function<vec_ch(const vec_ch&)> compute_fn_;

    std::mutex task_deque_mutex_;
    std::mutex result_deque_mutex_;
    std::mutex Worker_mutex_;

    std::condition_variable call_for_task_;

    std::size_t current_id_ = 0;
    std::size_t next_id_ = 1;

    std::ostream &os_;
    int threads_ = 1;
    bool stop_ = false;
    bool unbalanced_ = false;
};
    