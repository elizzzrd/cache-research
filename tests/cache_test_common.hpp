#pragma once

#include "cache_types.hpp"

#include <gtest/gtest.h>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace cache_tests {

    struct TestPage {
        const int load_id_;
        std::string payload;
        std::vector<int> data_;

        explicit TestPage(int number)
            : load_id_(number),
            payload("loaded-page-" + std::to_string(number)),
            data_{number, number + 100} {}
    };

    template <typename KeyT = caches::Key>
    struct PageLoader {
        int calls = 0;

        TestPage operator()(const KeyT&) {
            return TestPage{++calls};
        }
    };

    inline void expect_page(const TestPage& page, int number) {
        EXPECT_EQ(page.load_id_, number);
        EXPECT_EQ(page.payload, "loaded-page-" + std::to_string(number));
        EXPECT_EQ(page.data_, (std::vector<int>{number, number + 100}));
    }

    template <typename Cache, typename KeyT = caches::Key>
    void check_requests(Cache& cache, const std::vector<KeyT>& requests,
                        const std::vector<bool>& expected_hits) 
    {
        ASSERT_EQ(requests.size(), expected_hits.size());
        ASSERT_EQ(cache.size(), 0u);

        PageLoader<KeyT> source;
        std::unordered_map<KeyT, int> expected_ids;
        int expected_calls = 0;

        for (std::size_t position = 0; position < requests.size(); ++position) {
            SCOPED_TRACE(position);
            const KeyT& key = requests[position];
            if (!expected_hits[position]) {
                expected_ids[key] = ++expected_calls;
            }
            const auto expected = expected_ids.find(key);
            ASSERT_NE(expected, expected_ids.end());

            const auto result = cache.lookup_update(key, source);
            EXPECT_EQ(result.hit, expected_hits[position]);
            expect_page(result.value, expected->second);
            EXPECT_EQ(source.calls, expected_calls);
            EXPECT_LE(cache.size(), cache.capacity());
        }
    }

} 
