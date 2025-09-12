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
      buffer_.resize(10 * 1024 * 1024)
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
      pbump(static_cast<int>(other_sb.pptr() - other_sb.pbase()));
      // reset other
      other_sb.setp(nullptr, nullptr);
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

    void set_mod_function(std::function<std::vector<char>(const std::vector<char>&)> fn_mod);
    void set_ostream(std::ostream& os);
    void set_threads(const int threads);
    void set_continues_write(bool c) {};
    
  protected:
    int sync() override;
    std::streamsize xsputn(const char* s, std::streamsize count) override;
    int_type overflow(int_type ch) override;
    
  private:
    bool flush_buffer(){};

    ParallelDataModifier modifier_;
    std::vector<char> buffer_;
    std::ostream& ostream_;
    bool continues_write_ = true;
};
