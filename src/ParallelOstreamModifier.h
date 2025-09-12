#pragma once 

#include <iostream>
#include <functional>
#include <vector>

#include "ParallelStreambufModifier.h"

class ParallelOstreamModifier : public std::ostream
{
  public:
    ParallelOstreamModifier(std::ostream &os, const int threads = 1)
      : std::ostream(buffer_), (os), buffer_(threads)
    {
      buffer_.set_ostream(os);
      rdbuf(&buffer_);
    }
    
    // Move constructor
    ParallelOstreamModifier(ParallelOstreamModifier&& other_os) noexcept
      : std::ostream(std::move(other_os)), buffer_(std::move(other_os.buffer_))
    {
      rdbuf(&buffer_);
    }

    // Move assignment
    ParallelOstreamModifier& operator=(ParallelOstreamModifier&& other_os) noexcept 
    {
      if (this != &other_os)
      {
        std::ostream::operator=(std::move(other));
        underlying_ = other.underlying_;
        buffer_ = std::move(other.buffer_);
        rdbuf(&buffer_);
      }
      return *this;
    }

    //delete Copy constructors
    ParallelOstreamModifier(const ParallelOstreamModifier&) = delete;
    ParallelOstreamModifier& operator=(const ParallelOstreamModifier&) = delete;

    // not sure if necessary or default c++ can handle that
    ~ParallelOstreamModifier() override {} = default;
    
    void set_mod_function(std::function<std::vector<char>(const std::vector<char>&)> fn_mod) 
    {
      buffer_.set_mod_function(fn_mod);
    }
    // std::function<std::vector<char>(const std::vector<char>&)> get_mod_function() const
    // {
    //   return buffer_.get_mod_function();
    // }

    void set_ostream(std::ostream &new_os)
    {
      buffer_.set_ostream(new_os);
    }

  // std::ostream get_ostream() const
  // {
  //   return buffer_.get_ostream();
  // }

  void set_threads(const int threads)
  {
    buffer_.set_threads(threads);
  }

  // int get_threads() const
  // {
  //   return buffer_.get_threads();
  // }
  void set_continues_write(const bool write)
  {
    buffer_.set_continues_write(write);
  }

  void set_buffer_size(std::size_t &buffer_size)
  {
    buffer_.set_buffer_size(buffer_size);
  }
  // std::size_t get_buffer_size() const
  // {
  //   return buffer_.get_buffer_size();
  // }
    
  ParallelOstreamModifier& operator<<(const std::vector<char>& data)
  {
    write(data.data(), data.size());
    return *this;
  }

  ParallelOstreamModifier& operator<<(char c)
  {
    write(&c, 1);
    return *this;
  }

  private:
    std::ostream& underlying_;
    ParallelStreambufModifier buffer_;
};