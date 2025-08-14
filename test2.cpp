#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"
#include <fstream>
#include <cassert>

int main(int argc, char **argv)
{
  string compress_in;
  ifstream f(argv[1], ios::in | ios::binary);

  if (!f.is_open()) {
      cerr << "Error opening the file!";
      return 1;
  }
  string s;

  while (getline(f, s))
      compress_in += s;

  // Close the file
  f.close();
  compress_in += '\0'; // Null-terminate the string
  string compress_out = string(1024, '\0'); // Preallocate output buffer

  z_stream buff_stream;
  buff_stream.zalloc = Z_NULL;
  buff_stream.zfree = Z_NULL;
  buff_stream.opaque = Z_NULL;

  buff_stream.avail_in = compress_in.size() + 1;
  buff_stream.next_in = (Bytef*)compress_in.data();
  buff_stream.avail_out = compress_out.size();
  buff_stream.next_out = (Bytef*)compress_out.data();
  
  int res = deflateInit2
  (
    &strm, 
    Z_DEFAULT_COMPRESSION, 
    Z_DEFLATED, 
    15+16, 
    8, 
    Z_DEFAULT_STRATEGY
  );
  assert(res == Z_OK);
  res = deflate(&strm, Z_FINISH);
  assert(res == Z_STREAM_END);
  
  res = deflateEnd(&strm);
  assert(res == Z_OK);  

  std::ofstream outfile(argv[2], std::ios::out | std::ios::binary);
  outfile.write(compress_out, sizeof(compress_out) - strm.avail_out); 
  outfile.close();
  return Z_OK;
}
cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS=clang;lld;clang-tools-extra;lldb