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

#include <array>
#include <random>
#include <tuple>
#include <type_traits>
#include <utility>

#include <tlx/die.hpp>

#include <pps/CollisionDistribution.hpp>
#include <pps/EpochLengthController.hpp>
#include <pps/FairCoin.hpp>
#include <pps/Protocols.hpp>
#include <pps/ScopedTimer.h>

namespace pps {

template <typename Urn, typename Protocol, typename RandGen>
class AsyncDistributionSimulator {
public:
    using urn_type = Urn;

    AsyncDistributionSimulator() = delete;

    AsyncDistributionSimulator(urn_type urn, Protocol p, RandGen &gen)
        : agents_(std::move(urn)), protocol_(std::move(p)), prng_(gen),
          epoch_length_(static_cast<size_t>(std::pow(agents_.number_of_balls(), 0.5)) + 1) {
        die_verbose_unless(urn.number_of_balls() > 1, "Need at least two agents");
    }

    template <typename Monitor>
    void run(Monitor &&monitor) {
        do {
            // we still use the concept of epochs in order to keep the load on monitor
            // roughly comparable to the batch simulator
            for (size_t intraepoch = 0; intraepoch < epoch_length_; ++intraepoch) {
                perform_single_interaction();
            }

            num_interactions_ += epoch_length_;
            ++num_epochs_;
        } while (monitor(*this));
    }

    const urn_type &agents() const noexcept { return agents_; }

    const Protocol &protocol() const noexcept { return protocol_; }

    Protocol &protocol() noexcept { return protocol_; }

    size_t num_interactions() const noexcept { return num_interactions_; }

    size_t num_runs() const noexcept { return num_runs_; }

    size_t num_epochs() const noexcept { return num_epochs_; }

    size_t target_epoch_length() const noexcept { return epoch_length_; }

    RandGen &prng() { return prng_; }

private:
    urn_type agents_;

    Protocol protocol_;
    RandGen &prng_;
    size_t epoch_length_;

    // state
    size_t num_interactions_{0};
    size_t num_runs_{0};
    size_t num_epochs_{0};

    void perform_single_interaction() {
        state_pair_t old_states;

        // first agent is remove (as it may change)
        old_states.first = agents_.remove_random_ball(prng_);

        // in one-way communication the second agent won't change,
        // so we just draw a ball, but do not remove it
        if constexpr (Protocols::is_one_way<Protocol>) {
            old_states.second = agents_.get_random_ball(prng_);
        } else {
            old_states.second = agents_.remove_random_ball(prng_);
        }

        const auto new_states = Protocols::transition(protocol_, old_states);
        agents_.add_balls(new_states.first);

        if constexpr (!Protocols::is_one_way<Protocol>) {
            agents_.add_balls(new_states.second);
        }
    }
};

} // namespace pps