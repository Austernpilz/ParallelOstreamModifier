#pragma once

#include <cstring>  
#include <cctype> 

#include "ParallelOstreamModifier.h"

void ParallelStreambufModifier::set_mod_function(std::function<std::vector<char>(const std::vector<char>&)> fn_mod) 
{
  sync();
  modifier_.set_mod_function(fn_mod);
}

// std::function<std::vector<char>(const std::vector<char>&)> ParallelStreambufModifier::get_mod_function() const
// {
//   return modifier_.get_mod_function();
// }

// std::ostream ParallelStreambufModifier::get_ostream()
// {
//   return ostream_;
// }

void ParallelStreambufModifier::set_ostream(std::ostream& os)
{
  sync();
  ostream_ = os;
  modifier_.flush_to(os, continues_write_);
}

// std::ostream get_ostream() const
// {
//   return ostream_
// }

void ParallelStreambufModifier::set_buffer_size(const std::size_t &buffer_size)
{
  sync();
  buffer_.resize(buffer_size);
  setp(buffer_.data(), buffer_.data() + buffer_.size());
}

// void ParallelStreambufModifier::set_continues_write(bool continues_write)
// {
//   sync();
//   continues_write_ = continues_write;

//   //only start continues writing, when you have more than 3 threads
//   if (continues_write_ && ( modifier_.get_threads() > 3) )
//   {
//     modifier_.flush_to(ostream_, continues_write_);
//   }
//   else
//   {
//     continues_write_ = false;
//   }
// }

// bool ParallelStreambufModifier::get_continues_write() const
// {
//   return continues_write_;
// }

void ParallelStreambufModifier::set_threads(const int threads)
{
  modifier_.set_threads(threads);
}

// void ParallelStreambufModifier::get_threads() const
// {
//   modifier_.get_threads();
// }

int_type ParallelStreambufModifier::overflow(int_type ch) override
{
  if (ch != traits_type::eof()) 
  {
    if (pptr() == epptr())
    {
      flush_buffer();
    }
    *pptr() = static_cast<char>(ch);
    pbump(1);
  } 
  else
  {
    flush_buffer();
  }
  return ch;
}

int ParallelStreambufModifier::sync() override
{
  flush_buffer();
  modifier_.finish_up();
  modifier_.flush_to(ostream_, false);
  return 0;
}

std::streamsize ParallelStreambufModifier::xsputn(const char* s, std::streamsize s_size) override
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
  return s_size;
}

void ParallelStreambufModifier::flush_buffer()
{
  if (!continues_write_)
  {
    modifier_.flush_to(ostream_);
  }
  // else if (modifier_.get_unbalanced())
  // {
  //   modifier_.flush_to(ostream_, continues_write_);
  // }

  std::ptrdiff_t size = pptr() - pbase();
  if (size <= 0) { return; }

  std::vector<char> data;
  data.insert(data.end(), pbase(), pbase() + size);
  modifier_.enqueue_task(std::move(data));

  // reset put pointers
  setp(pbase(), epptr());
  pbump(0);
}
