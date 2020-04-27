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
#include <random>

namespace pps {

// Identical to std::bernoulli_distribution distr(0.5), but much faster
class FairCoin {
public:
    template <typename Gen>
    bool operator()(Gen &gen) {
        if (!valid_--) {
            buf_ = unif_(gen);
            valid_ = 64;
        }

        const auto res = buf_ & 1;
        buf_ >>= 1;
        return res;
    }

private:
    std::uniform_int_distribution<uint64_t> unif_;
    uint64_t buf_;
    uint64_t valid_{0};
};

} // namespace pps
