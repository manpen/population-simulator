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
#include <iostream>
#include <tlx/cmdline_parser.hpp>

#include <pps/AsyncBatchSimulator.hpp>
#include <pps/Protocols.hpp>
#include <pps/RoundBasedMonitor.hpp>
#include <pps/WeightedUrn.hpp>

#include <protocols/majority_protocol.hpp>

int main(int argc, char *argv[]) {
    // Commandline parser
    size_t num_agents = 1'000'000;
    size_t num_rounds = 100;
    size_t num_rounds_between_snapshots = 10;
    unsigned seed = 10;

    tlx::CmdlineParser parser;
    parser.add_size_t('n', "agents", num_agents, "Number of agents");
    parser.add_size_t('R', "repetitions", num_rounds, "Number of rounds");
    parser.add_size_t('g', "gap", num_rounds_between_snapshots, "Number of rounds between reports");
    parser.add_unsigned('s', "seed", seed, "Seed value of URN");
    if (!parser.process(argc, argv))
        return -1;

    // Setup simulation
    MajorityProtocol prot;
    pps::WeightedUrn urn(prot.num_states());
    urn.add_balls(prot.encode({false, true}), num_agents / 4 - 1);
    urn.add_balls(prot.encode({true, true}), num_agents - num_agents / 4 + 1);

    auto report = [&](const auto &sim, auto & /*monitor*/) {
        std::stringstream ss;

        unsigned bar_width = 80;
        double char_width = 1.0 * bar_width / sim.agents().number_of_balls();
        for (pps::state_t i = 0; i < prot.num_states(); ++i) {
            const auto num_agents = sim.agents().number_of_balls_with_color(i);
            const auto width = static_cast<unsigned>(num_agents * char_width);
            ss << "Op: " << prot.decode(i).opinion << " Strong: " << prot.decode(i).strong << " |"
               << std::string(width, '*') << std::string(bar_width - width, ' ') << "|"
               << std::setw(10) << num_agents << '\n';
        }

        std::cout << ss.str();
    };

    // Invoke simulator
    std::mt19937_64 gen(seed);
    auto simulator = pps::AsyncBatchSimulator(urn, std::move(prot), gen);
    auto monitor = pps::RoundBasedMonitor<decltype(report)>(
        std::cout, report, num_rounds_between_snapshots, num_rounds);
    report(simulator, monitor);
    simulator.run(monitor);

    return 0;
}
