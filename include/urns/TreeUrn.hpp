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
#include <vector>
#include <sampling/hypergeometric_distribution.hpp>
#include <tlx/math.hpp>

namespace urns {

class TreeUrn {
public:
    using value_type = int64_t;
    using color_type = size_t;

    explicit TreeUrn(color_type number_of_colors)
        : number_of_colors_(number_of_colors),
          first_leaf_(tlx::round_up_to_power_of_two(number_of_colors_)),
          tree_storage_(first_leaf_ + number_of_colors, 0),
          balls_with_color_(std::addressof(tree_storage_[first_leaf_ - 1])) {
        tree_1indexed_ = tree_storage_.data() - 1;
    }

    void add_balls(color_type col, value_type n = 1) {
        number_of_balls_ += n;

        auto i = first_leaf_ + col;
        do {
            auto step = [&] {
                const auto parent = i / 2;
                const auto isLeft = !(i & 1);
                tree_1indexed_[parent] += isLeft * n;
                i = parent;
            };

            if (i >= 16) {
                step();
                step();
                step();
            }

            step();
        } while (i > 1);

        balls_with_color_[col] += n;
    }

    void set_balls(color_type col, value_type n) {
        add_balls(col, n - number_of_balls_with_color(col));
    }

    void remove_balls(color_type col, value_type n) { add_balls(col, -n); }

    template <typename Generator>
    std::pair<color_type, value_type> remove_random_ball_with_index(Generator &&gen) noexcept {
        assert(!empty());
        auto value = std::uniform_int_distribution<value_type>{0, number_of_balls_ - 1}(gen);

        size_t i = 1;

        do {
            auto step = [&] {
                auto &leftWeight = tree_1indexed_[i];

                auto toRight = (value >= leftWeight);
                value -= toRight * leftWeight;
                leftWeight -= !toRight;

                i = 2 * i + toRight;
            };

            if (8 * i + 7 < first_leaf_) {
                step();
                step();
                step();
            }

            step();
        } while (i < first_leaf_);

        --number_of_balls_;
        const auto col = i - first_leaf_;
        balls_with_color_[col]--;

        return {col, value};
    }

    template <typename Generator>
    color_type remove_random_ball(Generator &&gen) noexcept {
        return remove_random_ball_with_index(std::forward<Generator>(gen)).first;
    }

    template <typename Generator>
    std::pair<color_type, value_type> get_random_ball_with_index(Generator &&gen) const noexcept {
        assert(!empty());
        auto value = std::uniform_int_distribution<value_type>{0, number_of_balls_ - 1}(gen);

        size_t i = 1;
        do {
            auto step = [&] {
                const auto leftWeight = tree_1indexed_[i];

                auto toRight = (value >= leftWeight);
                value -= toRight * leftWeight;

                i = 2 * i + toRight;
            };

            if (8 * i + 7 < first_leaf_) {
                step();
                step();
                step();
            }

            step();
        } while (i < first_leaf_);

        return {i - first_leaf_, value};
    }

    template <typename Generator>
    color_type get_random_ball(Generator &&gen) const noexcept {
        return get_random_ball_with_index(std::forward<Generator>(gen)).first;
    }

    value_type number_of_balls_with_color(color_type col) const noexcept {
        return balls_with_color_[col];
    }

    value_type number_of_balls() const noexcept { return number_of_balls_; }

    color_type number_of_colors() const noexcept { return number_of_colors_; }

    bool empty() const noexcept { return !number_of_balls(); }

    template <typename Urn>
    void add_urn(const Urn &other) {
        assert(other.number_of_colors() == number_of_colors());
        for (color_type c = 0; c < number_of_colors(); ++c)
            balls_with_color_[c] += other.number_of_balls_with_color(c);

        number_of_balls_ += other.number_of_balls();
        build_tree_from_balls();
    }

    void add_urn(const TreeUrn &other) {
        assert(other.number_of_colors() == number_of_colors());

        auto reader = other.tree_storage_.cbegin();
        for (auto &writer : tree_storage_)
            writer += *(reader++);

        number_of_balls_ += other.number_of_balls();
    }

    void clear() {
        number_of_balls_ = 0;
        std::fill(tree_storage_.begin(), tree_storage_.end(), 0);
    }

    // sample frequencies
    template <bool CallOnEmpty, typename Gen, typename Callback>
    void sample_without_replacement(const value_type num_of_samples, Gen &gen,
                                    Callback &&cb) const {
        if (TLX_UNLIKELY(!number_of_balls() || !num_of_samples))
            return;

        sampling::hypergeometric_distribution<Gen, value_type> hpd(gen);

        auto left_to_sample = num_of_samples;
        auto unconsidered_balls = static_cast<double>(number_of_balls());

        auto it_from = balls_with_color_;
        const auto *balls_end = balls_with_color_ + number_of_colors();

        while (left_to_sample) {
            assert(it_from != balls_end);
            const auto balls_with_color = *it_from;
            unconsidered_balls -= balls_with_color;
            const auto num_selected = [&]() -> size_t {
                if (!balls_with_color)
                    return 0;

                if (!unconsidered_balls)
                    return std::min(left_to_sample, balls_with_color);

                return hpd(balls_with_color, unconsidered_balls, left_to_sample);
            }();

            if (CallOnEmpty || num_selected)
                cb(static_cast<color_type>(it_from - balls_with_color_), num_selected);

            left_to_sample -= num_selected;
            it_from++;
        }

        if (CallOnEmpty) {
            for (; it_from != balls_end; ++it_from)
                cb(static_cast<color_type>(it_from - balls_with_color_), 0);
        }
    }

    /// Same as sample_without_replacement, but actually removes balls from urn
    template <bool CallOnEmpty, typename Gen, typename Callback>
    void remove_random_balls(const value_type num_of_samples, Gen &gen, Callback &&cb) {
        sample_without_replacement<CallOnEmpty>(num_of_samples, gen,
                                                [&](color_type color, value_type num) {
                                                    remove_balls(color, num);
                                                    cb(color, num);
                                                });
    }

private:
    value_type number_of_balls_{0};
    size_t number_of_colors_{0};
    size_t first_leaf_;

    std::vector<value_type> tree_storage_;

    value_type *tree_1indexed_;
    value_type *balls_with_color_;

    void build_tree_from_balls() {
        std::fill(tree_storage_.data(), balls_with_color_, 0);

        for (size_t i = first_leaf_ + number_of_colors() - 1; i; --i) {
            const auto parent = i >> tlx::ffs(~i);
            if (!parent)
                continue;

            tree_1indexed_[parent] += tree_1indexed_[i];
        }
    }
};

} // namespace urns