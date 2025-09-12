#pragma once 

#include <iostream>

#include "ParallelStreambufModifier.h"

class ParallelOstreamModifier : public std::ostream
{
  public:
    ParallelOstreamModifier(std::ostream &os, const int threads = 1)
      : std::ostream(nullptr), buffer_(os, threads)
    {
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
        std::ostream::operator=(std::move(other_os));
        buffer_ = std::move(other_os.buffer_);
        rdbuf(&buffer_);
      }
      return *this;
    }

    //delete Copy constructors
    ParallelOstreamModifier(const ParallelOstreamModifier&) = delete;
    ParallelOstreamModifier& operator=(const ParallelOstreamModifier&) = delete;

    // not sure if necessary or default c++ can handle that
    ~ParallelOstreamModifier() override
    {
      delete buffer_;
    }
    
  private:
      ParallelStreambufModifier buffer_;
};