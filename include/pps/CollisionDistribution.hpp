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

#include <cassert>
#include <cmath>
#include <random>

#include <tlx/define/likely.hpp>

#include <iostream>

namespace pps {

/**
 * Consider an urn with n balls of which g are red while n-g are green. Each time we remove
 * a ball, we put a red one into it. Let X be random variable describing how long we are
 * sampling until we see the first red ball. This corresponds to the
 * "strict collision distribution".
 */
class CollisionDisitribution {
    static constexpr size_t kNumStages = 16;
    static constexpr size_t kNumEstimates = 64;

public:
    using value_type = long long;

    explicit CollisionDisitribution(value_type n, value_type g = 0, value_type max_g = 0)
        : n_(n), log_n_(std::log(n)), stage_factor_(max_g / kNumStages) {
        set_red(g);

        for (size_t stage = 0; stage < kNumStages; stage++) {
            const auto red_lower = static_cast<value_type>(stage * stage_factor_);
            const auto red_upper = std::min<value_type>((1 + stage) * stage_factor_ + 1, max_g);

            for (size_t i = 0; i < kNumEstimates; ++i) {
                const auto rand_lower =
                    std::max(i / static_cast<double>(kNumEstimates), std::nextafter(0.0, 1.0));
                const auto rand_upper = (i + 1) / static_cast<double>(kNumEstimates);

                auto &limits = stages_[stage][i];

                limits.first = bisection(
                    TargetFunction{rand_upper, n_ - red_upper, std::lgamma(n_ - red_upper), log_n_},
                    0, n_ + 1);
                limits.second = bisection(TargetFunction{rand_lower, n_ - red_lower,
                                                         std::lgamma(n_ - red_lower), log_n_},
                                          0, n_ + 1)
                                + 1;

                assert(limits.first <= limits.second);
            }

            for (size_t i = 0; i < kNumEstimates; ++i) {
                const auto rand_lower =
                    std::max(i / static_cast<double>(kNumEstimates * kNumEstimates),
                             std::nextafter(0.0, 1.0));
                const auto rand_upper =
                    (i + 1) / static_cast<double>(kNumEstimates * kNumEstimates);

                auto &limits = small_stages_[stage][i];

                limits.first = bisection(
                    TargetFunction{rand_upper, n_ - red_upper, std::lgamma(n_ - red_upper), log_n_},
                    0, n_ + 1);
                limits.second = bisection(TargetFunction{rand_lower, n_ - red_lower,
                                                         std::lgamma(n_ - red_lower), log_n_},
                                          0, n_ + 1)
                                + 1;

                assert(limits.first <= limits.second);
            }
        }

        search_iters_ = 0;
    }

    void set_red(value_type g) {
        assert(g <= n_);
        current_stage_ = g / stage_factor_;
        assert(current_stage_ < kNumStages);

        n_green_ = n_ - g;
        loggamma_n_green_ = std::lgamma(n_green_);
    }

    template <typename Gen>
    value_type operator()(Gen &gen) {
        const auto rand = unif_(gen);
        return compute(rand);
    }

    /*
     * We equate the CDF with U where U is a unif random variable from [0; 1]
     *                U = 1.0 - exp(2.0 * (lgamma(n) - lgamma(n-k) - k*log(n)))
     * <=> log(1.0 - U) = 2.0 * (lgamma(n) - lgamma(n-k) - k*log(n))
     * <=> log(1.0 - U) / 2.0 - lgamma(n) = lgamma(n-k) - k * log(n)
     *
     * Now, we use a binary search to find the correct k
     */
    value_type compute(double uniform) {
        assert(0 < uniform && uniform < 1);
        bool force_bisection = false;

        const auto limits = [&] {
            if (uniform * kNumEstimates < 1.0) {
                force_bisection = true;
                return small_stages_[current_stage_][static_cast<size_t>(
                    uniform * (kNumEstimates * kNumEstimates))];
            } else {
                return stages_[current_stage_][static_cast<size_t>(uniform * (kNumEstimates))];
            }
        }();

        TargetFunction target_function(uniform, n_green_, loggamma_n_green_, log_n_);
        auto func = [&](auto x) {
            search_iters_++;
            return target_function(x);
        };

        value_type res;
        if (n_green_ < 1e6 || force_bisection) {
            res = bisection(func, limits.first, limits.second);
        } else {
            res = reg_falsi(func, limits.first, limits.second);
        }

        assert(res >= limits.first);
        assert(res <= limits.second);

        searches_++;

        return res;
    }

    size_t search_iters_ = 0;
    size_t searches_ = 0;

private:
    using estimator_stage_type = std::pair<value_type, value_type>[kNumEstimates];

    value_type n_;
    value_type n_green_;

    estimator_stage_type stages_[kNumStages];
    estimator_stage_type small_stages_[kNumStages];

    double loggamma_n_green_;
    double log_n_;
    double stage_factor_;

    size_t current_stage_;

    std::uniform_real_distribution<double> unif_{std::nextafter(0.0, 1.0),
                                                 std::nextafter(1.0, 2.0)};

    template <typename F>
    value_type bisection(F &&f, value_type left, value_type right) noexcept {
        assert(left <= right);

        while (left + 1 < right) {
            const auto mid = midpoint(left, right);
            const auto value = f(mid);

            // value(mid) is non-increasing in mid; thus flip compare
            if (value > 0) {
                right = mid;
            } else {
                left = mid;
            }

            search_iters_++;
        }

        return left;
    }

    template <typename F>
    value_type reg_falsi(F &&f, value_type x0int, value_type x1int) {
        if (TLX_UNLIKELY(x0int + 1 >= x1int))
            return x0int;

        // we need to compute two function values. rather than doing
        // it for x0 and x1, we carry out a bisection step and
        // obtain one value for free
        double f0, f1;
        double x0, x1;

        {
            const auto mid = midpoint(x0int, x1int);
            const auto val = f(mid);

            if (val < 0.0) {
                x0 = mid;
                f0 = val;
                x1 = x1int;
                f1 = f(x1);
            } else {
                x0 = x0int;
                f0 = f(x0);
                x1 = mid;
                f1 = val;
            }
        }

        if (TLX_UNLIKELY(f0 == 0.0))
            return x0int;

        for (int i = 0; i < 15; ++i) {
            if (x0 + 1.0 >= x1)
                return x0;

            assert(x0 < x1);
            assert(f0 < 0.0 && f1 >= 0.0);

            const auto new_x = (x0 * f1 - x1 * f0) / (f1 - f0);
            const auto new_f = f(new_x);

            if (TLX_UNLIKELY(!(x0 < new_x && new_x < x1)))
                break;

            if (new_f < 0.0) {
                x0 = new_x;
                f0 = new_f;
            } else {
                x1 = new_x;
                f1 = new_f;
            }
        }

        return bisection(f, static_cast<value_type>(x0),
                         std::min(x1int, static_cast<value_type>(x1) + 1));
    }

    class TargetFunction {
    public:
        TargetFunction(double rand, value_type n_green, double loggamma_n_green, double log_n)
            : target_{std::log(rand) - loggamma_n_green}, log_n_{log_n}, n_green_(n_green) {}

        double operator()(double k) const {
            return target_ + std::lgamma(n_green_ - k) + k * log_n_;
        }

    private:
        double target_;
        double log_n_;
        size_t n_green_;
    };

    value_type midpoint(value_type left, value_type right) const {
        return left + (right - left) / 2;
    }
};

} // namespace pps