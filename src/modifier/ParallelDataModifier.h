
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


class ParallelDataModifier
{
  public:
    using bytes_ = std::vector<char>;
    using mod_fn_ = std::function<bytes_(const bytes_&)>;
    
    ParallelDataModifier(std::ostream &os, int threads = 1) 
      : threads_(std::max(1, threads)),
        stop_(false),
        os_(os)
    {
      workers_.reserve(threads_);
      for (int i = 1; i < threads_; ++i) 
      {
        workers_.emplace_back(&ParallelDataModifier::worker_thread, this);
      }
      workers_.emplace_back(&ParallelDataModifier::writer_thread, this);
    }

    ~ParallelDataModifier() = default;

    void set_flags(const char* flag)
    {
      if (flag == "gzip")
      {
        set_mod_function(GzipCompressor());
      }
    };

    void set_mod_function(mod_fn_ fn)
    {
      compute_fn_ = fn;
    }

    void enqueue_task(bytes_ &&data);

    void flush_to(std::ostream &os);

    bytes_ flush();

  private:
    void worker_thread();
    void writer_thread();
    struct ThreadTask
    {
      uint64_t id;
      std::packaged_task<bytes_()> task_;
    };

    struct ThreadFuture
    {
      uint64_t id;
      std::future<bytes_> future_;
    };

    std::deque<ThreadTask> tasks_;
    std::deque<ThreadFuture> results_;
    std::vector<std::thread> workers_;
    mod_fn_ compute_fn_;

    std::mutex task_deque_mutex_;
    std::mutex result_deque_mutex_;

    std::condition_variable call_for_task_;

    uint64_t current_id;
    std::ostream &os_;
    int threads_;
    bool stop_;
};
    