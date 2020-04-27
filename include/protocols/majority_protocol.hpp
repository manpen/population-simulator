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

class MajorityProtocol : public pps::Protocols::DeterministicProtocol {
public:
    struct logical_t { ///< Logical state
        bool opinion;
        bool strong;
    };

    /// Convert logical representation into numerical
    pps::state_t encode(logical_t x) const noexcept { return (2 * x.strong) | x.opinion; }

    /// Convert numerical representation into logical
    logical_t decode(pps::state_t x) const noexcept {
        logical_t state;
        state.opinion = x & 0b01;
        state.strong = x & 0b10;
        return state;
    }

    constexpr pps::state_t num_states() const noexcept { return 4; }

    pps::state_pair_t operator()(pps::state_t fst, pps::state_t snd) const {
        auto first = decode(fst);
        auto second = decode(snd);

        if (first.strong == second.strong) {
            first.strong = false;
            second.strong = false;
        } else if (first.strong) {
            second.opinion = first.opinion;
        } else /* second.strong */ {
            first.opinion = second.opinion;
        }

        return {encode(first), encode(second)};
    }
};
