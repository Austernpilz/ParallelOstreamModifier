

class TaskCache 
{
  public:
    TaskCache(std::size_t buf_size, std::size_t n_buffer)
      : buffer_size_(buf_size), buffer_n_(n_buffer),next_id_(0)
    {
      for (std::size_t i = 0; i < n_buffer; ++i)
      {
        empty_buffer_queue_.push_back(std::make_unique<char[]>(buf_size));
      }
    }

    // No copy (shared queues + mutexes unsafe)
    TaskCache(const TaskCache&) = delete;
    TaskCache& operator=(const TaskCache&) = delete;

    // Move is safe: transfer queues, invalidate source
    TaskCache(TaskCache&& other) noexcept 
    {
      other.stop();
      std::scoped_lock lock(other.task_lock_, other.buffer_free_);
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
        std::scoped_lock lock(task_lock_, buffer_free_, other.task_lock_, other.buffer_free_);
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
    stop();
  }

  

  

private:
    std::deque<DataChunk> task_queue_;
    std::deque<std::unique_ptr<char[]>> empty_buffer_queue_;
  
    std::mutex task_lock_;
    std::mutex buffer_lock_;

    std::condition_variable task_go_;
    std::condition_variable buffer_free_;

    std::size_t buffer_size_;
    std::size_t buffer_n_;
    
};




