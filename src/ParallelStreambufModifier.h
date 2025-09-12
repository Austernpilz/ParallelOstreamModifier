#pragma once 

#pragma once

#include <streambuf>
#include <vector>
#include <functional>
#include <mutex>
#include <ostream>

#include "ParallelDataModifier.h"

class ParallelStreambufModifier : public std::streambuf
{
  public:
    ParallelStreambufModifier(std::ostream &os, int threads)
      : ostream_(os), modifier_(threads)
    {
      setp(buffer_.data(), buffer_.data() + buffer_.size());
      if (threads < 3) { continues_write_ = false; }
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

    ParallelStreambufModifier(const ParallelStreambufModifier&) = delete;
    ParallelStreambufModifier& operator=(const ParallelStreambufModifier&) = delete;

    ~ParallelStreambufModfier()
    {
      sync();
    }

  protected:
    int sync() override;
    std::streamsize xsputn(const char* s, std::streamsize count) override;
    int_type overflow(int_type ch) override;
    
  private:
    bool flush_buffer(){};

    ParallelDataModifier modifier_;
    std::vector<char> buffer_{10 * 1024 * 1024};
    std::ostream& ostream_;
    bool continues_write_ = true;
};
