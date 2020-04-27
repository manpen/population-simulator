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

#include <algorithm>
#include <cassert>
#include <random>
#include <vector>
#include <tlx/math.hpp>

namespace urns {

class LinearUrn {
public:
    using value_type = int64_t;
    using color_type = size_t;

    explicit LinearUrn(color_type number_of_colors) : balls_(number_of_colors) {
        assert(number_of_colors > 0);
    }

    void add_balls(color_type col, value_type n = 1) {
        number_of_balls_ += n;
        balls_[col] += n;
    }

    template <typename Generator>
    color_type remove_random_ball(Generator &&gen) noexcept {
        assert(!empty());
        auto value = std::uniform_int_distribution<value_type>{0, --number_of_balls_}(gen);

        size_t i = 0;
        while (true) {
            if (TLX_UNLIKELY(balls_[i] > value)) {
                --balls_[i];
                return i;
            }
            value -= balls_[i];
            ++i;
        }
    }

    template <typename Generator>
    color_type get_random_ball(Generator &&gen) const noexcept {
        assert(!empty());
        auto value = std::uniform_int_distribution<value_type>{0, number_of_balls_ - 1}(gen);

        size_t i = 0;
        while (true) {
            if (TLX_UNLIKELY(balls_[i] > value)) {
                return i;
            }
            value -= balls_[i];
            ++i;
        }
    }

    value_type number_of_balls() const noexcept { return number_of_balls_; }

    value_type number_of_balls_with_color(color_type col) const noexcept { return balls_[col]; }

    color_type number_of_colors() const noexcept { return balls_.size(); }

    bool empty() const noexcept { return !number_of_balls(); }

private:
    value_type number_of_balls_{0};
    std::vector<value_type> balls_;
};

} // namespace urns
