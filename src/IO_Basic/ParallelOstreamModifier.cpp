#include "ParallelOstreamModifier.h"

class ParallelOstreamModifier : public std::ostream
{
  void set_mod_function(std::function<std::vector<char>(const std::vector<char>&)> fn_mod) 
  {
    buffer_.set_mod_function(fn_mod);
  }

  void set_ostream(const std::ostream &new_os)
  {
    buffer_.set_ostream(new_os)
  }

  void set_threads(const int threads)
  {
    buffer_.set_threads(threads)
  }

  void set_continues_write(const bool write)
  {
    buffer_.set_continues_write(write)
  }
};
