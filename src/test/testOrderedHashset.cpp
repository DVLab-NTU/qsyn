/****************************************************************************
  FileName     [ testTensor2.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for orderedHashmap ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include "ordered_hashset.h"
#include "util.h"

using namespace std;
// --- Always put catch2/catch.hpp after all other header files ---
#include "catch2/catch.hpp"
// ----------------------------------------------------------------

TEST_CASE("oset_insert_and_erase", "[OSet]") {
    ordered_hashset<int> oset{1, 2, 3};

    oset.printSet();

    REQUIRE(oset.insert(3).second == false);

    oset.printSet();

    oset.insert(4);
    oset.insert(5);
    oset.insert(6);
    REQUIRE(oset.contains(4));
    REQUIRE(oset.contains(5));
    REQUIRE(oset.contains(6));

    REQUIRE(oset.erase(4) == 1);
    oset.printSet();
    REQUIRE(!oset.contains(4));
    REQUIRE(oset.erase(2) == 1);
    REQUIRE(!oset.contains(2));

    oset.printSet();
    REQUIRE(oset.insert(2).second == true);
    REQUIRE(oset.contains(2));
    
    REQUIRE(oset.erase(4) == 0);

    oset.erase(1);
    oset.erase(5);

    oset.printSet();
}

TEST_CASE("oset_copy", "[OSet]") {
    ordered_hashset<int> oset1{1, 2, 3};
    ordered_hashset<int> oset2{4, 2, 7};

    oset2 = oset1;

    REQUIRE(oset2.contains(1));
    REQUIRE(oset2.contains(2));
    REQUIRE(oset2.contains(3));
    REQUIRE(!oset2.contains(4));
    REQUIRE(!oset2.contains(7));
}

TEST_CASE("oset_iterator", "[OSet]") {
    ordered_hashset<int> oset{1, 2, 3, 4, 5};

    oset.erase(2);
    oset.erase(5);
    vector<int> ref;
    for (auto& item : oset) {
        ref.push_back(item);
    }
    REQUIRE(ref.size() == 3);
    REQUIRE(ref[0] == 1);
    REQUIRE(ref[1] == 3);
    REQUIRE(ref[2] == 4);

    ref.clear();
    oset.insert(6);
    for (auto itr = oset.find(3); itr != oset.end(); ++itr) {
        ref.push_back(*itr);
    }
    REQUIRE(ref[0] == 3);
    REQUIRE(ref[1] == 4);
    REQUIRE(ref[2] == 6);

}

// TEST_CASE("oset_playground", "[OSet]") {
//     ordered_hashset<int> oset;
//     char op;
//     int key;

//     cout << "> ";
//     while (cin >> op) {
//         if (op == 'a') {
//             cin >> key;
//             oset.emplace(key);
//         } else if (op == 'r') {
//             cin >> key;
//             oset.erase(key);
//         } else if (op == 'p') {
//             oset.printSet();
//         } else if (op == 'f') {
//             cin >> key;
//             if (oset.contains(key)) {
//                 cout << key << endl;
//             } else {
//                 cout << "No match" << endl;
//             }
//         } else if (op == 'i') {
//             for (auto& k : oset) {
//                 cout << k << endl;
//             }
//         }

//         cout << "> ";
//     }
// }
