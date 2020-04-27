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

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <tlx/define.hpp>

namespace pps {

template <typename Callback>
class RoundBasedMonitor {
    using Clock = std::chrono::steady_clock;

public:
    RoundBasedMonitor() = delete;

    RoundBasedMonitor(std::ostream &os, Callback &cb, size_t rounds_between_reports,
                      size_t stop_in_round, bool terminal_store_cursor = false)
        : output_stream_(os), callback_(cb), terminal_store_cursor_(terminal_store_cursor),
          termination_round_(stop_in_round),
          next_report_in_round_(rounds_between_reports ? rounds_between_reports
                                                       : std::numeric_limits<size_t>::max()),
          gap_between_reports_(rounds_between_reports) {}

    template <typename Simulator>
    bool operator()(const Simulator &sim) {
        const auto inters = sim.num_interactions();
        const auto round = inters / sim.agents().number_of_balls();

        if (terminal_store_cursor_)
            std::cout << "\033[0;0H\n";

        if (TLX_UNLIKELY(termination_round_ && round >= termination_round_)) {
            report_time(sim);
            callback_(sim, *this);

            keep_running_ = false;
        } else if (TLX_UNLIKELY(next_report_in_round_ <= round)) {
            report_time(sim);
            callback_(sim, *this);

            if (!keep_running_) {
                std::cout << "Stopped prematurely as requested by reporter callback\n";
            }

            next_report_in_round_ += gap_between_reports_;
        }

        return keep_running_;
    }

    /// gracefully stop simulation with one epoch
    void stop_simulation() { keep_running_ = false; }

private:
    std::ostream &output_stream_;
    Callback &callback_;

    bool terminal_store_cursor_;
    bool keep_running_{true};

    size_t termination_round_{
        0}; //!< Stop simulation after this many rounds. Infinite simulation if 0
    size_t next_report_in_round_{0};
    size_t gap_between_reports_{1};

    size_t last_runs_{0};
    size_t last_epochs_{0};

    // performance counters
    Clock::time_point time_start_{Clock::now()};
    Clock::time_point time_last_report_{Clock::now()};
    size_t interactions_last_report_{0};

    template <typename Simulator>
    void report_time(const Simulator &sim) {
        using namespace std::chrono;

        // compute time and throughput
        const auto now = steady_clock::now();
        const auto elapsed_total =
            duration_cast<duration<double, std::milli>>(now - time_start_).count();
        const auto elapsed_last =
            duration_cast<duration<double, std::milli>>(now - time_last_report_).count();
        const auto through_total = sim.num_interactions() / elapsed_total / 1000.0;
        const auto through_last =
            (sim.num_interactions() - interactions_last_report_) / elapsed_last / 1000.0;

        const auto elapsed_epochs = sim.num_epochs() - last_epochs_;
        const auto elapsed_runs = sim.num_runs() - last_runs_;
        last_epochs_ = sim.num_epochs();
        last_runs_ = sim.num_runs();

        // produce output
        {
            std::stringstream ss;
            ss << "Round: " << std::setw(8)
               << (sim.num_interactions() / sim.agents().number_of_balls()) << ". Elapsed time\n";
            ss << " since start " << std::setw(10) << elapsed_total << "ms (" << std::setw(10)
               << std::round(through_total * 10) / 10.0 << " interact/us)\n";
            ss << " since last  " << std::setw(10) << elapsed_last << "ms (" << std::setw(10)
               << std::round(through_last * 10) / 10.0 << " interact/us)\n";

            ss << " epoch target length n^" << std::setprecision(2) << std::setw(4)
               << std::log(sim.target_epoch_length()) / std::log(sim.agents().number_of_balls())
               << " runs per epoch " << std::setw(4)
               << static_cast<size_t>(std::round(elapsed_runs / elapsed_epochs)) << "\n";
            output_stream_ << ss.str();
        }

        // update state
        time_last_report_ = now;
        interactions_last_report_ = sim.num_interactions();
    }
};

} // namespace pps
