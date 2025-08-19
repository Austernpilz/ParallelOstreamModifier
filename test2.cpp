#include <zlib.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <cassert>
#include <vector>
derive from to 
std::streamubuf

class ChunkableOfstream : public std::ostream
{
  public:
    ChunkableOfstream()
    { 
    }
    ChunkableOfstream(const std::string& filename)
    {
    }

    ~ChunkableOstream()
    {
      flush();
      file_stream_.close();
    }

    void flush()
    {
      if (buffer_.str().empty()) return;
      file_stream_ << buffer_.str();
      buffer_.str(""); // Clear the buffer
    }

    template <class T>
    ChunkAbleStream& operator << (const T& data)
    {
      data_buffer_.append(std::move(data));
      if (data_buffer_.size() > max_size_)
      {
        
      }
    }
  private:
    ChunkableOstream::Data data_buffer_;

struct ChunkableOstream::Data
{
  
}

class ParallelCompressor
{
  ParallelCompressor(int threads, const int chunk_size)
  : threads_(threads), max_chunk_size_(chunk_size)
  {
  }
  void compress(T& data)
  {
    //errorhandling xyz
    int result = compress_(data)
// switch (result)
// {
// case 0:
// }
    }
  }

  int compress_(T& data)
  {
    //int def(FILE *source, FILE *dest, int level)
    z_stream compression_stream;
    unsigned char buffer_input[max_chunk_size_];
    unsigned char buffer_output[max_chunk_size_];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15+16,
                         8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
  }


  private:
    const int threads_ = 1
    const int chunk_size = 16384
}
    {
        out << c.real;
        out << "+i" << c.imag << endl;
        return out;
    }
  private:
    ParallelCompressor cp;
}

Int main()
{
  int t = 10;

  ChunableOstream out_file_stream(filename = "test.txt", threads = t, );
  writeMZML(out_file_stream);
}

void writeMZML(std::ostream& out)
{
	out << "<mzML>ganz geheime daten</mzML>";
	for (int i = 1; i < 1000; i ++) out << i;
}


// —------------------------

// class ChunkAbleStream : public std::ostream
// {

// ChunkAbleStream()
// 	: cp(10)
// {
	
// }
// 	template <class T>
// ChunkAbleStream& operator<<(const T& data)
// {
// 	Internal_data_ << data;
// 	If (internal_data.size() > 10000)
// {
// 	cp.compressChunk(internal_data);
// 	internal_data.clear();
// }
// }

// Private:
// 	std::stringstream internal_data_;
// 	ParallelCompressor cp;
	
// }

// class ParallelCompressor 
// {
// ParallelCompressor(int nr_of_threads);

// void compressChunk()
// {
// }
// }

// ChunkableStream cs;

// writeMZML(cs);



// int main(int argc, char **argv)
// {
//   const int MAX_CHUNK_SIZE = 1024 * 16 (24kb bis 1mb);
//   const int WINDOW_BITS = 15;

//   std::string buffer;
//   std::ifstream f(argv[1], std::ios::in | std::ios::binary);

//   if (!f.is_open()) {
//       std::cerr << "Error opening the file!";
//       return 1;
//   }
//   // Read the entire file into buffer
//   f.seekg(0, std::ios::end);
//   std::streamsize file_size = f.tellg();
//   f.seekg(0, std::ios::beg);
//   buffer.resize(file_size);
//   if (!f.read(&buffer[0], file_size)) {
//       std::cerr << "Error reading the file!";
//   }
//   f.close();


//   z_stream buff_stream;
//   buff_stream.zalloc = Z_NULL;
//   buff_stream.zfree = Z_NULL;
//   buff_stream.opaque = Z_NULL;

//   int ret = deflateInit2(&buff_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15+16,
//                          8, Z_DEFAULT_STRATEGY);

//   buff_stream.next_in = reinterpret_cast<unsigned char *>(const_cast<char *>(buffer.data()));
//   buff_stream.avail_in = strlen(buffer.data()) + 1;
//   unsigned char out_buffer[MAX_CHUNK_SIZE];
//   std::ofstream outfile(argv[2], std::ios::out | std::ios::binary);
//   do {
//     // Reset output buffer position and size.
//     buff_stream.avail_out = MAX_CHUNK_SIZE;
//     buff_stream.next_out = out_buffer;
//     // Do compress.
//     deflate(&buff_stream, true ? Z_FINISH : Z_NO_FLUSH);
//     // Allocate output memory.
//     std::size_t out_size = MAX_CHUNK_SIZE - buff_stream.avail_out;
//     if (out_size > 0) {
//       out_data_list << out_buffer;
//     }

//   } while (buff_stream.avail_out == 0);
//   // Done.
  
//   outfile.close();
//   return Z_OK;
// }
// // cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS=clang;lld;clang-tools-extra;lldb