#pragma once

bool stop_requested();

#include <algorithm>
#include <random>
#include <vector>

#include "util/phase.hpp"

std::mt19937& rand_gen();

template <typename T>
std::vector<T> get_shuffle_seq(std::vector<T> vec) {
    std::ranges::shuffle(vec, rand_gen());
    return vec;
}

dvlab::Phase get_random_phase();

bool coin_flip(float prob = 0.5);
