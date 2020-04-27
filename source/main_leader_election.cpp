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
#include <pps/RoundBasedMonitor.hpp>
#include <pps/WeightedUrn.hpp>

#include <protocols/leader_election_protocol.hpp>

int main(int argc, char *argv[]) {
    // Commandline parser
    size_t num_agents = 1'000'000;
    size_t num_rounds = 1000;
    unsigned seed = 10;

    tlx::CmdlineParser parser;
    parser.add_size_t('n', "agents", num_agents, "Number of agents");
    parser.add_size_t('R', "repetitions", num_rounds, "Number of rounds");
    parser.add_unsigned('s', "seed", seed, "Seed value of URN");
    if (!parser.process(argc, argv))
        return -1;

    // Setup simulation
    LeaderElectionProtocol prot;
    std::cout << pps::Protocols::transition_matrix(prot, prot.num_states()) << "\n";

    pps::WeightedUrn urn(prot.num_states());
    urn.add_balls(LeaderElectionProtocol::Leader, num_agents);

    auto report = [&](const auto &sim, auto &monitor) {
        const auto num_leaders =
            sim.agents().number_of_balls_with_color(LeaderElectionProtocol::Leader);

        std::stringstream ss;
        ss << "Leaders: " << std::setw(15) << num_leaders << " ("
           << (100. * num_leaders / num_agents) << "%%)\n";
        std::cout << ss.str();

        if (num_leaders == 1)
            monitor.stop_simulation();
    };

    // Invoke simulator
    std::mt19937_64 gen(seed);
    auto simulator = pps::AsyncBatchSimulator(urn, std::move(prot), gen);
    auto monitor = pps::RoundBasedMonitor<decltype(report)>(std::cout, report, 10, num_rounds);
    simulator.run(monitor);

    return 0;
}
