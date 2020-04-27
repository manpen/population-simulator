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
#include <array>
#include <cassert>
#include <random>
#include <tuple>
#include <vector>
#include <tlx/math.hpp>

#include <urns/Traits.hpp>

namespace urns {

class AliasUrnSimple {
public:
    using value_type = int64_t;
    using color_type = size_t;

    explicit AliasUrnSimple(color_type number_of_colors, double lower_thresh = 0.8,
                            double upper_thresh = 1.5)
        : param_lower_threshold_(lower_thresh), param_upper_threshold_(upper_thresh),
          alias_table_(number_of_colors), balls_with_color_(number_of_colors) {
        assert(lower_thresh < 1);
        assert(upper_thresh > 1);
        assert(number_of_colors > 0);
    }

    void bulk_add_balls(color_type col, value_type n = 1) {
        number_of_balls_ += n;
        balls_with_color_[col] += n;
    }

    void bulk_commit() { build_alias_table(); }

    void add_balls(color_type col, value_type n = 1) {
        assert(col < number_of_colors());
        assert(balls_with_color_[col] >= -n);

        // update data structure
        alias_table_[col].weights[0] += n;
        balls_with_color_[col] += n;
        number_of_balls_ += n;

        const auto new_weight = alias_table_[col].total_weight();
        if (TLX_UNLIKELY(row_current_max_ < new_weight))
            row_current_max_ = new_weight;

        assert_consistency();

        if (TLX_UNLIKELY(new_weight < row_weight_lower_ || row_weight_upper_ < new_weight))
            if (TLX_UNLIKELY(!try_fix_row(col)))
                return build_alias_table();
    }

    template <typename Generator>
    color_type remove_random_ball(Generator &&gen) noexcept {
        auto [row_id, color, column_id] = get_random_ball_(gen);
        auto &row = alias_table_[row_id];

        balls_with_color_[color]--;
        number_of_balls_--;

        row.weights[static_cast<size_t>(column_id)]--;
        assert_consistency();

        if (TLX_UNLIKELY(row.total_weight() < row_weight_lower_))
            if (TLX_UNLIKELY(!try_fix_row(gen, row_id)))
                build_alias_table();

        return color;
    }

    template <typename Generator>
    color_type get_random_ball(Generator &&gen) const noexcept {
        return std::get<1>(get_random_ball_(gen));
    }

    value_type number_of_balls() const noexcept { return number_of_balls_; }

    value_type number_of_balls_with_color(color_type col) const noexcept {
        return balls_with_color_[col];
    }

    color_type number_of_colors() const noexcept { return alias_table_.size(); }

    bool empty() const noexcept { return !number_of_balls(); }

    template <typename Urn>
    void add_urn(const Urn &other) {
        assert(other.number_of_colors() == number_of_colors());
        for (color_type c = 0; c < number_of_colors(); ++c)
            balls_with_color_[c] += other.number_of_balls_with_color(c);

        number_of_balls_ += other.number_of_balls();
        build_alias_table();
    }

private:
    value_type number_of_balls_{0};
    double param_lower_threshold_;
    double param_upper_threshold_;

    struct Row {
        std::array<value_type, 2> weights;
        color_type color2;

        explicit constexpr Row(value_type w1 = 0, value_type w2 = 0, color_type c2 = 0)
            : weights({w1, w2}), color2(c2) {}

        constexpr value_type total_weight() const { return weights[0] + weights[1]; }
    };

    std::vector<Row> alias_table_;
    std::vector<value_type> balls_with_color_;

    std::vector<color_type> small_elements_;
    std::vector<color_type> large_elements_;

    value_type row_weight_lower_{0};
    value_type row_weight_upper_{0};
    value_type row_current_max_{0};

    template <typename Gen>
    auto get_random_ball_(Gen &&gen) const {
        assert(!empty());
        std::uniform_int_distribution<size_t> distr{0, number_of_colors() * row_current_max_ - 1};

        while (true) {
            const auto random = distr(gen);
            const auto row_id = random / row_current_max_;
            auto random_weight = random % row_current_max_;

            const Row &row = alias_table_[row_id];
            if (random_weight < row.weights[0]) {
                return std::make_tuple(row_id, row_id, false);
            }

            random_weight -= row.weights[0];
            if (TLX_LIKELY(random_weight < row.weights[1])) {
                return std::make_tuple(row_id, row.color2, true);
            }
        }
    }

    void categorize_into_small_and_large() {
        const auto average_floored = number_of_balls() / number_of_colors();

        assert(small_elements_.empty());
        assert(large_elements_.empty());

        small_elements_.reserve(number_of_colors());
        large_elements_.reserve(number_of_colors());

        std::array<std::vector<color_type> *, 2> elements_{&small_elements_, &large_elements_};

        for (color_type i = 0; i < number_of_colors(); i++) {
            const auto num = number_of_balls_with_color(i);
            elements_[num > average_floored]->emplace_back(i);
            alias_table_[i].weights[0] = num;
            alias_table_[i].weights[1] = 0;
        }

#ifndef NDEBUG
        assert(!small_elements_.empty());
        const auto sum_small =
            std::accumulate(small_elements_.cbegin(), small_elements_.cend(), 0llu,
                            [&](auto s, auto i) { return s + alias_table_[i].weights[0]; });
        const auto sum_large =
            std::accumulate(large_elements_.cbegin(), large_elements_.cend(), 0llu,
                            [&](auto s, auto i) { return s + alias_table_[i].weights[0]; });
        assert(sum_small + sum_large == number_of_balls());
#endif
    }

    void split_large_elements() noexcept {
        const auto average_floored = number_of_balls() / number_of_colors();
        auto num_above_avg =
            static_cast<int64_t>(number_of_balls() - average_floored * number_of_colors());

        row_weight_lower_ = static_cast<value_type>(average_floored * param_lower_threshold_);
        row_current_max_ = average_floored + (num_above_avg > 0);
        row_weight_upper_ =
            static_cast<value_type>(std::ceil(row_current_max_ * param_upper_threshold_));

        while (!large_elements_.empty()) {
            assert(!small_elements_.empty());
            auto &row = alias_table_[small_elements_.back()];
            small_elements_.pop_back();

            const auto remaining = average_floored + (num_above_avg-- > 0) - row.weights[0];
            if (TLX_UNLIKELY(remaining == 0))
                continue;

            const auto large_id = large_elements_.back();
            auto &large_weight = alias_table_[large_id].weights[0];

            assert(large_weight >= remaining);
            large_weight -= remaining;

            row.weights[1] = remaining;
            row.color2 = large_id;

            if (large_weight <= average_floored) {
                small_elements_.push_back(large_id);
                large_elements_.pop_back();
            }
        }
        small_elements_.clear();

#ifndef NDEBUG
        const auto sum_table =
            std::accumulate(alias_table_.cbegin(), alias_table_.cend(), 0llu,
                            [](size_t sum, const Row &x) { return sum + x.total_weight(); });
        // ensure the total number of balls distributed matches the total number of balls stored
        assert(sum_table == number_of_balls());

        // ensure that each Row has average weight or one above
        assert(std::all_of(alias_table_.cbegin(), alias_table_.cend(), [=](const Row &x) {
            return average_floored <= x.total_weight() && x.total_weight() <= average_floored + 1;
        }));

        // ensure that each Row has average weight or one above
        assert(std::all_of(alias_table_.cbegin(), alias_table_.cend(), [=](const Row &x) {
            return x.weights[1] == 0 || (x.color2 < number_of_colors());
        }));
#endif
    }

    bool try_fix_row(size_t row_id) {
        auto &row = alias_table_[row_id];
        std::minstd_rand gen(1234567 * row_id ^ 345678 * row.weights[0]
                             ^ 567890 * row.weights[1] + 234234);
        return try_fix_row(gen, row_id);
    }

    template <typename Gen>
    bool try_fix_row(Gen &gen, size_t row_id) {
        auto &row = alias_table_[row_id];
        std::uniform_int_distribution<color_type> color_distr(0, number_of_colors() - 1);

        for (unsigned i = 0; i < 5; ++i) {
            auto partner_id = color_distr(gen);
            if (TLX_UNLIKELY(partner_id == row_id))
                continue;

            auto &partner_row = alias_table_[partner_id];

            const auto w1 = row.weights[0] + partner_row.weights[1];
            const auto w2 = row.weights[1] + partner_row.weights[0];

            if (row_weight_lower_ < w1 && row_weight_lower_ < w2 && w1 < row_weight_upper_
                && w2 < row_weight_upper_) {
                std::swap(row.weights[1], partner_row.weights[1]);
                std::swap(row.color2, partner_row.color2);
                return true;
            }
        }

        return false;
    }

    void build_alias_table() {
        const size_t num_colors = number_of_colors();
        if (!num_colors)
            return;

        assert_consistency(true);

        categorize_into_small_and_large();
        split_large_elements();

        assert_consistency();
    }

    void assert_consistency(bool ignore_alias_table = false) const noexcept {
#ifndef NDEBUG
        const auto sum_balls_with_color =
            std::accumulate(balls_with_color_.cbegin(), balls_with_color_.cend(), 0llu);
        assert(sum_balls_with_color == number_of_balls());

        if (!ignore_alias_table) {
            const auto sum_alias_table =
                std::accumulate(alias_table_.cbegin(), alias_table_.cend(), 0llu,
                                [](auto s, auto e) { return s + e.total_weight(); });
            assert(sum_alias_table == number_of_balls());
        }

        assert(std::all_of(alias_table_.cbegin(), alias_table_.cend(),
                           [&](auto e) { return e.total_weight() <= row_current_max_; }));
#endif
    }
};

namespace traits {

template <>
struct has_bulk_insertions<AliasUrnSimple> {
    static constexpr bool value = true;
};

} // namespace traits
} // namespace urns
