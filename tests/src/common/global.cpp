#include "common/global.hpp"

#include <random>

namespace {
std::random_device RD;
}

std::mt19937 RAND_GEN{RD()};

bool stop_requested() { return false; }

dvlab::Phase get_random_phase() {
    auto const num = std::uniform_int_distribution<>(0, 127)(RAND_GEN);
    auto const den = std::uniform_int_distribution<>(1, 64)(RAND_GEN);

    return dvlab::Phase(num, den);
}

bool coin_flip(float prob) {
    return std::bernoulli_distribution(prob)(RAND_GEN);
}
