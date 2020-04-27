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
#include <iomanip>
#include <iostream>
#include <random>

#include <tlx/cmdline_parser.hpp>

#include <pps/AsyncBatchSimulator.hpp>
#include <pps/RoundBasedMonitor.hpp>
#include <pps/WeightedUrn.hpp>

#include <protocols/clock_protocol.hpp>

int main(int argc, char *argv[]) {
    const auto seed = std::random_device{}();
    std::mt19937_64 gen(seed);
    std::cout << "Seed: " << seed << "\n";

    const auto max_rounds = 10000;

    std::cerr << "log2(n),n,m,N,time\n";

    for (unsigned num_agents_exp = 10; num_agents_exp < 40; ++num_agents_exp) {
        for (auto digits_on_clock : {7, 11}) {
            size_t num_agents = 1llu << num_agents_exp;

            // inform about new search
            {
                std::stringstream ss;
                ss << "Start simulation with n=" << std::setw(16) << num_agents << "=2^"
                   << std::setw(2) << num_agents_exp << " and m=" << std::setw(2)
                   << digits_on_clock;
                std::cout << ss.str() << std::endl;
            }
            pps::ScopedTimer timer;

            // Setup initial population
            ClockProtocol prot(digits_on_clock);
            pps::WeightedUrn urn(prot.num_states());
            const auto num_marked = static_cast<size_t>(std::round(std::sqrt(num_agents)));
            prot.create_uniform_distribution(urn, num_agents, num_marked);

            size_t next_report = 0;

            auto after_epoch_callback = [&](const auto &sim) {
                const auto max_gap = prot.compute_max_gap(sim.agents(), 0);

                if (next_report <= sim.num_interactions()) {
                    std::stringstream ss;
                    ss << " Interactions: " << std::setw(16) << sim.num_interactions()
                       << " Rounds: " << std::setw(5) << (sim.num_interactions() / num_agents)
                       << " Gap: " << max_gap;
                    std::cout << ss.str() << std::endl;

                    next_report = sim.num_interactions() + 10 * num_agents;
                }

                if (max_gap >= digits_on_clock / 2) {
                    std::cerr << num_agents_exp << ',' << num_agents << ',' << digits_on_clock
                              << ',' << sim.num_interactions() << ',' << timer.elapsed()
                              << std::endl;

                    return false;
                }

                return true;
            };

            // Invoke simulator
            auto simulator = pps::AsyncBatchSimulator(urn, std::move(prot), gen);
            // auto monitor = pps::RoundBasedMonitor<decltype(report)>(
            //    std::cout, report, 1, max_rounds, false);
            simulator.run(after_epoch_callback);
        }
    }

    return 0;
}
