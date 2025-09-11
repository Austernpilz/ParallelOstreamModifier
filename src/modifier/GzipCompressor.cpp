#include "GzipCompressor.h"

std::vector<char> GzipCompressor::operator()(std::vector<char> &input) const 
{
  std::vector<char> compressed_output;
  std::vector<char> zbuffer(compression_chunk_size_);

  z_stream strm{};
  if (deflateInit2(&strm, compression_level_, Z_DEFLATED,
                    15 + 16, // +16 = gzip header/trailer
                    compression_memory_level_,
                    compression_strategy_) != Z_OK)
  {
    throw std::runtime_error("deflateInit2 failed");
  }

  strm.next_in = reinterpret_cast<Bytef*>(input.data());
  strm.avail_in = static_cast<uInt>(input.size());

  do {
    strm.next_out = reinterpret_cast<Bytef*>(zbuffer.data());
    strm.avail_out = static_cast<uInt>(zbuffer.size());

    int ret = deflate(&strm, strm.avail_in ? Z_NO_FLUSH : Z_FINISH);
    if (ret == Z_STREAM_ERROR)
    {
      deflateEnd(&strm);
      throw std::runtime_error("deflate failed");
    }
    std::size_t have = zbuffer.size() - strm.avail_out;
    compressed_output.insert(compressed_output.end(),
                              zbuffer.data(), zbuffer.data() + have);
  } while (strm.avail_out == 0);

  deflateEnd(&strm);
  return compressed_output;
}
