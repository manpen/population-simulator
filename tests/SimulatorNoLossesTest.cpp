#include <algorithm>
#include <gtest/gtest.h>

#include <urns/LinearUrn.hpp>
#include <urns/TreeUrn.hpp>

#include <pps/AsyncBatchSimulator.hpp>
#include <pps/AsyncPopulationSimulator.hpp>
#include <pps/AsyncDistributionSimulator.hpp>

#include <protocols/increment_one_protocol.hpp>

template <typename Protocol>
class SimulatorNoLossesTest : public ::testing::Test {};

template <typename Protocol, typename Simulator>
void count_interactions(size_t num_agents, size_t num_states, std::mt19937_64& gen) {
    const auto max_states = static_cast<pps::state_t>(0.9 * num_states);
    using urn_type = typename Simulator::urn_type;

    urn_type initial_urn(num_states);
    initial_urn.add_balls(0, num_agents);

    constexpr auto updates_per_interaction = Protocol::kIncreasePerInteraction;
    Simulator simulator(std::move(initial_urn), Protocol{}, gen);

    size_t num_interactions{0};
    pps::state_t max_used_state{0};
    simulator.run([&] (const auto& sim) {
        auto&& agents = simulator.agents();

        // compute number of interactions necessary to reach current distribution
        num_interactions = [&] {
            size_t state_sum = 0;

            for(auto i = 1; i < agents.number_of_colors(); ++i) {
                const auto n = agents.number_of_balls_with_color(i);
                state_sum += i * n;
            }

            return state_sum / updates_per_interaction;
        }();

        max_used_state = [&] () -> pps::state_t {
            for(pps::state_t i = agents.number_of_colors() - 1; i; --i)
                if (agents.number_of_balls_with_color(i))
                    return i;
            return 0;
        }();

        if (max_used_state >= max_states || num_interactions != sim.num_interactions())
            return false;

        return true;
    });

    ASSERT_EQ(num_interactions, simulator.num_interactions())
                        << " max_used_state " << max_used_state;

    ASSERT_GE(num_interactions, max_states * num_agents / 2 / updates_per_interaction)
                        << " max_used_state " << max_used_state;

}

using MyProtocols = ::testing::Types<
    IncrementOneProtocol<IncrementOneStrategy::OneWay>,
    IncrementOneProtocol<IncrementOneStrategy::TwoWayFirst>,
    IncrementOneProtocol<IncrementOneStrategy::TwoWaySecond>,
    IncrementOneProtocol<IncrementOneStrategy::TwoWayBoth>
>;
TYPED_TEST_CASE(SimulatorNoLossesTest, MyProtocols);

constexpr size_t kNumAgents = 100;
constexpr size_t kNumRounds = 1000;

TYPED_TEST(SimulatorNoLossesTest, BatchSim) {
    std::mt19937_64 gen(10 + static_cast<unsigned>(TypeParam::kStrategy));
    count_interactions<TypeParam, pps::AsyncBatchSimulator<TypeParam, std::mt19937_64>>(kNumAgents, kNumRounds, gen);
}

TYPED_TEST(SimulatorNoLossesTest, DistrSimLinear) {
    std::mt19937_64 gen(20 + static_cast<unsigned>(TypeParam::kStrategy));
    count_interactions<TypeParam, pps::AsyncDistributionSimulator<urns::LinearUrn, TypeParam, std::mt19937_64>>(kNumAgents, kNumRounds, gen);
}

TYPED_TEST(SimulatorNoLossesTest, DistrSimTree) {
    std::mt19937_64 gen(30 + static_cast<unsigned>(TypeParam::kStrategy));
    count_interactions<TypeParam, pps::AsyncDistributionSimulator<urns::TreeUrn, TypeParam, std::mt19937_64>>(kNumAgents, kNumRounds, gen);
}

TYPED_TEST(SimulatorNoLossesTest, PopSimPrefetch0) {
    std::mt19937_64 gen(40 + static_cast<unsigned>(TypeParam::kStrategy));
    count_interactions<TypeParam, pps::AsyncPopulationSimulator<0, TypeParam, std::mt19937_64>>(kNumAgents, kNumRounds, gen);
}

TYPED_TEST(SimulatorNoLossesTest, PopSimPrefetch1) {
    std::mt19937_64 gen(50 + static_cast<unsigned>(TypeParam::kStrategy));
    count_interactions<TypeParam, pps::AsyncPopulationSimulator<1, TypeParam, std::mt19937_64>>(kNumAgents, kNumRounds, gen);
}

TYPED_TEST(SimulatorNoLossesTest, PopSimPrefetch10) {
    std::mt19937_64 gen(60 + static_cast<unsigned>(TypeParam::kStrategy));
    count_interactions<TypeParam, pps::AsyncPopulationSimulator<10, TypeParam, std::mt19937_64>>(kNumAgents, kNumRounds, gen);
}
