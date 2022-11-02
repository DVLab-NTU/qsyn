/****************************************************************************
  FileName     [ testTensor2.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for orderedHashmap ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include "orderedHashmap.h"
#include "util.h"

using namespace std;
// --- Always put catch2/catch.hpp after all other header files ---
#include "catch2/catch.hpp"
// ----------------------------------------------------------------

TEST_CASE("omap", "[OMap]") {
    OrderedHashmap<int, int> omap{{1, 1}, {2, 2}, {3, 3}};

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
    // REQUIRE_THROWS_AS(omap.at(4), std::out_of_range);
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

}


