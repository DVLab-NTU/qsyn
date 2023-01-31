/****************************************************************************
  FileName     [ cmdCharDef.h ]
  PackageName  [ cmd ]
  Synopsis     [ enum for keyboard mapping ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef CMD_CHAR_DEF_H
#define CMD_CHAR_DEF_H

#include <climits>

enum ParseChar {
    // Simple keys: one code for one key press
    // -- The following should be platform-independent
    LINE_BEGIN_KEY = 1,       // ctrl-a
    LINE_END_KEY = 5,         // ctrl-e
    INPUT_END_KEY = 4,        // ctrl-d
    TAB_KEY = int('\t'),      // tab('\t') or Ctrl-i
    NEWLINE_KEY = int('\n'),  // enter('\n') or ctrl-m
    ESC_KEY = 27,             // Not printable; used for combo keys

    // -- The following simple/combo keys are platform-dependent
    //    You should test to check the returned codes of these key presses
    // -- Use "testAsc.cpp" to test
    //
    // [FLAG bit for combo keys]
    // -- Added to the returned ParseChar of combo keys
    // -- i.e. The returned ParseChar will be "ComboKeyEnum + FLAG bit"
    // -- This is to avoid the collision with the ASCII codes of regular keys
    // -- Feel free to add/remove/modify on your own
    //
    // [Intermediate keys for combo keys]
    // -- Intermediate keys are the common parts of combo keys
    //
    BACK_SPACE_KEY = 127,

    //
    // -- Arrow keys: 27 -> 91 -> {UP=65, DOWN=66, RIGHT=67, LEFT=68}
    ARROW_KEY_FLAG = 1 << 8,
    ARROW_KEY_INT = 91,
    ARROW_UP_KEY = 65 + ARROW_KEY_FLAG,
    ARROW_DOWN_KEY = 66 + ARROW_KEY_FLAG,
    ARROW_RIGHT_KEY = 67 + ARROW_KEY_FLAG,
    ARROW_LEFT_KEY = 68 + ARROW_KEY_FLAG,
    ARROW_KEY_BEGIN = ARROW_UP_KEY,
    ARROW_KEY_END = ARROW_LEFT_KEY,

    //
    // -- MOD keys: 27 -> 91 -> {49-54} -> 126
    //    MOD_KEY = { INSERT, DELETE, HOME, END, PgUp, PgDown }
    //
    MOD_KEY_FLAG = 1 << 9,
    MOD_KEY_INT = 91,
    HOME_KEY = 49 + MOD_KEY_FLAG,
    INSERT_KEY = 50 + MOD_KEY_FLAG,
    DELETE_KEY = 51 + MOD_KEY_FLAG,
    END_KEY = 52 + MOD_KEY_FLAG,
    PG_UP_KEY = 53 + MOD_KEY_FLAG,
    PG_DOWN_KEY = 54 + MOD_KEY_FLAG,
    MOD_KEY_BEGIN = HOME_KEY,
    MOD_KEY_END = PG_DOWN_KEY,
    MOD_KEY_DUMMY = 126,

    //
    // [For undefined keys]
    UNDEFINED_KEY = INT_MAX,

    // For output only, you don't need to modify this part
    BEEP_CHAR = 7,
    BACK_SPACE_CHAR = 8,

    // dummy end
    PARSE_CHAR_END
};

#endif  // CMD_CHAR_DEF_H
