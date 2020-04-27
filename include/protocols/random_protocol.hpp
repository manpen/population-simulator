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
#include <random>
#include <vector>

#include <pps/Protocols.hpp>

class RandomProtocolOneWay : public pps::Protocols::DeterministicProtocol,
                             pps::Protocols::OneWayProtocol {
public:
    RandomProtocolOneWay(std::mt19937_64 &gen, pps::state_t num_states)
        : num_states_(num_states), transitions_(num_states * num_states) {
        std::uniform_int_distribution<pps::state_t> distr(0, num_states - 1);
        for (auto &t : transitions_)
            t = distr(gen);
    }

    constexpr pps::state_t num_states() const noexcept { return num_states_; }

    pps::state_t operator()(pps::state_t fst, pps::state_t snd) const {
        assert(fst < num_states_);
        assert(snd < num_states_);
        auto res = transitions_[fst * num_states_ + snd];
        assert(res < num_states_);
        return res;
    }

private:
    pps::state_t num_states_;
    std::vector<pps::state_t> transitions_;
};

class RandomProtocolTwoWay : public pps::Protocols::DeterministicProtocol {
public:
    RandomProtocolTwoWay(std::mt19937_64 &gen, pps::state_t num_states)
        : num_states_(num_states), transitions_(num_states * num_states) {
        std::uniform_int_distribution<pps::state_t> distr(0, num_states - 1);
        for (auto &t : transitions_)
            t = {distr(gen), distr(gen)};
    }

    constexpr pps::state_t num_states() const noexcept { return num_states_; }

    pps::state_pair_t operator()(pps::state_t fst, pps::state_t snd) const {
        assert(fst < num_states_);
        assert(snd < num_states_);
        auto res = transitions_[fst * num_states_ + snd];
        assert(res.first < num_states_);
        assert(res.second < num_states_);
        return res;
    }

private:
    pps::state_t num_states_;
    std::vector<pps::state_pair_t> transitions_;
};
