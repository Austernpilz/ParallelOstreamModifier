#include <streambuf>
#include <ostream>
#include <vector>
#include <cstdint>

class StreamBuffer_T : public std::streambuf
{
  public:
    StreamBuffer_T(std::size_t capacity = 1024) 
    {
      buffer_.resize(capacity);
      setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

  protected
    int_type overflow(int_type ch) override 
    {
      if (ch != traits_type::eof()) 
      {
        *pptr() = static_cast<char>(process_byte(static_cast<uint8_t>(ch)));
        pbump(1);
        if (pptr() >= epptr()) {
            // handle buffer full if needed
            return traits_type::eof();
        }
        return ch;
      }
      return traits_type::eof();
    }

    
    const std::vector<uint8_t>& get_data() const { return buffer_; }
    std::size_t get_size() const { return pptr() - pbase(); }

private:
    std::vector<uint8_t> buffer_;
}


#include <thread>
#include <future>
#include <fstream>
#include <queue>

void parallel_gzip(std::istream& in, std::ostream& out, size_t block_size = 64*1024, bool use_simd = false) {
    std::vector<std::future<void>> futures;
    std::vector<Block> blocks;
    size_t block_index = 0;

    // Read blocks and launch threads
    while (in) {
        std::vector<uint8_t> buffer(block_size);
        in.read(reinterpret_cast<char*>(buffer.data()), block_size);
        size_t n = in.gcount();
        if (n == 0) break;

        buffer.resize(n);

        blocks.push_back({block_index++, std::move(buffer)});
        Block& blk = blocks.back();

        futures.push_back(std::async(std::launch::async, [&, blk_index = blocks.size()-1]() {
            process_block(blocks[blk_index], use_simd);
        }));
    }

    // Wait for all threads
    for (auto& f : futures) f.get();

    // Write blocks sequentially
    for (auto& blk : blocks)
        out.write(reinterpret_cast<char*>(blk.compressed.data()), blk.compressed.size());
}


// More Pipeline-Style
// Main Thread -- Working Bees -- Write Thread
// less overhead, make a factory whos sets stuff up. and gives you an streambuffer.
// 2 Klasse 
// Streambuf, for all input, output and shifting
// TaskCache und Task Flock fürs arbeiten und zwischenspeichern.
