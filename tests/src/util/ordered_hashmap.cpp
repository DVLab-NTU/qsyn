#include "util/ordered_hashmap.hpp"

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

TEST_CASE("ordered_hashmap iterator constness", "[ordered_hashmap]") {
    using Map = dvlab::utils::ordered_hashmap<int, int>;
    static_assert(std::is_same_v<std::ranges::range_reference_t<Map>, Map::value_type&>);
    static_assert(std::is_same_v<std::ranges::range_reference_t<Map const>, Map::value_type const&>);

    Map values{{1, 2}, {3, 4}};
    auto const& read_only = values;
    static_assert(std::is_same_v<decltype((read_only.begin()->second)), int const&>);

    values.begin()->second = 5;
    REQUIRE(read_only.begin()->second == 5);
    REQUIRE(read_only.cbegin()->first == 1);

    values.erase(1);
    REQUIRE(read_only.begin()->first == 3);
    REQUIRE(read_only.begin()->second == 4);
}
