/****************************************************************************
  FileName     [ cmdCharDef.h ]
  PackageName  [ cmd ]
  Synopsis     [ enum for keyboard mapping ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef CMD_CHAR_DEF_H
#define CMD_CHAR_DEF_H

#include <climits>

// TO define common TA settings
// Don't change these #defines
#define TA_LINE_BEGIN_KEY   1
#define TA_LINE_END_KEY     5
#define TA_INPUT_END_KEY    4
#define TA_TAB_KEY          int('\t')
#define TA_NEWLINE_KEY      int('\n')
#define TA_ESC_KEY          27
#define TA_BACK_SPACE_KEY   127
#define TA_ARROW_KEY_FLAG   (1 << 8)
#define TA_ARROW_KEY_INT    91
#define TA_ARROW_UP_KEY     (65 + TA_ARROW_KEY_FLAG)
#define TA_ARROW_DOWN_KEY   (66 + TA_ARROW_KEY_FLAG)
#define TA_ARROW_RIGHT_KEY  (67 + TA_ARROW_KEY_FLAG)
#define TA_ARROW_LEFT_KEY   (68 + TA_ARROW_KEY_FLAG)
#define TA_ARROW_KEY_BEGIN  TA_ARROW_UP_KEY
#define TA_ARROW_KEY_END    TA_ARROW_LEFT_KEY
#define TA_MOD_KEY_FLAG     (1 << 9)
#define TA_MOD_KEY_INT      91
#define TA_HOME_KEY         (49 + TA_MOD_KEY_FLAG)
#define TA_INSERT_KEY       (50 + TA_MOD_KEY_FLAG)
#define TA_DELETE_KEY       (51 + TA_MOD_KEY_FLAG)
#define TA_END_KEY          (52 + TA_MOD_KEY_FLAG)
#define TA_PG_UP_KEY        (53 + TA_MOD_KEY_FLAG)
#define TA_PG_DOWN_KEY      (54 + TA_MOD_KEY_FLAG)
#define TA_MOD_KEY_BEGIN    TA_HOME_KEY
#define TA_MOD_KEY_END      TA_PG_DOWN_KEY
#define TA_MOD_KEY_DUMMY    126
#define TA_UNDEFINED_KEY    INT_MAX
#define TA_BEEP_CHAR        7
#define TA_BACK_SPACE_CHAR  8


#ifndef TA_KB_SETTING
// For HW 3, this part is NOT used by default, so you don't need to modify it.
// Change this part only if you want to customize it for your own keyboard
// mapping. Remember to undefine "TA_KB_SETTING" in src/Makefile.in.
//
// Run "testAsc" to define your setting here...
//
// Make sure you DO NOT define TA_KB_SETTING in your Makefile
//
enum ParseChar
{
   // Simple keys: one code for one key press
   // -- The following should be platform-independent
   LINE_BEGIN_KEY   = 1,          // ctrl-a
   LINE_END_KEY     = 5,          // ctrl-e
   INPUT_END_KEY    = 4,          // ctrl-d
   TAB_KEY          = int('\t'),  // tab('\t') or Ctrl-i
   NEWLINE_KEY      = int('\n'),  // enter('\n') or ctrl-m
   ESC_KEY          = 27,         // Not printable; used for combo keys

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
   BACK_SPACE_KEY   = 127,
   
   //
   // -- Arrow keys: 27 -> 91 -> {UP=65, DOWN=66, RIGHT=67, LEFT=68}
   ARROW_KEY_FLAG   = 1 << 8,
   ARROW_KEY_INT    = 91,
   ARROW_UP_KEY     = 65 + ARROW_KEY_FLAG,
   ARROW_DOWN_KEY   = 66 + ARROW_KEY_FLAG,
   ARROW_RIGHT_KEY  = 67 + ARROW_KEY_FLAG,
   ARROW_LEFT_KEY   = 68 + ARROW_KEY_FLAG,
   ARROW_KEY_BEGIN  = ARROW_UP_KEY,
   ARROW_KEY_END    = ARROW_LEFT_KEY,

   //
   // -- MOD keys: 27 -> 91 -> {49-54} -> 126
   //    MOD_KEY = { INSERT, DELETE, HOME, END, PgUp, PgDown }
   //
   MOD_KEY_FLAG     = 1 << 9,
   MOD_KEY_INT      = 91,
   HOME_KEY         = 49 + MOD_KEY_FLAG,
   INSERT_KEY       = 50 + MOD_KEY_FLAG,
   DELETE_KEY       = 51 + MOD_KEY_FLAG,
   END_KEY          = 52 + MOD_KEY_FLAG,
   PG_UP_KEY        = 53 + MOD_KEY_FLAG,
   PG_DOWN_KEY      = 54 + MOD_KEY_FLAG,
   MOD_KEY_BEGIN    = HOME_KEY,
   MOD_KEY_END      = PG_DOWN_KEY,
   MOD_KEY_DUMMY    = 126,

   //
   // [For undefined keys]
   UNDEFINED_KEY  = INT_MAX,

   // For output only, you don't need to modify this part
   BEEP_CHAR        = 7,
   BACK_SPACE_CHAR  = 8,

   // dummy end
   PARSE_CHAR_END
};
#else  // TA_KB_SETTING is defined
// DO NOT CHANGE THIS PART...
// DO NOT CHANGE THIS PART...
//
// This part will be used by TA to test your program.
//
// TA will use "make -DTA_KB_SETTING" to test your program
//
enum ParseChar
{
   // Simple keys: one code for one key press
   // -- The following should be platform-independent
   LINE_BEGIN_KEY   = TA_LINE_BEGIN_KEY,  // ctrl-a
   LINE_END_KEY     = TA_LINE_END_KEY,    // ctrl-e
   INPUT_END_KEY    = TA_INPUT_END_KEY,   // ctrl-d
   TAB_KEY          = TA_TAB_KEY,         // tab('\t') or Ctrl-i
   NEWLINE_KEY      = TA_NEWLINE_KEY,     // enter('\n') or ctrl-m
   ESC_KEY          = TA_ESC_KEY,         // Not printable; used for combo keys

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
   BACK_SPACE_KEY   = TA_BACK_SPACE_KEY,
   
   //
   // -- Arrow keys: 27 -> 91 -> {UP=65, DOWN=66, RIGHT=67, LEFT=68}
   ARROW_KEY_FLAG   = TA_ARROW_KEY_FLAG,
   ARROW_KEY_INT    = TA_ARROW_KEY_INT,
   ARROW_UP_KEY     = TA_ARROW_UP_KEY,
   ARROW_DOWN_KEY   = TA_ARROW_DOWN_KEY,
   ARROW_RIGHT_KEY  = TA_ARROW_RIGHT_KEY,
   ARROW_LEFT_KEY   = TA_ARROW_LEFT_KEY,
   ARROW_KEY_BEGIN  = TA_ARROW_KEY_BEGIN,
   ARROW_KEY_END    = TA_ARROW_KEY_END,

   //
   // -- MOD keys: 27 -> 91 -> {49-54} -> 126
   //    MOD_KEY = { INSERT, DELETE, HOME, END, PgUp, PgDown }
   //
   MOD_KEY_FLAG     = TA_MOD_KEY_FLAG,
   MOD_KEY_INT      = TA_MOD_KEY_INT,
   HOME_KEY         = TA_HOME_KEY,
   INSERT_KEY       = TA_INSERT_KEY,
   DELETE_KEY       = TA_DELETE_KEY,
   END_KEY          = TA_END_KEY,
   PG_UP_KEY        = TA_PG_UP_KEY,
   PG_DOWN_KEY      = TA_PG_DOWN_KEY,
   MOD_KEY_BEGIN    = TA_MOD_KEY_BEGIN,
   MOD_KEY_END      = TA_MOD_KEY_END,
   MOD_KEY_DUMMY    = TA_MOD_KEY_DUMMY,

   //
   // [For undefined keys]
   UNDEFINED_KEY    = TA_UNDEFINED_KEY,

   // For output only, you don't need to modify this part
   BEEP_CHAR        = TA_BEEP_CHAR,
   BACK_SPACE_CHAR  = TA_BACK_SPACE_CHAR,

   // dummy end
   PARSE_CHAR_END
};
#endif // TA_KB_SETTING

#endif // CMD_CHAR_DEF_H
