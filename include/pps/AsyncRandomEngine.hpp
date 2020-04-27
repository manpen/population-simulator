/**
 * @author Manuel Penschuck
 * @copyright
 * Copyright (C) 2019 Manuel Penschuck
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * @copyright
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <tlx/define/likely.hpp>

namespace pps {

template <typename OriginalEngine, size_t ElementsInBlock = 1llu << 16>
class AsyncRandomEngine {
public:
    static constexpr size_t kElementsInBlock = ElementsInBlock;
    using original_engine_type = OriginalEngine;
    using result_type = typename original_engine_type::result_type;

    template <typename... Args>
    explicit AsyncRandomEngine(size_t num_blocks = 16, Args... engine_args)
        : engine_(std::forward<Args...>(engine_args...)) {
        for (size_t i = 0; i < num_blocks; ++i) {
            block_t block;
            block.reserve(ElementsInBlock);
            empty_blocks_.emplace();
        }

        generator_ = std::thread(&AsyncRandomEngine::generator_main, this);
    }

    ~AsyncRandomEngine() {
        running_ = false;
        cv_.notify_one();
        generator_.join();
    }

    static constexpr result_type min() { return original_engine_type::min(); }

    static constexpr result_type max() { return original_engine_type::max(); }

    result_type operator()() {
        if (TLX_UNLIKELY(block_consuming_.empty())) {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return !full_blocks_.empty(); });
            empty_blocks_.emplace(std::move(block_consuming_));
            block_consuming_ = std::move(full_blocks_.front());
            full_blocks_.pop();

            cv_.notify_one();
        }

        assert(!block_consuming_.empty());

        const auto res = block_consuming_.back();
        block_consuming_.pop_back();
        return res;
    }

private:
    using block_t = std::vector<result_type>;

    original_engine_type engine_;

    std::atomic<bool> running_{true};

    std::thread generator_;

    std::queue<block_t> empty_blocks_;
    std::queue<block_t> full_blocks_;

    block_t block_consuming_{};

    std::mutex mutex_;
    std::condition_variable cv_;

    void generator_main() {
        block_t current_block;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return !empty_blocks_.empty(); });
            current_block = std::move(empty_blocks_.front());
            empty_blocks_.pop();
        }

        while (running_) {
            // generate
            assert(current_block.empty());
            for (size_t i = 0; i < kElementsInBlock; ++i)
                current_block.emplace_back(engine_());

            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [&] { return !empty_blocks_.empty() || !running_; });
                if (!running_)
                    break;

                full_blocks_.emplace(std::move(current_block));
                current_block = std::move(empty_blocks_.front());
                empty_blocks_.pop();

                cv_.notify_one();
            }
        }
    }
};

} // namespace pps