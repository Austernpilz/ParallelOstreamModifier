#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include "BufferCache.h" 

void singleThreadTest()
{
  BufferCache cache(1, 1024, 2);

  // get a buffer
  auto buf1 = cache.getBuffer();
  assert(buf1->size() == 1024);

  cache.giveBackBuffer(std::move(buf1));

  auto buf2 = cache.getBuffer();
  assert(buf2 != nullptr);
  assert(buf2->size() == 1024);

  cache.giveBackBuffer(std::move(buf2));

  std::cout << "Single-thread test passed\n";
}

void multiThreadTest()
{
  constexpr int thread_count = 4;
  BufferCache cache(thread_count, 512, 2);

  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i)
  {
    threads.emplace_back([&cache, i]()
    {
      for (int j = 0; j < 100; ++j)
      {
        auto buf = cache.getBuffer();
        assert(buf->size() == 512);
        (*buf)[0] = static_cast<char>(i); // modify to simulate work
        cache.giveBackBuffer(std::move(buf));
      }
    });
  }

  for (auto& t : threads) t.join();

  std::cout << "Multi-thread test passed\n";
}

int main()
{
  singleThreadTest();
  multiThreadTest();

  std::cout << "All BufferCache tests passed\n";


  return 0;
}
