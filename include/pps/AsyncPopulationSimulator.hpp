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
#include <random>
#include <tuple>
#include <utility>

#include <tlx/container/ring_buffer.hpp>
#include <tlx/die.hpp>
#include <tlx/meta.hpp>

#include <pps/Protocols.hpp>
#include <pps/WeightedUrn.hpp>

namespace pps {

template <size_t PrefetchInteractions, typename Protocol, typename RandGen>
class AsyncPopulationSimulator {
public:
    constexpr static auto kPrefetchInteractions = PrefetchInteractions;
    using urn_type = pps::WeightedUrn;

    AsyncPopulationSimulator() = delete;

    AsyncPopulationSimulator(urn_type urn, Protocol p, RandGen &gen)
        : population_(urn.number_of_balls(), 0), num_states_(urn.number_of_colors()),
          protocol_(std::move(p)), prng_(gen), agent_distr_(0, urn.number_of_balls() - 1),
          epoch_length_(std::max(kPrefetchInteractions,
                                 static_cast<size_t>(std::pow(urn.number_of_balls(), 0.5)) + 1)),
          prefetch_buffer_(2 * kPrefetchInteractions) {
        die_verbose_unless(urn.number_of_balls() > 1, "Need at least two agents");

        // copy distribution to population
        auto it = population_.begin();
        for (pps::state_t s = 0; s < urn.number_of_colors(); ++s) {
            const auto n = urn.number_of_balls_with_color(s);
            std::fill_n(it, n, s);
            it += n;
        }
    }

    template <typename Monitor>
    void run(Monitor &&monitor) {
        do {
            if constexpr (kPrefetchInteractions == 0) {
                // we still use the concept of epochs in order to keep the load on monitor
                // roughly comparable to the batch simulator
                for (size_t intraepoch = 0; intraepoch < epoch_length_; ++intraepoch) {
                    perform_single_interaction_with_prefetch();
                }
            } else {
                for (size_t prefetch = 0; prefetch < kPrefetchInteractions; ++prefetch)
                    prefetch_pair();

                for (size_t intraepoch = 0; intraepoch < epoch_length_ - kPrefetchInteractions;
                     ++intraepoch) {
                    perform_prefetched_pair();
                    prefetch_pair();
                }

                for (size_t postperform = 0; postperform < kPrefetchInteractions; ++postperform)
                    perform_prefetched_pair();
            }

            num_interactions_ += epoch_length_;
            ++num_epochs_;
        } while (monitor(*this));
    }

    const Protocol &protocol() const noexcept { return protocol_; }

    Protocol &protocol() noexcept { return protocol_; }

    size_t num_interactions() const noexcept { return num_interactions_; }

    size_t num_runs() const noexcept { return num_runs_; }

    size_t num_epochs() const noexcept { return num_epochs_; }

    size_t target_epoch_length() const noexcept { return epoch_length_; }

    RandGen &prng() { return prng_; }

    const std::vector<pps::state_t> &population() const noexcept { return population_; }

    // for compat only. EXPENSIVE
    urn_type agents() const {
        urn_type agents(num_states_);
        for (pps::state_t x : population_) {
            agents.add_balls(x);
        }

        return agents;
    }

private:
    std::vector<pps::state_t> population_;
    pps::state_t num_states_;

    Protocol protocol_;
    RandGen &prng_;
    std::uniform_int_distribution<size_t> agent_distr_;
    size_t epoch_length_;

    tlx::RingBuffer<pps::state_t *> prefetch_buffer_;

    // state
    size_t num_interactions_{0};
    size_t num_runs_{0};
    size_t num_epochs_{0};

    // variant without prefetching
    void perform_single_interaction_with_prefetch() {
        const auto first_id = agent_distr_(prng_);
        pps::state_t second_id;
        do {
            second_id = agent_distr_(prng_);
        } while (TLX_UNLIKELY(second_id == first_id));

        const auto new_states =
            Protocols::transition(protocol_, {population_[first_id], population_[second_id]});
        assert(new_states.first < num_states_);
        assert(new_states.second < num_states_);

        population_[first_id] = new_states.first;
        if constexpr (!Protocols::is_one_way<Protocol>) {
            population_[second_id] = new_states.second;
        }
    }

    // prefetched variant
    void prefetch_pair() {
        // first id is easy
        pps::state_t *first = std::addressof(population_[agent_distr_(prng_)]);
        __builtin_prefetch(first, 1); // 1 indicates that we intent to write to this position
        prefetch_buffer_.push_back(first);

        // second id needs to be different from first
        pps::state_t *second;
        do {
            second = std::addressof(population_[agent_distr_(prng_)]);
        } while (TLX_UNLIKELY(first == second));
        __builtin_prefetch(second, !Protocols::is_one_way<Protocol>);
        prefetch_buffer_.push_back(second);
    }

    void perform_prefetched_pair() {
        auto *first = prefetch_buffer_.front();
        prefetch_buffer_.pop_front();
        auto *second = prefetch_buffer_.front();
        prefetch_buffer_.pop_front();

        const auto new_states = Protocols::transition(protocol_, {*first, *second});
        assert(new_states.first < num_states_);
        assert(new_states.second < num_states_);

        *first = new_states.first;
        if constexpr (!Protocols::is_one_way<Protocol>) {
            *second = new_states.second;
        }
    }
};

} // namespace pps
