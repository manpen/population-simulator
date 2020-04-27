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

enum class IncrementOneStrategy { OneWay, TwoWayFirst, TwoWaySecond, TwoWayBoth };

template <IncrementOneStrategy Strategy>
struct IncrementOneProtocol : public pps::Protocols::DeterministicProtocol {
private:
    static constexpr bool kIncreaseFirst =
        (Strategy == IncrementOneStrategy::OneWay || Strategy == IncrementOneStrategy::TwoWayFirst
         || Strategy == IncrementOneStrategy::TwoWayBoth);
    static constexpr bool kIncreaseSecond = (Strategy == IncrementOneStrategy::TwoWaySecond
                                             || Strategy == IncrementOneStrategy::TwoWayBoth);

public:
    static constexpr auto kStrategy = Strategy;
    static constexpr size_t kIncreasePerInteraction = kIncreaseFirst + kIncreaseSecond;
    static_assert(kIncreasePerInteraction >= 1);

    pps::state_pair_t operator()(pps::state_t first, pps::state_t second) const noexcept {
        return pps::state_pair_t(first + kIncreaseFirst, second + kIncreaseSecond);
    }
};

template <>
struct IncrementOneProtocol<IncrementOneStrategy::OneWay>
    : public pps::Protocols::DeterministicProtocol, pps::Protocols::OneWayProtocol {
    static constexpr auto kStrategy = IncrementOneStrategy::OneWay;
    static constexpr size_t kIncreasePerInteraction = 1;

    pps::state_t operator()(pps::state_t first, pps::state_t second) const noexcept {
        return first + 1;
    }
};
