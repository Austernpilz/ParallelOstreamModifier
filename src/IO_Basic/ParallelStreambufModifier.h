#pragma once 

#include <streambuf>
#include <vector>
#include <functional>

#include "ParallelDataModifier.h"

class ParallelStreambufModifier : public std::streambuf
{
  public:
    ParallelStreambufModifier(std::ostream &os, int nthreads)
      : ostream_(os), modifier_(nthreads)
    {
      setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

    // Move constructor
    ParallelStreambufModifier(ParallelStreambufModifier&& other_sb) noexcept
      : ostream_(other_sb.ostream_),
        modifier_(std::move(other_sb.modifier_)),
        buffer_(std::move(other_sb.buffer_))
    {
      setp(buffer_.data(), buffer_.data() + buffer_.size());
      pbump(static_cast<int>(other.pptr() - other.pbase()));
      // reset other
      other.setp(nullptr, nullptr);
    }

    // Move assignment
    ParallelStreambufModifier& operator=(ParallelStreambufModifier&& other_sb) noexcept
    {
      if (this != &other_sb)
      {
        sync();
        ostream_ = other_sb.ostream_;
        modifier_ = std::move(other_sb.modifier_);
        buffer_ = std::move(other_sb.buffer_);
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        pbump(static_cast<int>(other_sb.pptr() - other_sb.pbase()));
        other_sb.setp(nullptr, nullptr);
      }
      return *this;
    }

    ParallelStreambufModfier(const ParallelStreambufModfier&) = delete;
    ParallelStreambufModfier& operator=(const ParallelStreambufModfier&) = delete;

    ~ParallelStreambufModfier()
    {
      sync()
      delete modifier_
      delete buffer_
    }

    void set_mod_function(std::function<std::vector<char>(const std::vector<char>&)> fn_mod) 
    {
      modifier_.set_mod_function(fn_mod);
    }

    void set_ostream(std::ostream& os)
    {
      ostream_ = &os
    }

    void set_buffer_size(std::size_t buffer_size)
    {
      sync()
      buffer_.resize(buffer_size)
      setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

  protected:
    int sync() override;
    std::streamsize xsputn(const char* s, std::streamsize count) override;
    int_type overflow(int_type ch) override;
    
  private:
    bool flush_buffer(){};

    ParallelDataProcessing modifier_;
    std::vector<char> buffer_(10 * 1024 * 1024);
    std::ostream& ostream_;
    std::mutex write_mutex_;
};
