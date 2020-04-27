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
#include <numeric>
#include <random>
#include <vector>

#include <sstream>
#include <string>

#include <sampling/hypergeometric_distribution.hpp>
#include <tlx/define.hpp>

namespace pps {

/**
 * A weighted urn contains n colored balls. There are m different colors.
 * While not a strict limitation, we optimize with the assumption that m << n:
 * Since two balls with the same color are indistinguishable from each other,
 * we store the number of balls in each color, and thereby shift the most complexities
 * from Theta(n) to O~tilde(m).
 */
class WeightedUrn {
public:
    using value_type = uint64_t;
    using color_type = size_t;
    using storage_type = std::vector<value_type>;
    using const_iterator = typename storage_type::const_iterator;

    // construction
    WeightedUrn() = delete;

    /**
     * Builds an urn with m = freqs.size() colors, where freqs[i] contains
     * the number of balls with color i. If number_of_balls is non-negative,
     * its number must match the sum of all counts provided.
     */
    explicit WeightedUrn(const storage_type &freqs)
        : balls_with_color_(freqs), number_of_balls_(count_balls()) {}

    WeightedUrn(const storage_type &freqs, value_type num_of_balls)
        : balls_with_color_(freqs), number_of_balls_(num_of_balls) {}

    explicit WeightedUrn(storage_type &&freqs)
        : balls_with_color_(std::move(freqs)), number_of_balls_(count_balls()) {}

    WeightedUrn(storage_type &&freqs, value_type num_of_balls)
        : balls_with_color_(std::move(freqs)), number_of_balls_(num_of_balls) {}

    /// Construct a uniformly filled urn
    explicit WeightedUrn(color_type num_of_colors, value_type balls_each = 0)
        : balls_with_color_(num_of_colors, balls_each),
          number_of_balls_(num_of_colors * balls_each) {}

    WeightedUrn(const WeightedUrn &) = default;
    WeightedUrn(WeightedUrn &&) = default;
    WeightedUrn &operator=(const WeightedUrn &) = default;
    WeightedUrn &operator=(WeightedUrn &&) = default;

    // accessors
    value_type operator[](color_type i) const noexcept { return number_of_balls_with_color(i); }

    value_type number_of_balls() const noexcept { return number_of_balls_; }

    value_type number_of_balls_with_color(color_type col) const noexcept {
        assert(col < number_of_colors());
        return balls_with_color_[col];
    }

    size_t number_of_colors() const noexcept { return balls_with_color_.size(); }

    // manipulators
    //! Adds n balls of color col
    void add_balls(color_type col, value_type n = 1) {
        assert(col < number_of_colors());
        balls_with_color_[col] += n;
        number_of_balls_ += n;
    }

    //! Removes n balls of color col
    void remove_balls(color_type col, value_type n = 1) {
        assert(col < number_of_colors());
        assert(n <= balls_with_color_[col]);
        balls_with_color_[col] -= n;
        number_of_balls_ -= n;
    }

    // sample single ball

    //! Picks a ball uniformly at random and returns its color
    //! @warning Urn must not be empty (i.e., contain at least one ball).
    template <typename Gen>
    value_type get_random_ball(Gen &gen) const {
        assert(number_of_balls() > 0);
        std::uniform_int_distribution<size_t> distr(0, number_of_balls() - 1);

        auto variate = distr(gen);
        auto it = balls_with_color_.cbegin();

        while (*it <= variate) {
            variate -= *it;
            ++it;
            assert(it != balls_with_color_.cend());
        }

        return std::distance(balls_with_color_.cbegin(), it);
    }

    //! Same as get_color_of_random(..) but also removes the ball from the urn
    //! @warning Urn must not be empty (i.e., contain at least one ball).
    template <typename Gen>
    value_type remove_random_ball(Gen &gen) {
        assert(number_of_balls() > 0);

        const auto color = get_random_ball(gen);
        --balls_with_color_[color];
        --number_of_balls_;
        return color;
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

        auto it_from = balls_with_color_.cbegin();

        while (left_to_sample) {
            assert(it_from != balls_with_color_.cend());
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
                cb(static_cast<color_type>(it_from - balls_with_color_.cbegin()), num_selected);

            left_to_sample -= num_selected;
            it_from++;
        }

        if (CallOnEmpty) {
            for (; it_from != balls_with_color_.cend(); ++it_from)
                cb(static_cast<color_type>(it_from - balls_with_color_.cbegin()), 0);
        }
    }

    template <typename Gen>
    WeightedUrn sample_without_replacement(const value_type num_of_samples, Gen &gen) const {
        auto urn = WeightedUrn(storage_type(number_of_colors(), 0), 0);

        sample_without_replacement<false>(
            num_of_samples, gen,
            [&](color_type color, value_type num) { urn.add_balls(color, num); });

        return urn;
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

    template <typename Gen>
    WeightedUrn remove_random_balls(const value_type num_of_samples, Gen &gen) {
        auto urn = WeightedUrn(storage_type(number_of_colors(), 0), 0);

        remove_random_balls(num_of_samples, gen,
                            [&](color_type color, value_type num) { urn.add_balls(color, num); });

        return urn;
    }

    // arithmetic
    bool operator==(const WeightedUrn &o) const { return balls_with_color_ == o.balls_with_color_; }

    bool operator!=(const WeightedUrn &o) const { return balls_with_color_ != o.balls_with_color_; }

    WeightedUrn &operator+=(const WeightedUrn &o) {
        apply_elementwise(balls_with_color_, o.balls_with_color_, std::plus<value_type>());
        number_of_balls_ += o.number_of_balls();
        return *this;
    }

    WeightedUrn &operator-=(const WeightedUrn &o) {
        apply_elementwise(balls_with_color_, o.balls_with_color_, std::minus<value_type>());
        number_of_balls_ -= o.number_of_balls();
        return *this;
    }

    WeightedUrn operator+(const WeightedUrn &o) const {
        auto copy = *this;
        copy += o;
        return copy;
    }

    WeightedUrn operator-(const WeightedUrn &o) const {
        auto copy = *this;
        copy -= o;
        return copy;
    }

    void add_urn(const WeightedUrn &o) { *this += o; }

    // helpers
    bool empty() const noexcept { return !number_of_balls(); }

    void clear() noexcept {
        number_of_balls_ = 0;
        std::fill(balls_with_color_.begin(), balls_with_color_.end(), 0);
    }

    std::vector<double> relative_frequencies() const {
        std::vector<double> res;
        res.reserve(number_of_colors());

        const auto scale = 1.0 / number_of_balls();

        for (auto x : balls_with_color_)
            res.push_back(x * scale);

        return res;
    }

    std::string to_string() {
        std::stringstream ss;
        ss << '[';
        color_type i = 0;
        bool sep = false;
        for (size_t i = 0; i < balls_with_color_.size(); ++i) {
            if (!balls_with_color_[i])
                continue;
            if (sep)
                ss << ", ";
            ss << i << ':' << balls_with_color_[i];
            sep = true;
        }
        ss << ']';
        return ss.str();
    }

private:
    storage_type balls_with_color_;
    value_type number_of_balls_;

    value_type count_balls() const noexcept {
        return std::accumulate(balls_with_color_.cbegin(), balls_with_color_.cend(), value_type{0});
    }

    template <typename Op2>
    void apply_elementwise(storage_type &a, const storage_type &b, Op2 op) const {
        assert(a.size() == b.size());
        auto ib = b.cbegin();
        for (auto ia = a.begin(); ia != a.end(); ++ia, ++ib) {
            *ia = op(*ia, *ib);
        }
    }
};

} // namespace pps
