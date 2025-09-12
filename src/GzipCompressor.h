#pragma once 

#include <cstddef>
#include <stdexcept>
#include <vector>
#include <zlib.h>
#include <vector>


struct GzipCompressor
{
  std::size_t compression_chunk_size_ = 256 * 1024; //256kb
  int compression_level_ = Z_DEFAULT_COMPRESSION;
  int compression_strategy_ = Z_DEFAULT_STRATEGY;
  int compression_memory_level_ = 8;

  void set_chunk_size(std::size_t chunk)
  {
    compression_chunk_size_ = chunk;
  }

  void set_compression_level(int lvl)
  {
    compression_level_ = lvl;
  }

  void set_strategy(int s)
  {
    compression_strategy_ = s;
  }

  void set_mem_level(int lvl)
  {
    compression_memory_level_ = lvl;
  }

  std::vector<char> operator()(const std::vector<char> &input) const;
};