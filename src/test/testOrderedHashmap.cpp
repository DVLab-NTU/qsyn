/****************************************************************************
  FileName     [ testTensor2.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for orderedHashmap ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include "ordered_hashmap.h"
#include "util.h"

using namespace std;
// --- Always put catch2/catch.hpp after all other header files ---
#include "catch2/catch.hpp"
// ----------------------------------------------------------------

TEST_CASE("omap_insert_and_erase", "[OMap]") {
    ordered_hashmap<int, int> omap{{1, 1}, {2, 2}, {3, 3}};

    omap.printMap();

    omap.insert({3, 4});

    omap.printMap();

    omap.insert({4, 4});
    omap.insert({5, 5});
    omap.insert({6, 6});
    REQUIRE(omap.at(4) == 4);
    REQUIRE(omap.at(5) == 5);
    REQUIRE(omap.at(6) == 6);

    omap.erase(4);
    omap.printMap();
    REQUIRE_THROWS_AS(omap.at(4), std::out_of_range);
    omap.erase(2);
    REQUIRE_THROWS_AS(omap.at(2), std::out_of_range);

    omap.printMap();
    omap.insert({2, 2});
    REQUIRE(omap.at(2) == 2);

    omap.printMap();

    omap.erase(4);

    omap.printMap();

    REQUIRE(omap.at(1) == 1);
    REQUIRE(omap.at(2) == 2);
    REQUIRE(omap.at(3) == 3);
    REQUIRE_THROWS_AS(omap.at(4), std::out_of_range);
    REQUIRE(omap.at(5) == 5);
    REQUIRE(omap.at(6) == 6);
    REQUIRE_THROWS_AS(omap.at(7), std::out_of_range);

    omap.erase(1);
    omap.erase(5);

    omap.printMap();
}

TEST_CASE("omap_copy", "[OMap]") {
    ordered_hashmap<int, int> omap1{{1, 1}, {2, 2}, {3, 3}};
    ordered_hashmap<int, int> omap2{{4, 4}, {2, 2}, {7, 7}};

    omap2 = omap1;

    REQUIRE(omap2.at(1) == omap1.at(1));
    REQUIRE(omap2.at(2) == omap1.at(2));
    REQUIRE(omap2.at(3) == omap1.at(3));
    REQUIRE_THROWS_AS(omap2.at(4), out_of_range);
    REQUIRE_THROWS_AS(omap2.at(7), out_of_range);
}

TEST_CASE("omap_modify", "[OMap]") {
    ordered_hashmap<int, int> omap{{1, 1}, {2, 2}, {3, 3}};

    omap.at(1) = 2;
    REQUIRE(omap.at(1) == 2);
    REQUIRE_THROWS_AS(omap.at(4) = 4, out_of_range);
    omap[4] = 4;
    REQUIRE(omap.at(4) == 4);
    omap[4] = 0;
    REQUIRE(omap.at(4) == 0);

}

TEST_CASE("omap_iterator", "[OMap]") {
    ordered_hashmap<int, int> omap{{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};

    omap.erase(2);
    omap.erase(5);
    vector<int> ref;
    for (auto& [k, v] : omap) {
        ref.push_back(k);
        ref.push_back(v);
    }
    REQUIRE(ref.size() == 6);
    REQUIRE(ref[0] == 1);
    REQUIRE(ref[1] == 1);
    REQUIRE(ref[2] == 3);
    REQUIRE(ref[3] == 3);
    REQUIRE(ref[4] == 4);
    REQUIRE(ref[5] == 4);

    ref.clear();
    omap.insert({6, 6});
    for (auto itr = omap.find(3); itr != omap.end(); ++itr) {
        ref.push_back(itr->first);
        ref.push_back(itr->second);
    }
    REQUIRE(ref[0] == 3);
    REQUIRE(ref[1] == 3);
    REQUIRE(ref[2] == 4);
    REQUIRE(ref[3] == 4);
    REQUIRE(ref[4] == 6);
    REQUIRE(ref[5] == 6);

}

// TEST_CASE("omap_playground", "[OMap]") {
//     ordered_hashmap<int, int> omap;
//     char op;
//     int key, val;

//     cout << "> ";
//     while (cin >> op) {
//         if (op == 'a') {
//             cin >> key >> val;
//             omap.emplace(key, val);
//         } else if (op == 'r') {
//             cin >> key;
//             omap.erase(key);
//         } else if (op == 'p') {
//             omap.printMap();
//         } else if (op == 'f') {
//             cin >> key;
//             try {
//                 cout << key << " : " << omap.at(key) << endl;
//             } catch (std::out_of_range& e) {
//                 cout << "No match" << endl;
//             }
//         } else if (op == 'i') {
//             for (auto& [k, v] : omap) {
//                 cout << k << " : " << v << endl;
//             }
//         }

//         cout << "> ";
//     }
// }
