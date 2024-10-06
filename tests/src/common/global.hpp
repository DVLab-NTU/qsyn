#pragma once

bool stop_requested();

#include <algorithm>
#include <random>
#include <vector>

#include "util/phase.hpp"

extern std::mt19937 RAND_GEN;

template <typename T>
std::vector<T> get_shuffle_seq(std::vector<T> vec) {
    std::shuffle(vec.begin(), vec.end(), RAND_GEN);
    return vec;
}

dvlab::Phase get_random_phase();

bool coin_flip(float prob = 0.5);
