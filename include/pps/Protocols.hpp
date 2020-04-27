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
#include <cassert>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace pps {
using state_t = unsigned;
using state_pair_t = std::pair<state_t, state_t>;

namespace Protocols {
class DeterministicProtocol {};
class OneWayProtocol {};

template <typename Protocol>
constexpr bool is_deterministic = std::is_base_of_v<DeterministicProtocol, Protocol>;

template <typename Protocol>
constexpr bool is_one_way = std::is_base_of_v<OneWayProtocol, Protocol>;

template <typename Protocol>
state_pair_t transition(Protocol &protocol, state_pair_t input) {
    if constexpr (is_deterministic<Protocol>) {
        if constexpr (is_one_way<Protocol>) {
            return {protocol(input.first, input.second), input.second};

        } else {
            return protocol(input.first, input.second);
        }

    } else {
        std::array<state_t, 2> new_states;
        size_t num_updates = 0;

        auto assign_callback = [&](state_t new_state, const size_t num) {
            if (num == 1) {
                new_states[num_updates] = new_state;
            } else {
                new_states[0] = new_state;
                new_states[1] = new_state;
            }
            num_updates += num;

            assert(num_updates <= 2);
        };

        protocol(input.first, input.second, 1u, assign_callback);

        if constexpr (is_one_way<Protocol>) {
            assert(num_updates == 1);
            return {new_states[0], input.second};
        } else {
            assert(num_updates == 2);
            return {new_states[0], new_states[1]};
        }
    }
}

template <typename Protocol>
std::string transition_matrix(Protocol &protocol, unsigned num_states, bool vt100 = true) {
    const auto width = static_cast<unsigned>(std::ceil(std::log10(num_states + 1)));

    std::stringstream ss;
    for (state_t first = 0; first < num_states; ++first) {
        for (state_t second = 0; second < num_states; ++second) {
            const auto from = state_pair_t{first, second};
            const auto to = transition(protocol, from);
            const auto no_change =
                (from == to) || (from.first == to.second && from.second == to.first);

            if (vt100 && no_change)
                ss << "\e[90m";
            // ss << '(' << std::setw(width) << from.first << ',' << std::setw(width) << from.second
            // << ")->";
            if constexpr (is_one_way<Protocol>) {
                ss << std::setw(width) << to.first << ", ";
            } else {
                ss << '(' << std::setw(width) << to.first << ',' << std::setw(width) << to.second
                   << "), ";
            }
            if (vt100 && no_change)
                ss << "\e[39m";
        }
        ss << '\n';
    }

    return ss.str();
}

template <typename Protocol>
auto transactions_without_change(const Protocol &protocol, unsigned num_states) {
    std::vector<std::vector<state_t>> skip_trans(num_states);
    size_t skips = 0;
    for (state_t first = 0; first < num_states; ++first) {
        for (state_t second = 0; second < num_states; ++second) {
            const auto from = state_pair_t{first, second};
            const auto to = transition(protocol, from);
            const auto no_change =
                (from == to) || (from.first == to.second && from.second == to.first);

            skips += no_change;
            if (no_change)
                skip_trans[first].emplace_back(second);
        }
    }
    return std::make_pair(skip_trans, skips);
}

using OneWayPartitions = std::vector<std::vector<std::pair<std::vector<state_t>, state_t>>>;

template <typename Protocol>
OneWayPartitions parition_oneway_transactions(const Protocol &protocol, unsigned num_states) {
    OneWayPartitions mapping;
    for (state_t first = 0; first < num_states; ++first) {
        std::map<state_t, std::vector<state_t>> row_map;
        for (state_t second = 0; second < num_states; ++second) {
            const auto from = state_pair_t{first, second};
            const auto to = transition(protocol, from);
            assert(from.second == to.second);

            row_map[to.first].emplace_back(from.second);
        }

        mapping.emplace_back();
        mapping.back().reserve(row_map.size());
        for (auto &grp : row_map) {
            mapping.back().emplace_back(std::move(grp.second), grp.first);
        }
    }

    return mapping;
}

} // namespace Protocols
} // namespace pps
