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
#include <iomanip>
#include <iostream>
#include <optional>

#include <tlx/cmdline_parser.hpp>

#include <pps/AsyncBatchSimulator.hpp>
#include <pps/AsyncRandomEngine.hpp>
#include <pps/RoundBasedMonitor.hpp>

#include "protocols/clock_protocol.hpp"
#include <urns/TreeUrn.hpp>

struct Configuration {
    yapps::Clock digits_on_clock{12};
    size_t num_agents{1'000'000};
    size_t num_rounds{2};
    size_t rounds_between_reports{1};
    unsigned seed{std::random_device{}()};
    unsigned num_output_lines{10};

    static std::optional<Configuration> parse_cmd(int argc, char *argv[]) {
        tlx::CmdlineParser parser;
        Configuration config;

        parser.add_unsigned('s', "seed", config.seed, "Seed value");

        parser.add_size_t('n', "agents", config.num_agents, "Number of agents");
        parser.add_unsigned('m', "clocksize", config.digits_on_clock, "Digits on clock");
        parser.add_size_t('R', "repetitions", config.num_rounds, "Number of rounds");

        parser.add_unsigned('l', "lines", config.num_output_lines, "Height of histogram");
        parser.add_size_t('g', "gap", config.rounds_between_reports,
                          "Number of rounds between reports");

        if (!parser.process(argc, argv)) {
            return {};
        }

        die_verbose_unless(config.num_agents > 1, "Need at least two agents");
        die_verbose_unless(config.digits_on_clock > 1, "Need at least two digits");

        return config;
    }
};

template <typename Simulator>
void print_histogram(const Configuration &config, const Simulator &simulator) {
    const auto &prot = simulator.protocol();
    const auto num_agents = simulator.agents().number_of_balls();

    if (simulator.agents().number_of_colors() > 30)
        return;

    std::stringstream ss;

    ss << "Interactions: " << std::setw(10) << simulator.num_interactions() << " ("
       << std::round(1.0 * simulator.num_interactions() / num_agents) << " rounds)\n";

    // Iterate over all clock digits, compute population and pass to callback
    auto for_each_digit = [&](auto cb) {
        for (yapps::Clock i = 0; i < prot.digits_on_clock(); ++i) {
            const auto num_unmarked =
                simulator.agents().number_of_balls_with_color(prot.encode({i, false}));
            const auto num_marked =
                simulator.agents().number_of_balls_with_color(prot.encode({i, true}));
            const auto num_total = num_marked + num_unmarked;
            cb(num_total, num_unmarked, num_marked);
        }
    };

    // Histogram
    for (int line = config.num_output_lines; --line;) {
        const size_t print_if_above =
            static_cast<size_t>((1.0 * config.num_agents / config.num_output_lines) * (line - 0.5));

        for_each_digit([&](auto num_total, auto num_unmarked, auto num_marked) {
            ss << "  |  "
               << (num_marked > print_if_above ? '+' : (num_total > print_if_above ? '*' : ' '));
        });

        ss << "  |\n";
    }

    // Population percentage
    ss << "  ";
    ss << std::setprecision(4);
    for_each_digit([&](auto num_total, auto num_unmarked, auto num_marked) {
        ss << '|' << std::setw(3) << (100 * num_total / config.num_agents) << "."
           << (1000 * num_total / config.num_agents) % 10;
    });
    ss << "|\n";

    // Digit labels
    ss << " ";
    for (unsigned digit = 0; digit < prot.digits_on_clock(); ++digit) {
        ss << " | " << std::setw(3) << digit;
    }
    ss << " |\n\n";

    ss << std::setw(10) << "#Total" << std::setw(10) << "# UNmarked" << std::setw(10)
       << "#Marked\n";
    for_each_digit([&](size_t num_total, size_t num_unmarked, size_t num_marked) {
        ss << std::setw(10) << num_total << std::setw(10) << num_unmarked << std::setw(10)
           << num_marked << '\n';
    });

    std::cout << ss.str() << std::endl;
}

int main(int argc, char *argv[]) {
    // Fetch command line config
    Configuration config;
    {
        auto opt_config = Configuration::parse_cmd(argc, argv);
        if (!opt_config)
            return 0;
        config = *opt_config;
        std::cout << "Seed: " << config.seed << '\n';
    }
    // pps::AsyncRandomEngine<std::mt19937_64> gen(16, config.seed);
    std::mt19937_64 gen(config.seed);

    pps::ScopedTimer timer;

    // Setup initial population
    ClockProtocol prot(config.digits_on_clock);
    if (config.digits_on_clock < 10)
        std::cout << pps::Protocols::transition_matrix(prot, prot.num_states()) << "\n";

    urns::TreeUrn urn(prot.num_states());
    const auto num_marked = static_cast<size_t>(std::round(std::sqrt(config.num_agents)));
    prot.create_uniform_distribution(urn, config.num_agents, num_marked);

    size_t gap_threshold = 0; // config.num_agents / 1e6 / config.digits_on_clock;

    auto report = [&](const auto &sim, auto &&) {
        print_histogram(config, sim);

        // const auto max_gap = prot.compute_max_gap(sim.agents(), gap_threshold);
        // std::cout << "Max gap: " << max_gap << "\n";
    };

    // Invoke simulator
    auto simulator = pps::AsyncBatchSimulator(urn, std::move(prot), gen);
    auto monitor = pps::RoundBasedMonitor<decltype(report)>(
        std::cout, report, config.rounds_between_reports, config.num_rounds, false);
    simulator.run(monitor);

    // Summary line
    std::cout << ".|" << config.num_rounds << "|" << config.num_agents << "|" << num_marked << "|"
              << config.digits_on_clock << "|x|" << timer.elapsed() / 1000. << "\n";

    return 0;
}
