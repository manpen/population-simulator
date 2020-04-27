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
#include <pps/Protocols.hpp>

// copied from YAPPS
namespace yapps {
using Clock = unsigned;

const bool inInterval(const Clock value, const Clock low, const Clock high) {
    return !(value < low) && (value < high);
}

const Clock modAbsoluteDifference(const Clock a, const Clock b, const Clock m) {
    if (b > a)
        return modAbsoluteDifference(b, a, m);

    return std::min(a - b, m - (a - b));
}

inline bool gt(const Clock clock1, const Clock clock2, const Clock m) {
    return (clock2 > clock1 && clock2 < clock1 + m / 2)
           || (clock2 < clock1 && clock2 + (m + 1) / 2 < clock1);
}
} // namespace yapps

// Define behaivour of the simulation
class ClockProtocol : public pps::Protocols::DeterministicProtocol, pps::Protocols::OneWayProtocol {
public:
    ClockProtocol() = delete;
    explicit ClockProtocol(yapps::Clock digits_on_clock) : digits_on_clock_(digits_on_clock) {}

    using state_t = pps::state_t;

    state_t operator()(state_t act_state, const state_t pas_state) const {
        auto active = decode(act_state);
        const auto passive = decode(pas_state);

        active.clock += yapps::gt(active.clock, passive.clock, digits_on_clock_)
                        || (active.clock == passive.clock && passive.marked);
        active.clock = (active.clock >= digits_on_clock_) ? 0 : active.clock;

        return encode(active);
    }

    struct logical_t {
        yapps::Clock clock;
        bool marked;
    };

    state_t encode(logical_t x) const noexcept {
        assert(x.clock < digits_on_clock_);
        return x.clock + digits_on_clock_ * x.marked;
    }

    logical_t decode(state_t x) const noexcept {
        assert(x < 2 * digits_on_clock_);
        const auto marked = (x >= digits_on_clock_);
        return logical_t{x - digits_on_clock_ * marked, marked};
    }

    // The remainder is application specific and not required by the simulator
    state_t num_states() const noexcept { return 2 * digits_on_clock_; }

    yapps::Clock digits_on_clock() const noexcept { return digits_on_clock_; }

    template <typename Agents>
    yapps::Clock compute_max_gap(const Agents &agents, size_t threshold) const noexcept {
        auto is_empty = [&](unsigned digit) {
            return agents.number_of_balls_with_color(encode({digit, false}))
                       + agents.number_of_balls_with_color(encode({digit, true}))
                   <= threshold;
        };

        // find the long contigious of gap (i.e. a digit without agents)
        yapps::Clock max_gap = 0;
        for (yapps::Clock i = 0; i < digits_on_clock(); ++i) {
            if (!is_empty(i))
                continue;

            yapps::Clock gap_length = 1;
            for (; gap_length < digits_on_clock() - 1; ++gap_length) {
                auto digit = (i + gap_length) % digits_on_clock();
                if (!is_empty(digit))
                    break;
            }

            max_gap = std::max(gap_length, max_gap);
        }

        return max_gap;
    }

    template <typename Agents>
    void create_uniform_distribution(Agents &agents, size_t num_agents_upper_bound,
                                     size_t num_marked_upper_bound) {
        const auto num_agents_per_digit = (num_agents_upper_bound / digits_on_clock());
        const auto num_marked_per_digit = (num_marked_upper_bound / digits_on_clock());

        for (yapps::Clock i = 0; i < digits_on_clock(); ++i) {
            agents.add_balls(encode({i, false}), num_agents_per_digit - num_marked_per_digit);
            agents.add_balls(encode({i, true}), num_marked_per_digit);
        }
    }

private:
    yapps::Clock digits_on_clock_; // digits on clock
};
