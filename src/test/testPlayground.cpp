// #include <vector>
// #include <iostream>
// #include <tuple>
// #include <optional>

// using namespace std;
// // --- Always put catch2/catch.hpp after all other header files ---
// #include "catch2/catch.hpp"
// // ----------------------------------------------------------------
// TEST_CASE("Vec const", "[Vec]") {
//     // optional<pair<const int, int>> tmp;
//     vector<optional<pair<const int, int>>> a, b;
//     a.push_back(make_pair(1, 1));
//     a.push_back(make_pair(2, 2));
//     a.push_back(make_pair(3, 3));
//     b.push_back(make_pair(4, 4));
//     b.push_back(make_pair(5, 5));
//     b.push_back(make_pair(6, 6));
//     // tmp = make_pair(7, 7);
//     // b.push_back(tmp);
//     a.swap(b);

//     for (auto i : a) cout << "("<< i.value().first << ", " << i.value().second << "), ";
//     cout << endl;
//     for (auto i : b) cout << "("<< i.value().first << ", " << i.value().second << "), ";
//     cout << endl;
// }