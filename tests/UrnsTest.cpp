#include <algorithm>
#include <gtest/gtest.h>

#include <urns/AliasUrnSimple.hpp>
#include <urns/LinearUrn.hpp>
#include <urns/TreeUrn.hpp>
#include <pps/WeightedUrn.hpp>

template <typename T>
class UrnsTest : public ::testing::Test {

protected:
    template <typename Urn>
    auto random_fill_urn(Urn& urn, std::mt19937_64& gen) {
        const auto num_colors = urn.number_of_colors();
        std::uniform_int_distribution<unsigned> distr_num_balls(0, 100);
        size_t num_balls = 0;
        std::vector<size_t> nums_balls(urn.number_of_colors(), 0);
        for(unsigned int c = 0; c < num_colors; ++c) {
            nums_balls[c] = urn.number_of_balls_with_color(c);
            num_balls += nums_balls[c];
        }

        for(unsigned int c = 0; c < num_colors; ++c) {
            const auto num = distr_num_balls(gen);
            urn.add_balls(c, num);
            nums_balls[c] += num;
            num_balls += num;
            assert(urn.number_of_balls_with_color(c) == nums_balls[c]);
        }

        return std::make_pair(num_balls, nums_balls);
    }
};

using MyUrns = ::testing::Types<
    urns::TreeUrn,
    urns::AliasUrnSimple,
    urns::LinearUrn,
    pps::WeightedUrn
>;
TYPED_TEST_CASE(UrnsTest, MyUrns);

TYPED_TEST(UrnsTest, AddGetRemoveSingle) {
    std::uniform_int_distribution<unsigned> distr_num_colors(2, 100);

    std::mt19937_64 gen(1);

    for(unsigned int num_colors = 2; num_colors < 100; ++num_colors) {
        for(unsigned c = 0; c < num_colors; ++c) {
            TypeParam urn(num_colors);
            urn.add_balls(c, num_colors);

            ASSERT_FALSE(urn.empty());
            ASSERT_EQ(urn.get_random_ball(gen), c);
            ASSERT_EQ(urn.remove_random_ball(gen), c);
            ASSERT_EQ(urn.number_of_balls(), num_colors-1);
        }
    }
}

TYPED_TEST(UrnsTest, AddGetRemoveMany) {
    if constexpr (urns::traits::has_bulk_insertions_v<TypeParam>)
        return;

    const auto num = 100;
    std::mt19937_64 gen(1);

    for(unsigned int num_colors = 2; num_colors < 100; ++num_colors) {
        for(unsigned c = 0; c < num_colors; ++c) {
            TypeParam urn(num_colors);
            urn.add_balls(c, num);

            ASSERT_FALSE(urn.empty());
            ASSERT_EQ(urn.get_random_ball(gen), c);

            for(unsigned j = 0; j < num; ++j)
                ASSERT_EQ(urn.remove_random_ball(gen), c);

            ASSERT_TRUE(urn.empty());
        }
    }
}

TYPED_TEST(UrnsTest, FillRemove) {
    std::uniform_int_distribution<unsigned> distr_num_colors(2, 100);

    std::mt19937_64 gen(1);

    for(unsigned int i = 0; i < 100; ++i) {
        const auto num_colors = distr_num_colors(gen);
        std::uniform_int_distribution<unsigned> distr_color(0, num_colors-1);

        TypeParam urn(num_colors);
        ASSERT_TRUE(urn.empty());
        for(unsigned c=0; c < num_colors; ++c) {
            if constexpr (urns::traits::has_bulk_insertions_v<TypeParam>) {
                urn.bulk_add_balls(c, 2);
            } else {
                urn.add_balls(c, 2);
            }
        }
        if constexpr (urns::traits::has_bulk_insertions_v<TypeParam>)
            urn.bulk_commit();

        auto [num_balls, nums_balls] = this->random_fill_urn(urn, gen);
        ASSERT_FALSE(urn.empty());

        std::bernoulli_distribution distr_add(0.1);
        while(num_balls > num_colors) {
            if (distr_add(gen)) {
                const auto color = distr_color(gen);
                urn.add_balls(color);
                nums_balls[color]++;
                num_balls++;
            } else {
                const auto color = urn.remove_random_ball(gen);
                ASSERT_GT(nums_balls[color], 0);
                nums_balls[color]--;
                num_balls--;
            }
            ASSERT_EQ(urn.number_of_balls(), num_balls);
        }
    }
}

TYPED_TEST(UrnsTest, FillGet) {
    std::uniform_int_distribution<unsigned> distr_num_colors(4, 100);
    std::uniform_int_distribution<unsigned> distr_num_balls(0, 100);

    std::mt19937_64 gen(1);

    for(unsigned int i = 0; i < 100; ++i) {
        const auto num_colors = distr_num_colors(gen);

        TypeParam urn(num_colors);
        ASSERT_TRUE(urn.empty());
        urn.add_balls(0, num_colors);

        auto [num_balls, nums_balls] = this->random_fill_urn(urn, gen);
        ASSERT_FALSE(urn.empty());

        for(unsigned int c = 0; c < num_colors * 100; ++c) {
            const auto color = urn.get_random_ball(gen);
            ASSERT_GT(nums_balls[color], 0);
        }

        ASSERT_EQ(urn.number_of_balls(), num_balls);
    }
}

TYPED_TEST(UrnsTest, RepeatedFillNum) {
    std::mt19937_64 gen(1);
    std::uniform_int_distribution<unsigned> distr_num_colors(4, 100);
    std::uniform_int_distribution<unsigned> distr_num_balls(0, 100);

    for(unsigned int i = 0; i < 100; ++i) {
        const auto num_colors = distr_num_colors(gen);
        std::uniform_int_distribution<unsigned> distr_color(0, num_colors-1);
        TypeParam urn(num_colors);

        std::vector<size_t> nums_balls(urn.number_of_colors());
        size_t num_balls = 0;

        for(unsigned int c = 0; c < 10 * num_colors; ++c) {
            const auto color = distr_color(gen);
            const auto num = distr_num_balls(gen);
            urn.add_balls(color, num);
            nums_balls[color] += num;
            num_balls += num;

            ASSERT_EQ(urn.number_of_balls(), num_balls);
            for(unsigned int col = 0; col < num_colors; ++col)
                ASSERT_EQ(urn.number_of_balls_with_color(col), nums_balls[col]) << i << " " << c << " " << col;
        }
    }
}

TEST(TreeUrn, AddUrn) {
    std::uniform_int_distribution<unsigned> distr_num_colors(2, 100);

    std::mt19937_64 gen(1);

    for(unsigned int num_colors = 2; num_colors < 20; ++num_colors) {
        for(unsigned c = 0; c < num_colors; ++c) {
            urns::TreeUrn org(num_colors);
            org.add_balls(c);

            urns::TreeUrn copy(num_colors);
            copy.add_urn(org);

            urns::TreeUrn copy_twice(num_colors);
            copy_twice.add_urn(org);
            copy_twice.add_urn(org);

            ASSERT_EQ(copy.number_of_balls(), 1u);
            ASSERT_EQ(copy_twice.number_of_balls(), 2u);

            for(unsigned cc = 0; cc < num_colors; ++cc) {
                ASSERT_EQ(copy.number_of_balls_with_color(cc), cc == c ? 1u : 0u);
                ASSERT_EQ(copy_twice.number_of_balls_with_color(cc), cc == c ? 2u : 0u);
            }

            ASSERT_FALSE(copy.empty());
            ASSERT_EQ(copy.get_random_ball(gen), c) << num_colors << ' ' << c;
            ASSERT_EQ(copy.remove_random_ball(gen), c);
            ASSERT_TRUE(copy.empty());
        }
    }
}
