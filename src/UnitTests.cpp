#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include "Streambuf_Modifier_t.h" 

// BufferCache
void BC_singleThreadTest()
{
  BufferCache cache(1, 8, 2);

  assert(cache.getLocalCacheSize() == 0)
  assert(cache.getGlobalCacheSize() == 2)

  auto buf1 = cache.getBuffer();
  (*buf1)[0] = static_cast<char>(42);

  auto ptr1 = buf1.get(); // keep raw pointer
  cache.giveBackBuffer(std::move(buf1));

  auto buf2 = cache.getBuffer();
  auto ptr2 = buf2.get();

  // Expect same buffer pointer back from cache
  assert(ptr1 == ptr2);
  assert((*buf2)[0] == static_cast<char>(42));

  cache.giveBackBuffer(std::move(buf2));

  buf1 = cache.getBuffer();
  auto ptr1 = buf1.get();

  buf2 = cache.getBuffer();
  auto ptr2 = buf2.get();

  cache.giveBackBuffer(std::move(buf1));
  cache.giveBackBuffer(std::move(buf2));

   assert(ptr1 != ptr2);
   assert(cache.getLocalCacheSize() == 2)
   assert(getGlobalCacheSize() == 0)

  std::cout << "Single-thread cache reuse test passed\n";
}

void BC_multiThreadTest()
{
  constexpr int thread_count = 4;
  BufferCache cache2(thread_count, 64, 2);

  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i)
  {
    threads.emplace_back( [this] { BC_singleThreadTest(); } )
    threads.emplace_back( [&cache2, i] ()
    {
      std::unique_ptr<std::vector<char>> first_buf;

      for (int j = 1; j < 50; ++j)
      {
        auto buf = cache2.getBuffer();
        if (j == 0)
        {
          (*buf)[0] = static_cast<char>(i); // mark with thread id
          first_buf = std::move(buf);
          cache2.giveBackBuffer(std::move(first_buf));
        }
        else
        {
          if ((*buf)[0] == static_cast<char>(i))
          {
            // Got our own buffer back
            std::cout << "Thread " << i << " reused buffer\n";
            cache2.giveBackBuffer(std::move(buf));
            return; // success for this thread
          }
          cache2.giveBackBuffer(std::move(buf));
        }
      }
    });
  }

  for (auto& t : threads) 
  {
    if (t.joinable) { t.join(); }
  }
  std::cout << "Multi-thread cache reuse test passed\n";
}


int main()
{
  singleThreadTest();
  multiThreadTest();

  std::cout << "All BufferCache tests passed\n";


  return 0;
}
