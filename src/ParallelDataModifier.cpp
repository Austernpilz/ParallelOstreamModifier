#include "ParallelDataModifier.h"


{
  using vec_ch = std::vector<char>;
  
  // bool ParallelDataModifier::get_unbalanced()
  // {
  //   return unbalanced_;
  // }

  void ParallelDataModifier::set_threads(int threads)
  {
    while(workers_.size() > threads)
    {
      stop_worker();
    }

    while(workers_.size() < threads)
    {
      start_worker();
    }
    
    threads_ = threads;
  }

  void ParallelDataModifier::set_mod_function(std::function<vec_ch(vec_ch&)> mod_fn)
  { 
    finish_up();
    compute_fn_ = mod_fn; 

    while(workers_.size() < threads_)
    {
      start_worker();
    }
  }

  void ParallelDataModifier::enqueue_task(vec_ch&& data)
  {
    if (data.size() == 0) 
    {
      return;
    }
    
    std::deque<ThreadTask> new_tasks;
    std::deque<ThreadFuture> new_results;

    std::size_t data_chunk_size = data.size() / static_cast<std::size_t>(threads_);
    std::size_t index = 0;

    while (index < data.size())
    {
      std::size_t len = std::min(data_chunk_size, data.size() - index);
      vec_ch chunk(data.begin() + index, data.begin() + index + len);
      
      index += len;
      uint64_t id = current_id++;

      //lambda expression for my task
      std::packaged_task<vec_ch()> var_task(
        [chunk=std::move(chunk), this]()
        {
          return compute_fn_(std::move(chunk));
        });

      //save the result in a future seperatly
      std::future<vec_ch> var_future = var_task.get_future();

      new_tasks.push_back(ThreadTask{id, std::move(var_task)});
      new_results.push_back(ThreadFuture{id, std::move(var_future)});

    }

    //save both in the queues
    {
      std::unique_lock<std::mutex> task_lock(task_deque_mutex_);
      for (auto &t : new_tasks)
      {
        tasks_.push_back(std::move(t));
        call_for_task_.notify_one();
      }
      size_t task_size = task_.size();
    }

    {
      std::unique_lock<std::mutex> result_lock(result_deque_mutex_);
      for (auto &r : new_results)
      {
        results_.push_back(std::move(r));
        call_for_result_.notify_one();
      }
      size_t result_size = task_.size()
      }
    }
    unbalanced_ = (task_size*2) < result_size
  }

  void ParallelDataModifier::flush_to(std::ostream &os, bool continues_write)
  {
    if (continues_write)
    {
      stop_worker();
      start_writer(os);
    }
    else
    {
      flush_to(os)
    }
  }

  void ParallelDataModifier::flush_to(std::ostream &os)
  {
    std::deque<ThreadFuture> local_results;
    {
      std::unique_lock<std::mutex> result_lock(result_deque_mutex_);
      local_results.swap(results_);
      std::size_t next_id = current_id - statlocal_results.size()
    }
  
    while(!local_results.empty())
    {
      for (auto &result : local) 
      {
        if ( result.id_ == next_id)
        {
          vec_ch data_chunk = result.future_.get(); 
          os.write(data_chunk.data(), static_cast<std::streamsize>(data_chunk.size()));
          ++next_id;
        }
      }
    }
  }

  vec_ch ParallelDataModifier::flush()
  {
    std::deque<ThreadFuture> local_results;
    {
      std::unique_lock<std::mutex> result_lock(result_deque_mutex_);
      local_results.swap(results_);
      std::size_t next_id = current_id - statlocal_results.size()
    }

    vec_ch output.resize(local_results.size());
  
    while(!local_results.empty())
    {
      for (auto &result : local) 
      {
        if ( result.id_ == next_id)
        {
          vec_ch data_chunk = result.future_.get(); 
          output.insert(output.end(), data_chunk.data(), data_chunk.data() + data_chunk.size());
          ++next_id;
        }
      }
    }
    

    return output;
  };

  void ParallelDataModifier::start_worker()
  {
    if (workers_.size() + writers_.size() >= threads_) { return; }
    if (stop_) { stop_ = false; }
    workers_.emplace_back(
      Worker{
      true, std::thread(&ParallelDataModifier::worker_thread, this, static_cast<int>(workers_.size()))
    });
  }

  void ParallelDataModifier::stop_worker()
  {
    {
      std::unique_lock<std::mutex> startstop(Worker_mutex_)
      if (workers_.size() < 0) { return; }
      int i = static_cast<int>(workers_.size()) -1;
      if (i < 1) { return; }
      workers_[i].running = false;
      startstop.unlock();
      if (workers_[i].thread.joinable()) { workers_[i].thread.join() };
      workers_.erase(workers_.begin() + i);
    }
  } 

  void ParallelDataModifier::start_writer(std::ostream &os)
  {
    if (workers_.size() + writers_.size() >= threads_) { stop_worker(); }
    {
      std::unique_lock<std::mutex> startstop(Worker_mutex_)
      writers.emplace_back(
        Worker{
        true, std::thread(&ParallelDataModifier::writer_thread, this, os, static_cast<int>(writers_.size()))
      });
    } 
  }

  void ParallelDataModifier::stop_writer()
  {
    {
      std::unique_lock<std::mutex> startstop(Worker_mutex_)
      if (writers_.size() < 0) { return; }
      int i = static_cast<int>(writers_.size()) -1;
      if (i < 1) { return; }
      writers_[i].running = false;
      startstop.unlock();
      if (w.thread.joinable()) { w.thread.join() };
      writers_.erase(writers_.begin() + i);
    }
  } 

  void ParallelDataModifier::worker_thread(int id)
  {
    Worker& w = workers_[id];
    while (w.running)
    {
      {
        std::unique_lock<std::mutex> task_lock(task_deque_mutex_);
        call_for_task_.wait(task_lock, 
          [this]
          {
            return !tasks_.empty() || !stop_;
          });

        if (stop_ && tasks_.empty()) { break; }

        if (tasks_.empty()) { continue; }
        else
        {
          work_task = std::move(tasks_.front());
          tasks_.pop_front();
        }
      }
      work_task.task_();
      call_for_result_.notify_one();
    }
  }

  void ParallelDataModifier::writer_thread(std::ostream &os, int id)
  {
    Worker& w = writers_[id];
    while (w.running)
    {
      {
        std::unique_lock<std::mutex> result_lock(result_deque_mutex_);
        call_for_result_.wait(result_lock, 
          [this]
          {
            return !tasks_.empty() || !stop_ || (next_id > current_id)
          });

        if (results_.empty() && stop_) { break; }
        if (next_id > current_id)
        {
          start_worker();
          stop_writer();
          continue;
        }
        if (!results_.empty()) 
        {
          ThreadFuture& found = results_.front();
          if (r.id_ == next_id_)
          {
            vec_ch result = found.future_.get();
            os.write(result.data(), static_cast<std::streamsize>(result.size()));
            ++next_id;
            results_.pop_front();
          }
          else
          {
            auto it = std::find_if(results_.begin(), results_.end(),
            [&](const ThreadFuture& r)
            {
              return r.id_ == next_id_;
            }); 

            if (it != results_.end())
            {
              ThreadFuture& found = *it;
              vec_ch result = found.future_.get();
              os.write(result.data(), static_cast<std::streamsize>(result.size()));
              ++next_id;
            }
          }
        }
      }
    }
  }

  void ParallelDataModifier::finish_up()
  {
    {
      while (true)
      {
        std::unique_lock<std::mutex> task_deque_mutex_(task_mutex_);
        if (tasks_.empty()) break;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      stop_ = true;
      while (!tasks_.empty()) { std::this_thread::sleep_for(std::chrono::seconds(1)) }
      while (workers_.size() > 1 ) {stop_worker()}
      while (workers_.size() > 1) {stop_worker()}
    }
  }
}


