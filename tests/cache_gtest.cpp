#include "cache_test_common.hpp"

#include "lru_cache.hpp"
#include "2q_cache.hpp"
#include "arc_cache.hpp"
#include "ideal_cache.hpp"
#include "lirs_cache.hpp"
#include "lfu_cache.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

using cache_tests::TestPage;
using cache_tests::check_requests;
using cache_tests::check_repeated_requests;


// ---------- repeated requests ----------
TEST(LruCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::LruCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(TwoQCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::TwoQCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(ArcCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::ArcCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(LfuCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::LfuCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(LirsCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::LirsCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(IdealCacheTest, RepeatedRequestsReturnStoredPage) {
    const caches::Key key = 7;
    const std::vector<caches::Key> requests(4, key);
    caches::IdealCache<TestPage> cache{2, requests};

    check_repeated_requests(cache, key);
}

// ---------- zero capacity ----------
TEST(LruCacheTest, ZeroCapacityAlwaysMisses) {
    caches::LruCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(TwoQCacheTest, ZeroCapacityAlwaysMisses) {
    caches::TwoQCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(ArcCacheTest, ZeroCapacityAlwaysMisses) {
    caches::ArcCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(LfuCacheTest, ZeroCapacityAlwaysMisses) {
    caches::LfuCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(LirsCacheTest, ZeroCapacityAlwaysMisses) {
    caches::LirsCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(IdealCacheTest, ZeroCapacityAlwaysMisses) {
    const std::vector<caches::Key> requests{1, 1, 2, 1};
    caches::IdealCache<TestPage> cache{0, requests};

    check_requests(cache, requests, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}


// LRU --------------------------------------------------------------------
TEST(LruCacheTest, HitUpdatesEvictionOrder) {
    caches::LruCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 2, 1, 3, 1, 2},
                   {false, false, true, false, true, false});
}


// 2Q --------------------------------------------------------------------
TEST(TwoQCacheTest, RepeatedPageSurvivesNewRequests) {
    caches::TwoQCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 1, 2, 3, 1},
                   {false, true, false, false, true});
}

TEST(TwoQCacheTest, GhostRequestLoadsPageIntoMainQueue) {
    caches::TwoQCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 2, 1, 3, 2, 4, 2},
                   {false, false, true, false, false, false, true});
}


// ARC --------------------------------------------------------------------
TEST(ArcCacheTest, UniqueRequestsKeepTargetAtZero) {
    caches::ArcCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 2, 3, 4, 5},
                   {false, false, false, false, false});

    EXPECT_EQ(cache.target_t1_size(), 0u);
}

// LFU --------------------------------------------------------------------
TEST(LfuCacheTest, EvictsLessFrequentPage)
{
    caches::LfuCache<TestPage> cache{2};

    check_requests(
        cache,
        {1, 1, 1, 2, 3, 1},
        {false, true, true, false, false, true}
    );
}

TEST(LfuCacheTest, EqualFrequenciesUseLruOrder)
{
    caches::LfuCache<TestPage> cache{2};

    check_requests(
        cache,
        {1, 2, 1, 2, 3, 2, 1},
        {false, false, true, true, false, true, false}
    );
}

TEST(LfuCacheTest, ReloadedPageStartsWithFrequencyOne)
{
    caches::LfuCache<TestPage> cache{2};

    check_requests(
        cache,
        {1, 1, 2, 2, 2, 3, 1, 4, 2, 1},
        {false, true, false, true, true,
         false, false, false, true, false}
    );
}

// LIRS --------------------------------------------------------------------
TEST(LirsCacheTest, CapacityOneReplacesPreviousPage)
{
    caches::LirsCache<TestPage> cache{1};

    check_requests(
        cache,
        {1, 1, 2, 2, 1},
        {false, true, false, true, false}
    );

    EXPECT_EQ(cache.size(), 1u);
}
TEST(LirsCacheTest, ResidentHirInStackBecomesLir)
{
    caches::LirsCache<TestPage> cache{2};

    check_requests(
        cache,
        {1, 2, 2, 3, 2, 1},
        {false, false, true, false, true, false}
    );
}

TEST(LirsCacheTest, NonResidentHirRequiresLoading)
{
    caches::LirsCache<TestPage> cache{2};

    check_requests(
        cache,
        {1, 2, 3, 2, 4, 2},
        {false, false, false, false, false, true}
    );
}


// ideal cache --------------------------------------------------------------------
TEST(IdealCacheTest, EvictsPageNeededFarthestInFuture) {
    const std::vector<caches::Key> requests{1, 2, 3, 1, 2, 3};
    caches::IdealCache<TestPage> cache{2, requests};

    check_requests(cache,
                   requests,
                   {false, false, false, true, false, true});
}

