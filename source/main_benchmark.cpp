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
#include <chrono>
#include <iostream>
#include <optional>

#include <string>

#include <tlx/cmdline_parser.hpp>

#include <urns/AliasUrnSimple.hpp>
#include <urns/LinearUrn.hpp>

#include <pps/AsyncBatchSimulator.hpp>
#include <pps/AsyncDistributionSimulator.hpp>
#include <pps/AsyncPopulationSimulator.hpp>

#include <protocols/clock_protocol.hpp>
#include <protocols/random_protocol.hpp>

struct Configuration {
    enum class Protocol { RandomOneWay, RandomTwoWay, Clock, RunningClock };

    enum class Simulator {
        Batch,
        BatchTree,
        Population,
        Population4,
        Population8,
        DistrLinear,
        DistrTree,
        DistrAlias
    };

    size_t num_agents{1'024};
    size_t num_max_agents{std::numeric_limits<size_t>::max()};
    double time_budget_secs{10.0};

    pps::state_t num_states{20};
    size_t num_rounds{10};
    unsigned num_repeats{1};

    std::string simulator_name{"batch"};
    std::string protocol_name{"random1"};
    Simulator simulator;
    Protocol protocol;

    bool print_header_only{false};

    unsigned seed{std::random_device{}()};

    std::string to_string() const {
        std::stringstream ss;
        auto sim_name = simulator_name;
        if (sim_name == "distr-alias")
            sim_name = "distr-alias-fixed";

        ss << sim_name << ',' << protocol_name << ',' << num_agents << ',' << num_states << ','
           << num_rounds << ',' << seed;
        return ss.str();
    }

    static std::optional<Configuration> parse_cmd(int argc, char *argv[]) {
        tlx::CmdlineParser parser;
        Configuration config;

        parser.add_unsigned('s', "seed", config.seed, "Seed value");
        parser.add_string('a', "simulator", config.simulator_name,
                          "Simulator: batch, pop, pop4, pop8, distr-linear, distr-alias");
        parser.add_string('p', "protocol", config.protocol_name, "Protocol: random, clock");

        parser.add_size_t('n', "agents", config.num_agents, "Number of agents");
        parser.add_size_t('N', "maxagents", config.num_max_agents, "Max. number of agents");
        parser.add_double('t', "time", config.time_budget_secs, "Max time budget / run [seconds]");

        parser.add_unsigned('d', "states", config.num_states, "Number of states");

        parser.add_size_t('r', "rounds", config.num_rounds, "Number of rounds");
        parser.add_uint('R', "repeats", config.num_repeats, "Number of repeats");

        parser.add_flag("header-only", config.print_header_only, "Print CSV header and quit");

        if (!parser.process(argc, argv)) {
            return {};
        }

        if (config.simulator_name == "batch")
            config.simulator = Simulator::Batch;
        else if (config.simulator_name == "batch-tree")
            config.simulator = Simulator::BatchTree;
        else if (config.simulator_name == "pop")
            config.simulator = Simulator::Population;
        else if (config.simulator_name == "pop4")
            config.simulator = Simulator::Population4;
        else if (config.simulator_name == "pop8")
            config.simulator = Simulator::Population8;
        else if (config.simulator_name == "distr-linear")
            config.simulator = Simulator::DistrLinear;
        else if (config.simulator_name == "distr-tree")
            config.simulator = Simulator::DistrTree;
        else if (config.simulator_name == "distr-alias")
            config.simulator = Simulator::DistrAlias;
        else {
            std::cout << "Unknown simulator >" << config.simulator_name << "<\n";
            return {};
        }

        if (config.protocol_name == "random1")
            config.protocol = Protocol::RandomOneWay;
        else if (config.protocol_name == "random2")
            config.protocol = Protocol::RandomTwoWay;
        else if (config.protocol_name == "clock")
            config.protocol = Protocol::Clock;
        else if (config.protocol_name == "running-clock")
            config.protocol = Protocol::RunningClock;
        else {
            std::cout << "Unknown protocol: >" << config.protocol_name << "<\n";
            return {};
        }

        die_verbose_unless(config.num_agents > 1, "Need at least two agents");
        die_verbose_unless(config.num_states > 1, "Need at least two states");

        return config;
    }
};

double measure_single_run(const Configuration &config, std::mt19937_64 &prng) {
    auto run = [&](auto simulator) -> double {
        const auto threshold = config.num_agents * config.num_rounds;
        auto monitor = [&](const auto &sim) { return sim.num_interactions() < threshold; };

        const auto start = std::chrono::steady_clock::now();
        simulator.run(monitor);
        const auto stop = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::duration<double>>(stop - start).count();

        std::cout << config.to_string() << ',' << simulator.num_interactions() << ',' << elapsed
                  << std::endl;
        return elapsed;
    };

    auto select_simulator = [&](auto urn, auto protocol) -> double {
        auto convert_urn = [&](auto &target) {
            for (pps::state_t s = 0; s < urn.number_of_colors(); ++s)
                target.add_balls(s, urn.number_of_balls_with_color(s));
        };

        using Protocol = decltype(protocol);
        switch (config.simulator) {
        case Configuration::Simulator::Batch:
            return run(pps::AsyncBatchSimulator(urn, protocol, prng));
        case Configuration::Simulator::BatchTree: {
            urns::TreeUrn new_urn(urn.number_of_colors());
            convert_urn(new_urn);
            return run(pps::AsyncBatchSimulator(new_urn, protocol, prng));
        }
        case Configuration::Simulator::Population:
            return run(
                pps::AsyncPopulationSimulator<0, Protocol, std::mt19937_64>(urn, protocol, prng));
        case Configuration::Simulator::Population4:
            return run(
                pps::AsyncPopulationSimulator<4, Protocol, std::mt19937_64>(urn, protocol, prng));
        case Configuration::Simulator::Population8:
            return run(
                pps::AsyncPopulationSimulator<8, Protocol, std::mt19937_64>(urn, protocol, prng));
        case Configuration::Simulator::DistrLinear: {
            urns::LinearUrn new_urn(urn.number_of_colors());
            convert_urn(new_urn);
            return run(pps::AsyncDistributionSimulator(new_urn, protocol, prng));
        }
        case Configuration::Simulator::DistrTree: {
            urns::TreeUrn new_urn(urn.number_of_colors());
            convert_urn(new_urn);
            return run(pps::AsyncDistributionSimulator(new_urn, protocol, prng));
        }
        case Configuration::Simulator::DistrAlias: {
            urns::AliasUrnSimple new_urn(urn.number_of_colors());
            convert_urn(new_urn);
            return run(pps::AsyncDistributionSimulator(new_urn, protocol, prng));
        }
        default:
            abort();
        }
    };

    switch (config.protocol) {
    case Configuration::Protocol::RunningClock:
        [[fallthrough]];
    case Configuration::Protocol::Clock: {
        if (config.num_states % 2)
            tlx::die_with_message("num_states must be even for the clock protocol");
        auto num_agents = config.num_agents;
        auto num_marked = static_cast<size_t>(std::sqrt(num_agents) + 1);
        num_agents -= num_marked;

        pps::WeightedUrn urn(config.num_states);

        if (config.protocol == Configuration::Protocol::RunningClock) {
            urn.add_balls(0, num_agents);
            urn.add_balls(config.num_states / 2, num_marked);

        } else {
            for (pps::state_t s = 0; s < config.num_states / 2; ++s) {
                // unmarked
                {
                    const auto n = num_agents / (config.num_states - s);
                    urn.add_balls(s, n);
                    num_agents -= n;
                }

                {
                    const auto n = num_marked / (config.num_states - s);
                    urn.add_balls(s + config.num_states / 2, n);
                    num_marked -= n;
                }
            }
        }

        return select_simulator(urn, ClockProtocol(config.num_states / 2));
    }

    case Configuration::Protocol::RandomOneWay:
        [[fallthrough]];
    case Configuration::Protocol::RandomTwoWay: {
        pps::WeightedUrn urn(config.num_states);

        auto num_agents = config.num_agents;
        for (pps::state_t s = 0; s < config.num_states; ++s) {
            const auto n = num_agents / (config.num_states - s);
            urn.add_balls(s, n);
            num_agents -= n;
        }

        if (config.protocol == Configuration::Protocol::RandomOneWay) {
            return select_simulator(urn, RandomProtocolOneWay{prng, config.num_states});
        } else {
            return select_simulator(urn, RandomProtocolTwoWay{prng, config.num_states});
        }
    }

    default:
        abort();
    }
}

int main(int argc, char *argv[]) {
    const auto config = Configuration::parse_cmd(argc, argv);
    if (!config)
        return 1;
    if (config->print_header_only) {
        std::cout << "simulator,protocol,num_agents,num_states,num_rounds,seed,num_interactions,"
                     "walltime\n";
        return 0;
    }

    std::mt19937_64 prng(config->seed);

    const double expected_slowdown = 1;
    //(config->simulator == Configuration::Simulator::Batch || config->simulator ==
    //Configuration::Simulator::BatchTree) ? 1.4 : 1.9;

    for (unsigned repeat = 0; repeat < config->num_repeats; ++repeat) {
        for (size_t num_agents = config->num_agents; num_agents <= config->num_max_agents;
             num_agents *= 2) {
            auto my_config = *config;
            my_config.num_agents = num_agents;

            const auto elapsed = measure_single_run(my_config, prng);
            if (expected_slowdown * elapsed >= config->time_budget_secs)
                break;
        }
    }

    return 0;
}
