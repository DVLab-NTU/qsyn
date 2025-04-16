#include "common/global.hpp"

#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_test_macros.hpp>

std::mt19937& rand_gen() {
    static std::mt19937 rng{Catch::getSeed()};
    return rng;
}

bool stop_requested() { return false; }

dvlab::Phase get_random_phase() {
    auto const num = std::uniform_int_distribution<>(0, 127)(rand_gen());
    auto const den = std::uniform_int_distribution<>(1, 64)(rand_gen());

    return dvlab::Phase(num, den);
}

bool coin_flip(float prob) {
    return std::bernoulli_distribution(prob)(rand_gen());
}
