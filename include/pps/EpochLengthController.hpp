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
#include <chrono>
#include <cmath>

class EpochLengthController {
public:
    explicit EpochLengthController(size_t n)
        : min_(static_cast<size_t>(std::pow(n, 0.4)) + 1),
          max_(static_cast<size_t>(std::pow(n, 0.8)) + 1),
          current_best_(static_cast<size_t>(std::pow(n, 0.6)) + 1) {
        if (max_ > n)
            max_ = n;
        if (current_best_ > max_)
            current_best_ = max_;
    }

    EpochLengthController(size_t min, size_t max)
        : min_(min), max_(max), current_best_((max - min) / 2 + min) {
        assert(min < max);
    }

    void start() {
        state_ = States::MeasureBelow;
        phase_start_time_ = measure_start_time_ = std::chrono::steady_clock::now();
        current_measurement_ = update_value(state_);
    }

    void update(size_t num_interactions) {
        if (measure_epochs_++ >= measure_number_of_epochs_) {
            measure_epochs_ = 0;

            // update measurements
            {
                const auto now = std::chrono::steady_clock::now();
                const auto time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
                                              now - measure_start_time_)
                                              .count();
                measure_start_time_ = now;

                const auto progress = num_interactions - measure_num_interactions_start_;

                measured_times_[state_] = progress / time_elapsed;
            }
            measure_num_interactions_start_ = num_interactions;

            // transition to next state
            state_ = static_cast<States>(static_cast<int>(state_) + 1);
            if (state_ > 2) {
                // all three measurements are in; find the best
                auto best_idx =
                    std::distance(measured_times_.begin(),
                                  std::max_element(measured_times_.begin(), measured_times_.end()));

                current_best_ = update_value(static_cast<States>(best_idx));
                state_ = MeasureBelow;

                // calibrate measure time; we try to adjust every 50ms
                {
                    constexpr double target_ms_per_phase = 60;
                    constexpr double bias = 0.8;

                    const auto phase_time =
                        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
                            measure_start_time_ - phase_start_time_)
                            .count();
                    measure_number_of_epochs_ = static_cast<size_t>(
                        measure_number_of_epochs_
                        * (bias + (1 - bias) * target_ms_per_phase / phase_time));
                    if (measure_number_of_epochs_ < 10)
                        measure_number_of_epochs_ = 10;
                }

                phase_start_time_ = measure_start_time_;
            }

            current_measurement_ = update_value(state_);
        }
    }

    size_t min() const { return min_; }

    size_t max() const { return max_; }

    size_t current() const { return current_measurement_; }

    size_t current_best() const { return current_best_; }

private:
    size_t measure_number_of_epochs_{10};

    size_t min_;
    size_t max_;
    size_t current_best_;
    size_t current_measurement_;

    enum States { MeasureBelow = 0, MeasureCurrent = 1, MeasureAbove = 2 };
    States state_;

    std::array<double, 3> measured_times_;
    size_t measure_epochs_{0};
    std::chrono::steady_clock::time_point measure_start_time_;
    std::chrono::steady_clock::time_point phase_start_time_;
    size_t measure_num_interactions_start_{0};

    size_t update_value(States state) {
        auto value =
            static_cast<size_t>(current_best_ * (1.0 + (static_cast<int>(state) - 1) * 0.1));
        if (value < min_)
            return min_;
        if (value > max_)
            return max_;
        return value;
    }
};
