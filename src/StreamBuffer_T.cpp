
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