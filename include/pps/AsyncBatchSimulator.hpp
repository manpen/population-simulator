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

#include "WeightedUrn.hpp"
#include <urns/TreeUrn.hpp>

namespace pps {

template <typename Protocol, typename RandGen, typename UrnType = pps::WeightedUrn>
class AsyncBatchSimulator {
public:
    using urn_type = UrnType;

    AsyncBatchSimulator() = delete;

    AsyncBatchSimulator(const urn_type &urn, Protocol p, RandGen &gen)
        : agents_(urn.number_of_colors()), updated_agents_(agents_.number_of_colors()),
          target_epoch_length_(urn.number_of_balls()),

          protocol_(std::move(p)), prng_(gen),
          collision_distr_(urn.number_of_balls(), 0, 2 * target_epoch_length_.max()) {
        die_verbose_unless(urn.number_of_balls() > 0, "Provided empty urn to simulator");
        agents_.add_urn(urn);

        if constexpr (Protocols::is_deterministic<Protocol>) {
            if constexpr (Protocols::is_one_way<Protocol>) {
                one_way_partitions_ =
                    Protocols::parition_oneway_transactions(protocol_, agents_.number_of_colors());

            } else {
                size_t skips;
                std::tie(skipable_transactions_, skips) =
                    Protocols::transactions_without_change(protocol_, agents_.number_of_colors());
                use_skip_heuristic_ = (skips > agents_.number_of_colors());

                // std::cout << "Skip Transactions: " << use_skip_heuristic_ << '\n';
            }
        } else {
            skipable_transactions_.resize(agents_.number_of_colors());
        }
    }

    template <typename Monitor>
    void run(Monitor &&monitor) {
        target_epoch_length_.start();
        do {
            // start new epoch
            assert(updated_agents_.number_of_balls() == 0);

            sample_run_lengths_and_plant_collisions();
            process_delayed_agents();

            agents_.add_urn(updated_agents_);
            updated_agents_.clear();
            num_delayed_agents_ = 0;
            num_epochs_++;
            target_epoch_length_.update(num_interactions_);
        } while (monitor(*this));
    }

    const urn_type &agents() const noexcept { return agents_; }

    const Protocol &protocol() const noexcept { return protocol_; }

    Protocol &protocol() noexcept { return protocol_; }

    size_t num_interactions() const noexcept { return num_interactions_; }

    size_t num_runs() const noexcept { return num_runs_; }

    size_t num_epochs() const noexcept { return num_epochs_; }

    size_t target_epoch_length() const noexcept { return target_epoch_length_.current_best(); }

    RandGen &prng() { return prng_; }

private:
    using count_t = typename urn_type::value_type;

    urn_type agents_;
    size_t num_delayed_agents_{0};
    urn_type updated_agents_;

    EpochLengthController target_epoch_length_;

    Protocol protocol_;
    RandGen &prng_;
    FairCoin fair_coin_;

    CollisionDisitribution collision_distr_;

    std::vector<std::pair<state_t, count_t>>
        first_agents_; //! buffer for process_delayed_agents to avoid reallocation

    std::vector<std::vector<state_t>> skipable_transactions_;
    bool use_skip_heuristic_{false};

    Protocols::OneWayPartitions one_way_partitions_;

    // state
    size_t num_interactions_{0};
    size_t num_runs_{0};
    size_t num_epochs_{0};

    void sample_run_lengths_and_plant_collisions() {
        const auto num_agents = agents_.number_of_balls() + updated_agents_.number_of_balls();

        while (num_delayed_agents_ + updated_agents_.number_of_balls()
               < target_epoch_length_.current()) {
            // sample length of next round
            auto num_colliding_agents = num_delayed_agents_ + updated_agents_.number_of_balls();
            collision_distr_.set_red(num_colliding_agents);
            size_t round_length;
            do {
                round_length = collision_distr_(prng_);
            } while (!num_colliding_agents && round_length < 2);
            num_delayed_agents_ += 2 * (round_length / 2);

            // helper to sample agents
            num_colliding_agents = num_delayed_agents_ + updated_agents_.number_of_balls();
            auto sample_agent = [&](bool has_collision) {
                if (has_collision) {
                    if (with_probability_(num_delayed_agents_, num_colliding_agents))
                        return sample_delayed_agent();

                    return sample_updated_agent();
                }

                return sample_untouched_agent();
            };

            // plant collision
            const auto has_collision_on_first = (round_length % 2 == 0);
            const auto has_collision_on_second =
                !has_collision_on_first || with_probability_(num_colliding_agents, num_agents);
            auto first = sample_agent(has_collision_on_first);
            auto second = sample_agent(has_collision_on_second);

            std::tie(first, second) = perform_interaction(first, second);

            updated_agents_.add_balls(first, 1);
            updated_agents_.add_balls(second, 1);

            num_runs_++;
            assert(num_delayed_agents_ % 2 == 0);
        }
    }

    void process_delayed_agents() {
        if (Protocols::is_deterministic<Protocol> && Protocols::is_one_way<Protocol>)
            return process_delayed_agents_partitioned();

        assert(first_agents_.empty());
        const auto num_agents = agents_.number_of_balls() + updated_agents_.number_of_balls();

        agents_.template remove_random_balls<false>(
            num_delayed_agents_ / 2, prng_,
            [&](auto col, auto num) { first_agents_.emplace_back(col, num); });

        sampling::hypergeometric_distribution<RandGen, size_t> hpd(prng_);

        for (const auto task : first_agents_) {
            const auto first_state = task.first;
            const auto &skips = skipable_transactions_[first_state];
            auto left_to_sample = task.second;
            auto unconsidered_balls = static_cast<double>(agents_.number_of_balls());

            const auto number_of_skipable_balls =
                !use_skip_heuristic_
                    ? 0
                    : std::accumulate(skips.begin(), skips.end(), 0llu, [&](size_t sum, state_t x) {
                          return sum + agents_.number_of_balls_with_color(x);
                      });

            // first exclude skipped balls
            if (number_of_skipable_balls) {
                unconsidered_balls -= number_of_skipable_balls;
                const auto skipped_trans =
                    hpd(number_of_skipable_balls, unconsidered_balls, left_to_sample);
                left_to_sample -= skipped_trans;
                updated_agents_.add_balls(first_state, skipped_trans);
            }

            auto skips_iter = skips.cbegin();
            for (state_t second = 0; left_to_sample; ++second) {
                assert(second < agents_.number_of_colors());

                if (use_skip_heuristic_) {
                    while (skips_iter != skips.cend() && *skips_iter < second) {
                        skips_iter++;
                    }

                    if (skips_iter != skips.cend() && *skips_iter == second)
                        continue;
                }

                const auto balls_with_color = agents_.number_of_balls_with_color(second);
                unconsidered_balls -= balls_with_color;
                const auto num_selected = [&]() -> size_t {
                    if (!balls_with_color)
                        return 0;

                    if (!unconsidered_balls)
                        return std::min(left_to_sample, balls_with_color);

                    return hpd(balls_with_color, unconsidered_balls, left_to_sample);
                }();

                if (num_selected) {
                    agents_.remove_balls(second, num_selected);
                    perform_interactions(first_state, second, num_selected, updated_agents_);
                }

                left_to_sample -= num_selected;
            }
        }

        first_agents_.clear();
    }

    void process_delayed_agents_partitioned() {
        assert(first_agents_.empty());
        const auto num_agents = agents_.number_of_balls() + updated_agents_.number_of_balls();

        agents_.template remove_random_balls<false>(
            num_delayed_agents_ / 2, prng_,
            [&](auto col, auto num) { first_agents_.emplace_back(col, num); });

        sampling::hypergeometric_distribution<RandGen, size_t> hpd(prng_);

        for (const auto task : first_agents_) {
            const auto first_state = task.first;
            const auto &skips = skipable_transactions_[first_state];
            auto left_to_sample = task.second;
            auto unconsidered_balls = static_cast<double>(agents_.number_of_balls());

            if (TLX_UNLIKELY(!left_to_sample))
                continue;

            if (TLX_UNLIKELY(one_way_partitions_[first_state].size() == 1)) {
                // only one partition -> whole row goes into new state
                updated_agents_.add_balls(one_way_partitions_[first_state].front().second,
                                          left_to_sample);
                continue;
            }

            for (const auto partition : one_way_partitions_[first_state]) {
                count_t balls_in_second_state = 0;
                for (state_t x : partition.first)
                    balls_in_second_state += agents_.number_of_balls_with_color(x);

                unconsidered_balls -= balls_in_second_state;
                const auto num_selected = [&]() -> count_t {
                    if (!balls_in_second_state)
                        return 0;

                    if (!unconsidered_balls)
                        return std::min(left_to_sample, balls_in_second_state);

                    return hpd(balls_in_second_state, unconsidered_balls, left_to_sample);
                }();

                updated_agents_.add_balls(partition.second, num_selected);
                left_to_sample -= num_selected;

                if (!left_to_sample)
                    break;
            }
        }

        num_interactions_ += num_delayed_agents_ / 2;

        first_agents_.clear();
    }

    state_t sample_untouched_agent() { return agents_.remove_random_ball(prng_); }

    state_t sample_delayed_agent() {
        assert(num_delayed_agents_ >= 2);

        // obtain agents
        auto first = sample_untouched_agent();
        auto second = sample_untouched_agent();
        num_delayed_agents_ -= 2;

        // execute transition
        std::tie(first, second) = perform_interaction(first, second);

        // store one randomly selected partner, return the other one
        if (fair_coin_(prng_))
            std::swap(first, second);
        updated_agents_.add_balls(second, 1);

        return first;
    }

    state_t sample_updated_agent() { return updated_agents_.remove_random_ball(prng_); }

    bool with_probability_(count_t good, count_t total) {
        return std::uniform_int_distribution<count_t>{1, total}(prng_) <= good;
    }

    // interaction with protocol
    state_pair_t perform_interaction(state_t first, state_t second) {
        const auto res = Protocols::transition(protocol_, {first, second});
        num_interactions_++;
        return res;
    }

    // carry out a multiple interactions and return update states in urn
    void perform_interactions(state_t first, state_t second, count_t num, urn_type &target) {
        if constexpr (Protocols::is_deterministic<Protocol>) {
            const auto new_states = Protocols::transition(protocol_, {first, second});
            target.add_balls(new_states.first, num);
            target.add_balls(new_states.second, num);
            num_interactions_ += num;

        } else {
            const auto num_agents = target.number_of_balls();

            auto assign_callback = [&](state_t state, const count_t num) {
                target.add_balls(state, num);
            };

            protocol_(first, second, num, assign_callback);

            num_interactions_ += num;

            die_verbose_unless(
                target.number_of_balls() == num_agents + 2 * num,
                "The number of updated states assigned does not match the number of interactions");
        }
    }
};

} // namespace pps