/****************************************************************************
  FileName     [ cliCharDef.hpp ]
  PackageName  [ cli ]
  Synopsis     [ enum for keyboard mapping ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <climits>
#include <type_traits>

namespace KeyCode {
// Simple keys: one code for one key press
// -- The following should be platform-independent
constexpr int LINE_BEGIN_KEY = 1;       // ctrl-a
constexpr int LINE_END_KEY = 5;         // ctrl-e
constexpr int INTERRUPT_KEY = 3;        // ctrl-d
constexpr int INPUT_END_KEY = 4;        // ctrl-d
constexpr int TAB_KEY = int('\t');      // tab('\t') or Ctrl-i
constexpr int NEWLINE_KEY = int('\n');  // enter('\n') or ctrl-m
constexpr int CLEAR_CONSOLE_KEY = 12;   // ctrl-l
constexpr int ESC_KEY = 27;             // Not printable; used for combo keys

// -- The following simple/combo keys are platform-dependent
//    You should test to check the returned codes of these key presses
// -- Use "testAsc.cpp" to test
//
// [FLAG bit for combo keys]
// -- Added to the returned KeyCode of combo keys
// -- i.e. The returned KeyCode will be "ComboKeyEnum + FLAG bit"
// -- This is to avoid the collision with the ASCII codes of regular keys
// -- Feel free to add/remove/modify on your own
//
// [Intermediate keys for combo keys]
// -- Intermediate keys are the common parts of combo keys
//
constexpr int BACK_SPACE_KEY = 127;

//
// -- Arrow keys: 27 -> 91 -> {UP=65, DOWN=66, RIGHT=67, LEFT=68}
constexpr int ARROW_KEY_FLAG = 1 << 8;
constexpr int ARROW_KEY_INT = 91;
constexpr int ARROW_UP_KEY = 65 + ARROW_KEY_FLAG;
constexpr int ARROW_DOWN_KEY = 66 + ARROW_KEY_FLAG;
constexpr int ARROW_RIGHT_KEY = 67 + ARROW_KEY_FLAG;
constexpr int ARROW_LEFT_KEY = 68 + ARROW_KEY_FLAG;
constexpr int ARROW_KEY_BEGIN = ARROW_UP_KEY;
constexpr int ARROW_KEY_END = ARROW_LEFT_KEY;

//
// -- MOD keys: 27 -> 91 -> {49-54} -> 126
//    MOD_KEY = { INSERT, DELETE, HOME, END, PgUp, PgDown }
//
constexpr int MOD_KEY_FLAG = 1 << 9;
constexpr int MOD_KEY_INT = 91;
constexpr int HOME_KEY = 49 + MOD_KEY_FLAG;
constexpr int INSERT_KEY = 50 + MOD_KEY_FLAG;
constexpr int DELETE_KEY = 51 + MOD_KEY_FLAG;
constexpr int END_KEY = 52 + MOD_KEY_FLAG;
constexpr int PG_UP_KEY = 53 + MOD_KEY_FLAG;
constexpr int PG_DOWN_KEY = 54 + MOD_KEY_FLAG;
constexpr int MOD_KEY_BEGIN = HOME_KEY;
constexpr int MOD_KEY_END = PG_DOWN_KEY;
constexpr int MOD_KEY_DUMMY = 126;

//
// [For undefined keys]
constexpr int UNDEFINED_KEY = INT_MAX;

// For output only, you don't need to modify this part
constexpr int BEEP_CHAR = 7;
constexpr int BACK_SPACE_CHAR = 8;
};  // namespace KeyCode
