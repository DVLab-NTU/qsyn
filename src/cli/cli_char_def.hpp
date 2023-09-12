/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ keycodes for keyboard mapping ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <climits>
#include <type_traits>

namespace dvlab {

namespace key_code {
// Simple keys: one code for one key press
// -- The following should be platform-independent
constexpr int line_begin_key = 1;       // ctrl-a
constexpr int line_end_key = 5;         // ctrl-e
constexpr int interrupt_key = 3;        // ctrl-d
constexpr int input_end_key = 4;        // ctrl-d
constexpr int tab_key = int('\t');      // tab('\t') or Ctrl-i
constexpr int newline_key = int('\n');  // enter('\n') or ctrl-m
constexpr int clear_terminal_key = 12;  // ctrl-l
constexpr int esc_key = 27;             // Not printable; used for combo keys

// -- The following simple/combo keys are platform-dependent
//    You should test to check the returned codes of these key presses
// -- Use "testAsc.cpp" to test
//
// [FLAG bit for combo keys]
// -- Added to the returned key code of combo keys
// -- i.e. The returned key code will be "ComboKeyEnum + FLAG bit"
// -- This is to avoid the collision with the ASCII codes of regular keys
// -- Feel free to add/remove/modify on your own
//
// [Intermediate keys for combo keys]
// -- Intermediate keys are the common parts of combo keys
//
constexpr int back_space_key = 127;

//
// -- Arrow keys: 27 -> 91 -> {UP=65, DOWN=66, RIGHT=67, LEFT=68}
constexpr int arrow_key_flag = 1 << 8;
constexpr int arrow_key_int = 91;
constexpr int arrow_up_key = 65 + arrow_key_flag;
constexpr int arrow_down_key = 66 + arrow_key_flag;
constexpr int arrow_right_key = 67 + arrow_key_flag;
constexpr int arrow_left_key = 68 + arrow_key_flag;
constexpr int arrow_key_begin = arrow_up_key;
constexpr int arrow_key_end = arrow_left_key;

//
// -- MOD keys: 27 -> 91 -> {49-54} -> 126
//    MOD_KEY = { INSERT, DELETE, HOME, END, PgUp, PgDown }
//
constexpr int mod_key_flag = 1 << 9;
constexpr int mod_key_int = 91;
constexpr int home_key = 49 + mod_key_flag;
constexpr int insert_key = 50 + mod_key_flag;
constexpr int delete_key = 51 + mod_key_flag;
constexpr int end_key = 52 + mod_key_flag;
constexpr int pg_up_key = 53 + mod_key_flag;
constexpr int pg_down_key = 54 + mod_key_flag;
constexpr int mod_key_begin = home_key;
constexpr int mod_key_end = pg_down_key;
constexpr int mod_key_dummy = 126;

//
// [For undefined keys]
constexpr int undefined_key = INT_MAX;

// For output only, you don't need to modify this part
constexpr int beep_char = 7;
constexpr int back_space_char = 8;
};  // namespace key_code

}  // namespace dvlab