#include "ParallelDataModifier.h"


void ParallelDataModifier::enqueue_task(std::vector<char>&& data)
{
  flush();
  if (data.size() == 0) 
  {
    return;
  }
  
  std::deque<ThreadTask> new_tasks;
  std::deque<ThreadFuture> new_results;
  std::size_t data_chunk_size = data.size() / static_cast<std::size_t>(threads_);
  
  if (data_chunk_size == 0)
  {
    data_chunk_size = data.size();
  }

  std::size_t index = 0;
  while (index < data.size())
  {
    std::size_t len = std::min(data_chunk_size, data.size() - index);
    bytes_ chunk(data.begin() + index, data.begin() + index + len);
    index += len;
    
    uint64_t id = current_id++;
    //lambda expression for my task
    std::packaged_task<bytes_()> var_task(
      [&]()
      {
        return compute_fn_(std::move(chunk));
      });
    //save the result in a future seperatly
    std::future<bytes_> var_future = var_task.get_future();

    new_tasks.push_back(ThreadTask{id, std::move(var_task)});
    new_results.push_back(ThreadFuture{id, std::move(var_future)});
  }
  //save both in the queues
  {
    std::unique_lock<std::mutex> t_lock(task_deque_mutex_);
    for (auto &t : new_tasks)
    {
      tasks_.push_back(std::move(t));
    }
  }

  call_for_task_.notify_one();

  {
    std::unique_lock<std::mutex> r_lock(task_deque_mutex_);
    for (auto &r : new_results)
    {
      results_.push_back(std::move(r));
    }
  }
}

void ParallelDataModifier::flush_to(std::ostream &os)
{
  std::deque<ThreadFuture> local;
  {
    std::unique_lock<std::mutex> f_lock(result_deque_mutex_);
    local.swap(results_);
  }
  for (auto &result : local) 
  {
    bytes_ compressed = result.future_.get(); // wait here
    os.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
  }
}

ParallelDataModifier::bytes_ ParallelDataModifier::flush()
{
  std::deque<ThreadFuture> local;
  {
    std::unique_lock<std::mutex> f_lock(result_deque_mutex_);
    local.swap(results_);
  }
  bytes_ output;
  for (auto &result : local) 
  {
    bytes_ compressed = result.future_.get(); 
    output.insert(output.end(), compressed.data(), compressed.data() + compressed.size());
  }
  return output;
};

void ParallelDataModifier::worker_thread()
{
  while (true)
  {
    {
      bool todo = false;
      ParallelDataModifier::ThreadTask work_task;
      std::unique_lock<std::mutex> work_lock(task_deque_mutex_);
      call_for_task_.wait(work_lock, 
        [this]
        {
          return !tasks_.empty() || !stop_;
        });

      if (stop_ && tasks_.empty()) 
      {
        break;
      }
      if (!tasks_.empty()) 
      {
        work_task = std::move(tasks_.front());
        tasks_.pop_front();
        todo = true;
      }
      else 
      {
        continue;
      }
      if (todo)
      {
        work_task.task_();
      }
    }
    
  }
}

void ParallelDataModifier::writer_thread()
{
  while (true)
  {
    {
      bool todo = false;
      ParallelDataModifier::ThreadFuture work_task;
      std::unique_lock<std::mutex> write_lock(result_deque_mutex_);
      if (results_.empty() && stop_)
      {
        break;
      }
      if (!results_.empty()) 
      {
        work_task = std::move(results_.front());
        results_.pop_front();
        todo = true;
      }
      else 
      {
        continue;
      }
      if (todo)
      {
        bytes_ compressed = work_task.future_.get();
        os_.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
      }
    }
    
  }

}

