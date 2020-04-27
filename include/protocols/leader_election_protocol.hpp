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

// Define behaivour of the simulation
class LeaderElectionProtocol : public pps::Protocols::OneWayProtocol,
                               pps::Protocols::DeterministicProtocol {
public:
    enum Roles : pps::state_t { Follower = 0, Leader = 1 };

    pps::state_t operator()(pps::state_t first, const pps::state_t second) const {
        return (first == Leader && second == Leader) ? Follower : first;
    }

    constexpr static pps::state_t num_states() { return 2; }
};
