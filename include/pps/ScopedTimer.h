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

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace pps {

template <bool Enabled = true>
class ScopedTimer {
    using Clock = std::chrono::high_resolution_clock;

    Clock::time_point m_begin;
    bool m_started{false};

    std::string m_prefix;
    double *m_output{nullptr};

public:
    ScopedTimer() : m_begin(Clock::now()) {}

    ScopedTimer(const std::string &prefix, bool autostart = true)
        : m_prefix(prefix), m_output(nullptr) {
        if (autostart)
            start();
    }

    ScopedTimer(double &output, bool autostart = true) : m_output(&output) {
        if (autostart)
            start();
    }

    ScopedTimer(const std::string &prefix, double &output, bool autostart = true)
        : m_prefix(prefix), m_output(&output) {
        if (autostart)
            start();
    }

    ~ScopedTimer() {
        if (!m_started)
            return;

        if (!m_prefix.empty())
            report();

        if (m_output)
            *m_output = elapsed();
    }

    void start() noexcept {
        m_begin = Clock::now();
        m_started = true;
    }

    double elapsed() const noexcept {
        const auto t2 = Clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t2 - m_begin)
            .count();
    }

    double report() const { return report(m_prefix); }

    double report(const std::string &prefix) const {
        assert(m_started);
        const double timeUs = elapsed();
        std::cout << prefix << " Time elapsed: " << timeUs << "ms" << std::endl;

        return timeUs;
    }
};

template <>
class ScopedTimer<false> { // dummy disabled implementation
public:
    ScopedTimer() {}

    ScopedTimer(const std::string &prefix, bool autostart = true) {}

    ScopedTimer(double &output, bool autostart = true) {}

    ScopedTimer(const std::string &prefix, double &output, bool autostart = true) {}

    void start() noexcept {}

    double elapsed() const noexcept { return 0.0; }

    double report() const { return 0.0; }

    double report(const std::string &prefix) const { return 0.0; }
};

} // namespace pps
