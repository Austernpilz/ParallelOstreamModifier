#pragma once

#include ParallelStreambufModifier.h

namespace ParallelStreambufModifier
{
  protected:
    int_type overflow(int_type ch) override
    {
      if (ch != traits_type::eof()) 
      {
        if (pptr() == epptr())
        {
          flush_buffer()
        }
        *pptr() = static_cast<char>(ch);
        pbump(1);
      } 
      else
      {
        flush_buffer()
      }
      return ch;
    }

    int sync() override
    {
      flush_buffer()
      modifier_.flush_to(ostream_);
      return 0;
    }

    std::streamsize xsputn(const char* s, std::streamsize s_size) override
    {
      std::streamsize available_sapce = epptr() - pptr();
      if (s_size <= available_sapce) 
      {
        std::memcpy(pptr(), s, s_size);
        pbump(static_cast<int>(s_size));
      }
      else 
      {
        flush_buffer();
        if (s_size > buffer_.size()) 
        {
          enqueue_task(std::vector<char>(s, s + s_size));
        }
        else
        {
          std::memcpy(pbase(), s, s_size);
          pbump(static_cast<int>(s_size));
        }
      }
      return s_size
    }

  private:
    void flush_buffer()
    {
      modifier_.flush_to(ostream_);
      std::ptrdiff_t size = pptr() - pbase();
      if (size <= 0) { return; }
      std::vector<char> data;
      data.insert(data.end(), pbase(), pbase() + size);
      modifier_.enqueue_task(std::move(data));

      // reset put pointers
      setp(pbase(), epptr());
      pbump(0);
    }
}