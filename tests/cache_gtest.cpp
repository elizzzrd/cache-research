#include "cache_test_common.hpp"
#include "lru_cache.hpp"
#include "2q_cache.hpp"
#include "arc_cache.hpp"
#include "lfu_cache.hpp"
#include "lirs_cache.hpp"
#include "ideal_cache.hpp"

#include <cstdint>
#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using cache_tests::TestPage;
using cache_tests::PageLoader;
using cache_tests::expect_page;
using cache_tests::check_requests;

namespace 
{
    const std::vector<caches::Key> repeated_requests(1001, 7);
    const std::vector<caches::Key> copy_requests{7, 7, 7};
    const std::vector<caches::Key> zero_requests{1, 1, 2, 1};
    const std::vector<caches::Key> one_requests{1, 2, 1, 1, 3, 3, 2};
    const std::vector<std::string> string_requests{"", "alpha", "", "alpha"};
    const std::vector<std::uint64_t> wide_requests{
        1, (std::uint64_t{1} << 32) + 1, 1, (std::uint64_t{1} << 32) + 1};

    template <typename Cache>
    void check_construction(const Cache& cache, std::size_t expected_capacity) {
        EXPECT_EQ(cache.size(), 0u);
        EXPECT_EQ(cache.capacity(), expected_capacity);
        static_assert(std::is_same_v<decltype(cache.size()), std::size_t>);
        static_assert(std::is_same_v<decltype(cache.capacity()), std::size_t>);
    }

    template <typename Cache>
    void check_lookup_result(Cache& cache) {
        PageLoader<> load;
        const caches::Key key = 7;

        auto first = cache.lookup_update(key, load);
        static_assert(std::is_same_v<decltype(first), caches::LookupResult<TestPage>>);
        static_assert(std::is_same_v<decltype(first.value), TestPage>);
        static_assert(std::is_same_v<decltype(first.hit), bool>);
        EXPECT_FALSE(first.hit);
        expect_page(first.value, 1);
        EXPECT_EQ(load.calls, 1);
        EXPECT_EQ(cache.size(), 1u);

        for (int repeat = 0; repeat < 1000; ++repeat) {
            SCOPED_TRACE(repeat);
            const auto result = cache.lookup_update(key, load);
            EXPECT_TRUE(result.hit);
            expect_page(result.value, 1);
            EXPECT_EQ(load.calls, 1);
            EXPECT_EQ(cache.size(), 1u);
        }
    }

    template <typename Cache>
    void check_returned_copy(Cache& cache) {
        PageLoader<> load;
        const caches::Key key = 7;

        auto first = cache.lookup_update(key, load);
        first.value.payload = "changed after miss";
        first.value.data_.clear();

        auto second = cache.lookup_update(key, load);
        EXPECT_TRUE(second.hit);
        expect_page(second.value, 1);
        second.value.payload = "changed after hit";
        second.value.data_.clear();

        const auto third = cache.lookup_update(key, load);
        EXPECT_TRUE(third.hit);
        expect_page(third.value, 1);
        EXPECT_EQ(load.calls, 1);
    }

    struct FailingLoader {
        int calls = 0;
        TestPage operator()(const std::string& key) {
            ++calls;
            if (key == "broken") {
                throw std::runtime_error("source failed");
            }
            return TestPage{calls};
        }
    };

    template <typename Cache>
    void check_failed_load(Cache& cache) 
    {
        FailingLoader load;

        EXPECT_FALSE(cache.lookup_update("saved", load).hit);
        EXPECT_THROW(cache.lookup_update("broken", load), std::runtime_error);
        EXPECT_EQ(cache.size(), 1u);
        const auto result = cache.lookup_update("saved", load);
        EXPECT_TRUE(result.hit);
        expect_page(result.value, 1);
        EXPECT_EQ(load.calls, 2);
    }
} 


template <typename KeyT>
using CacheTypes = ::testing::Types<
    caches::LruCache<TestPage, KeyT>,
    caches::TwoQCache<TestPage, KeyT>,
    caches::ArcCache<TestPage, KeyT>,
    caches::LfuCache<TestPage, KeyT>,
    caches::LirsCache<TestPage, KeyT>
>;

template <typename Cache>
class CacheContract : public ::testing::Test {};

template <typename Cache>
class StringKeyContract : public ::testing::Test {};

template <typename Cache>
class WideKeyContract : public ::testing::Test {};

using DefaultCaches = CacheTypes<caches::Key>;
using StringCaches = CacheTypes<std::string>;
using WideCaches = CacheTypes<std::uint64_t>;

TYPED_TEST_SUITE(CacheContract, DefaultCaches);
TYPED_TEST_SUITE(StringKeyContract, StringCaches);
TYPED_TEST_SUITE(WideKeyContract, WideCaches);

TYPED_TEST(CacheContract, Construction) {
    TypeParam cache{3};
    check_construction(cache, 3);
}

TYPED_TEST(CacheContract, LookupReturnsPageAndHitFlag) {
    TypeParam cache{2};
    check_lookup_result(cache);
}

TYPED_TEST(CacheContract, ReturnedPageIsIndependentCopy) {
    TypeParam cache{2};
    check_returned_copy(cache);
}

TYPED_TEST(CacheContract, ZeroCapacityAlwaysMisses) {
    TypeParam cache{0};
    check_requests(cache, zero_requests, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TYPED_TEST(CacheContract, CapacityOneReplacesPreviousPage) {
    TypeParam cache{1};
    check_requests(cache, one_requests, {false, false, false, true, false, true, false});
    EXPECT_EQ(cache.size(), 1u);
}

TYPED_TEST(StringKeyContract, SupportsStringKeys) {
    TypeParam cache{4};
    check_requests(cache, string_requests, {false, false, true, true});
    EXPECT_EQ(cache.size(), 2u);
}

TYPED_TEST(StringKeyContract, FailedLoadPreservesStoredPage) {
    TypeParam cache{1};
    check_failed_load(cache);
}

TYPED_TEST(WideKeyContract, DoesNotNarrowIntegerKeys) {
    TypeParam cache{4};
    check_requests(cache, wide_requests, {false, false, true, true});
    EXPECT_EQ(cache.size(), 2u);
}

// Ideal 
TEST(IdealCacheContract, Construction) {
    caches::IdealCache<TestPage, caches::Key> cache{3, repeated_requests};
    check_construction(cache, 3);
}

TEST(IdealCacheContract, LookupReturnsPageAndHitFlag) {
    caches::IdealCache<TestPage, caches::Key> cache{2, repeated_requests};
    check_lookup_result(cache);
}

TEST(IdealCacheContract, ReturnedPageIsIndependentCopy) {
    caches::IdealCache<TestPage, caches::Key> cache{2, copy_requests};
    check_returned_copy(cache);
}

TEST(IdealCacheContract, ZeroCapacityAlwaysMisses) {
    caches::IdealCache<TestPage, caches::Key> cache{0, zero_requests};
    check_requests(cache, zero_requests, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(IdealCacheContract, CapacityOneReplacesPreviousPage) {
    caches::IdealCache<TestPage, caches::Key> cache{1, one_requests};
    check_requests(cache, one_requests, {false, false, false, true, false, true, false});
    EXPECT_EQ(cache.size(), 1u);
}

TEST(IdealCacheContract, SupportsStringKeys) {
    caches::IdealCache<TestPage, std::string> cache{4, string_requests};
    check_requests(cache, string_requests, {false, false, true, true});
    EXPECT_EQ(cache.size(), 2u);
}

TEST(IdealCacheContract, DoesNotNarrowIntegerKeys) {
    caches::IdealCache<TestPage, std::uint64_t> cache{4, wide_requests};
    check_requests(cache, wide_requests, {false, false, true, true});
    EXPECT_EQ(cache.size(), 2u);
}


// -------------------------------------------------------------------------------
TEST(LruCacheContract, HitUpdatesEvictionOrder) {
    caches::LruCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 2, 1, 3, 1, 2},
                   {false, false, true, false, true, false});
}

TEST(LruCacheContract, RepeatedHitOnFrontPreservesEvictionOrder) {
    caches::LruCache<TestPage> cache{3};
    check_requests(cache,
                   {1, 2, 3, 3, 3, 4, 2, 3, 1},
                   {false, false, false, true, true, false, true, true, false});
    EXPECT_EQ(cache.size(), 3u);
}

// ------------------------------------------------------------------------------
TEST(TwoQCacheContract, RepeatedPageSurvivesNewRequests) {
    caches::TwoQCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 1, 2, 3, 1},
                   {false, true, false, false, true});
}

TEST(TwoQCacheContract, GhostRequestLoadsPageIntoMainQueue) {
    caches::TwoQCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 2, 1, 3, 2, 4, 2},
                   {false, false, true, false, false, false, true});
}

TEST(TwoQCacheContract, EmptyRecentQueueEvictsFromMainQueue) {
    caches::TwoQCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 1, 2, 2, 3, 2, 1},
                   {false, true, false, true, false, true, false});
}

TEST(TwoQCacheContract, ForgottenGhostIsInsertedAsNewPage) {
    caches::TwoQCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 1, 2, 3, 4, 2, 5, 1},
                   {false, true, false, false, false, false, false, true});
}

// ---------------------------------------------------------------------------------
TEST(ArcCacheContract, UniqueRequestsKeepTargetAtZero) {
    caches::ArcCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 2, 3, 4, 5},
                   {false, false, false, false, false});
    EXPECT_EQ(cache.target_t1_size(), 0u);
}

TEST(ArcCacheContract, GhostHitsIncreaseAndDecreaseTarget) {
    caches::ArcCache<TestPage> cache{2};
    PageLoader<> load;

    const std::vector<caches::Key> requests{1, 2, 1, 3, 2, 1};
    const std::vector<bool> hits{false, false, true, false, false, false};
    const std::vector<int> page_ids{1, 2, 1, 3, 4, 5};
    const std::vector<std::size_t> targets{0, 0, 0, 0, 1, 0};
    
    int expected_calls = 0;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        SCOPED_TRACE(i);
        if (!hits[i]) {
            ++expected_calls;
        }
        const auto result = cache.lookup_update(requests[i], load);
        EXPECT_EQ(result.hit, hits[i]);
        expect_page(result.value, page_ids[i]);
        EXPECT_EQ(load.calls, expected_calls);
        EXPECT_EQ(cache.target_t1_size(), targets[i]);
        EXPECT_LE(cache.size(), cache.capacity());
    }
}

TEST(ArcCacheContract, TargetStaysWithinCapacityDuringMixedRequests) {
    for (const std::size_t capacity : std::vector<std::size_t>{1, 2, 3, 10}) {
        SCOPED_TRACE(capacity);
        caches::ArcCache<TestPage> cache{capacity};
        PageLoader<> load;
        for (int i = 0; i < 300; ++i) {
            SCOPED_TRACE(i);
            // Alternate a small working set and a scan of different keys.
            const caches::Key key = static_cast<caches::Key>(
                i % 30 < 15 ? i % 3 : 3 + i % 17);
            cache.lookup_update(key, load);
            EXPECT_LE(cache.target_t1_size(), capacity);
            EXPECT_LE(cache.size(), capacity);
        }
    }
}

// --------------------------------------------------------------------------------------
TEST(LfuCacheContract, EvictsLessFrequentPage) {
    caches::LfuCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 1, 1, 2, 3, 1},
                   {false, true, true, false, false, true});
}

TEST(LfuCacheContract, EqualFrequenciesUseLruOrder) {
    caches::LfuCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 2, 1, 2, 3, 2, 1},
                   {false, false, true, true, false, true, false});
}

TEST(LfuCacheContract, ReloadedPageStartsWithFrequencyOne) {
    caches::LfuCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 1, 2, 2, 2, 3, 1, 4, 2, 1},
                   {false, true, false, true, true,
                    false, false, false, true, false});
}

TEST(LfuCacheContract, EmptyNonMinimumBucketDoesNotChangeMinimum) {
    caches::LfuCache<TestPage> cache{3};
    check_requests(cache,
                   {1, 2, 3, 1, 1, 4, 1, 3, 2},
                   {false, false, false, true, true, false, true, true, false});
}

// ---------------------------------------------------------------------------------------
TEST(LirsCacheContract, ResidentHirInStackBecomesLir) {
    caches::LirsCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 2, 2, 3, 2, 1},
                   {false, false, true, false, true, false});
}

TEST(LirsCacheContract, NonResidentHirRequiresLoading) {
    caches::LirsCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 2, 3, 2, 4, 2},
                   {false, false, false, false, false, true});
}

TEST(LirsCacheContract, PrunedGhostIsTreatedAsNewPage) {
    caches::LirsCache<TestPage> cache{2};
    check_requests(cache,
                   {1, 2, 3, 1, 2, 4, 1},
                   {false, false, false, true, false, false, true});
}

TEST(LirsCacheContract, ResidentHirOutsideStackReturnsToStack) {
    caches::LirsCache<TestPage> cache{3};
    check_requests(cache,
                   {1, 2, 3, 1, 3, 2, 4, 2, 5, 2},
                   {false, false, false, true, true, true, false, false, false, true});
    EXPECT_EQ(cache.size(), 3u);
}

// -------------------------------------------------------------------------------------
TEST(IdealCacheContract, EvictsPageNeededFarthestInFuture) {
    const std::vector<caches::Key> requests{1, 2, 3, 1, 2, 3};
    caches::IdealCache<TestPage> cache{2, requests};
    check_requests(cache,
                   requests,
                   {false, false, false, true, false, true});
}

TEST(IdealCacheContract, EmptyRequestSequence) {
    const std::vector<caches::Key> requests;
    caches::IdealCache<TestPage> cache{2, requests};
    check_construction(cache, 2);
}

TEST(IdealCacheContract, EvictsPageWithNoFutureRequests) {
    const std::vector<caches::Key> requests{1, 2, 3, 1, 3};
    caches::IdealCache<TestPage> cache{2, requests};
    check_requests(cache, requests, {false, false, false, true, true});
}

TEST(IdealCacheContract, UniqueRequestsDoNotRequireATieBreakingOrder) {
    const std::vector<caches::Key> requests{1, 2, 3, 4, 5};
    caches::IdealCache<TestPage> cache{2, requests};

    check_requests(cache, requests, {false, false, false, false, false});
    EXPECT_EQ(cache.size(), 2u);
}