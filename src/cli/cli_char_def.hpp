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
using key_code_type = int;
// Simple keys: one code for one key press
// -- The following should be platform-independent
constexpr key_code_type line_begin_key{1};       // ctrl-a
constexpr key_code_type line_end_key{5};         // ctrl-e
constexpr key_code_type interrupt_key{3};        // ctrl-d
constexpr key_code_type input_end_key{4};        // ctrl-d
constexpr key_code_type tab_key{'\t'};           // tab('\t') or Ctrl-i
constexpr key_code_type newline_key{'\n'};       // enter('\n') or ctrl-m
constexpr key_code_type clear_terminal_key{12};  // ctrl-l
constexpr key_code_type esc_key{27};             // Not printable; used for combo keys

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
constexpr key_code_type back_space_key{127};

//
// -- Arrow keys: 27 -> 91 -> {UP=65, DOWN=66, RIGHT=67, LEFT=68}
constexpr key_code_type arrow_key_flag{1 << 8};
constexpr key_code_type arrow_key_int{91};
constexpr key_code_type arrow_up_key{65 + arrow_key_flag};
constexpr key_code_type arrow_down_key{66 + arrow_key_flag};
constexpr key_code_type arrow_right_key{67 + arrow_key_flag};
constexpr key_code_type arrow_left_key = 68 + arrow_key_flag;
constexpr key_code_type arrow_key_begin{arrow_up_key};
constexpr key_code_type arrow_key_end{arrow_left_key};

// -- CTRL keys: 27 -> 91 -> {49-54} -> 126
//    CTRL_KEY = { INSERT, DELETE, HOME, END, PgUp, PgDown }

constexpr key_code_type ctrl_key_flag{1 << 9};
constexpr key_code_type ctrl_key_int = arrow_key_int;
constexpr key_code_type home_key{49 + ctrl_key_flag};
constexpr key_code_type insert_key{50 + ctrl_key_flag};
constexpr key_code_type delete_key{51 + ctrl_key_flag};
constexpr key_code_type end_key{52 + ctrl_key_flag};
constexpr key_code_type pg_up_key{53 + ctrl_key_flag};
constexpr key_code_type pg_down_key{54 + ctrl_key_flag};
constexpr key_code_type ctrl_key_begin{home_key};
constexpr key_code_type ctrl_key_end{pg_down_key};
constexpr key_code_type ctrl_key_dummy{126};

constexpr key_code_type alt_key_flag{1 << 10};
constexpr key_code_type alt_key_int = arrow_key_int;
constexpr key_code_type prev_word_key{98 + alt_key_flag};
constexpr key_code_type next_word_key{102 + alt_key_flag};
constexpr key_code_type alt_key_begin{prev_word_key};
constexpr key_code_type alt_key_end{next_word_key};
constexpr key_code_type alt_key_dummy{126};

// [For undefined keys]
constexpr key_code_type undefined_key{INT_MAX};

// For output only, you don't need to modify this part
constexpr key_code_type beep_char{7};
constexpr key_code_type back_space_char{8};
};  // namespace key_code

}  // namespace dvlab
