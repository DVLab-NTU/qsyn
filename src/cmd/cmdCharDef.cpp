/****************************************************************************
  FileName     [ cmdCharDef.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Process keyboard inputs ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/
#include <iostream>
#include <iomanip>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <cassert>
#include "cmdParser.h"

using namespace std;

//----------------------------------------------------------------------
//    Global static funcitons
//----------------------------------------------------------------------
static struct termios stored_settings;

static void reset_keypress(void)
{
   tcsetattr(0,TCSANOW,&stored_settings);
}

static void set_keypress(void)
{
   struct termios new_settings;
   tcgetattr(0,&stored_settings);
   new_settings = stored_settings;
   new_settings.c_lflag &= (~ICANON);
   new_settings.c_lflag &= (~ECHO);
   new_settings.c_cc[VTIME] = 0;
   tcgetattr(0,&stored_settings);
   new_settings.c_cc[VMIN] = 1;
   tcsetattr(0,TCSANOW,&new_settings);
}

//----------------------------------------------------------------------
//    Global funcitons
//----------------------------------------------------------------------
static char mygetc(istream& istr)
{
   char ch;
   set_keypress();
   istr.unsetf(ios_base::skipws);
   istr >> ch;
   istr.setf(ios_base::skipws);
   reset_keypress();
   #ifdef TEST_ASC
   cout << left << setw(6) << int(ch);
   #endif // TEST_ASC
   return ch;
}

void mybeep()
{
   cout << char(BEEP_CHAR);
}

inline static ParseChar returnCh(int);

#ifndef TA_KB_SETTING
// For HW 3, you don't need to customize this part!!!
//
// Modify "cmdCharDef.h" to work with the following code
//
// Make sure you DO NOT define TA_KB_SETTING in your Makefile
//
ParseChar
CmdParser::getChar(istream& istr) const
{
   char ch = mygetc(istr);

   if (istr.eof())
      return returnCh(INPUT_END_KEY);
   switch (ch) {
      // Simple keys: one code for one key press
      // -- The following should be platform-independent
      case LINE_BEGIN_KEY:  // Ctrl-a
      case LINE_END_KEY:    // Ctrl-e
      case INPUT_END_KEY:   // Ctrl-d
      case TAB_KEY:         // tab('\t') or Ctrl-i
      case NEWLINE_KEY:     // enter('\n') or ctrl-m
         return returnCh(ch);

      // TODO... Check and change only if you want to use your own
      // keyboard mapping for special keys.
      // -- The following simple/combo keys are platform-dependent
      //    You should test to check the returned codes of these key presses
      // -- You should either modify the "enum ParseChar" definitions in
      //    "cmdCharDef.h", or revise the control flow of the "case ESC" below
      //
      // -- You need to handle:
      //    { BACK_SPACE_KEY, ARROW_UP/DOWN/RIGHT/LEFT,
      //      HOME/END/PG_UP/PG_DOWN/INSERT/DELETE }
      //
      // Combo keys: multiple codes for one key press
      // -- Usually starts with ESC key, so we check the "case ESC"
      // case ESC_KEY:

      // For the remaining printable and undefined keys
      default:
         if (isprint(ch)) return returnCh(ch);
         else return returnCh(UNDEFINED_KEY);
   }

   return returnCh(UNDEFINED_KEY);
}
#else // TA_KB_SETTING is defined
// DO NOT CHANGE THIS PART...
// DO NOT CHANGE THIS PART...
//
// This part will be used by TA to test your program.
//
// TA will use "make -DTA_KB_SETTING" to test your program
//
ParseChar
CmdParser::getChar(istream& istr) const
{
   char ch = mygetc(istr);

   if (istr.eof())
      return returnCh(INPUT_END_KEY);
   switch (ch) {
      // Simple keys: one code for one key press
      // -- The following should be platform-independent
      case LINE_BEGIN_KEY:  // Ctrl-a
      case LINE_END_KEY:    // Ctrl-e
      case INPUT_END_KEY:   // Ctrl-d
      case TAB_KEY:         // tab('\t') or Ctrl-i
      case NEWLINE_KEY:     // enter('\n') or ctrl-m
         return returnCh(ch);

      // -- The following simple/combo keys are platform-dependent
      //    You should test to check the returned codes of these key presses
      // -- You should either modify the "enum ParseChar" definitions in
      //    "cmdCharDef.h", or revise the control flow of the "case ESC" below
      case BACK_SPACE_KEY:
         return returnCh(ch);

      // Combo keys: multiple codes for one key press
      // -- Usually starts with ESC key, so we check the "case ESC"
      case ESC_KEY: {
         char combo = mygetc(istr);
         // Note: ARROW_KEY_INT == MOD_KEY_INT, so we only check MOD_KEY_INT
         if (combo == char(MOD_KEY_INT)) {
            char key = mygetc(istr);
            if ((key >= char(MOD_KEY_BEGIN)) && (key <= char(MOD_KEY_END))) {
               if (mygetc(istr) == MOD_KEY_DUMMY)
                  return returnCh(int(key) + MOD_KEY_FLAG);
               else return returnCh(UNDEFINED_KEY);
            }
            else if ((key >= char(ARROW_KEY_BEGIN)) &&
                     (key <= char(ARROW_KEY_END)))
               return returnCh(int(key) + ARROW_KEY_FLAG);
            else return returnCh(UNDEFINED_KEY);
         }
         else { mybeep(); return getChar(istr); }
      }
      // For the remaining printable and undefined keys
      default:
         if (isprint(ch)) return returnCh(ch);
         else return returnCh(UNDEFINED_KEY);
   }

   return returnCh(UNDEFINED_KEY);
}
#endif // TA_KB_SETTING

inline static ParseChar
returnCh(int ch)
{
#ifndef MAKE_REF
   return ParseChar(ch);
#else
   switch (ParseChar(ch)) {
      case LINE_BEGIN_KEY : return ParseChar(TA_LINE_BEGIN_KEY);
      case LINE_END_KEY   : return ParseChar(TA_LINE_END_KEY);
      case INPUT_END_KEY  : return ParseChar(TA_INPUT_END_KEY);
      case TAB_KEY        : return ParseChar(TA_TAB_KEY);
      case NEWLINE_KEY    : return ParseChar(TA_NEWLINE_KEY);
      case ESC_KEY        : return ParseChar(TA_ESC_KEY);
      case BACK_SPACE_KEY : return ParseChar(TA_BACK_SPACE_KEY);
      case ARROW_KEY_FLAG : return ParseChar(TA_ARROW_KEY_FLAG);
      case ARROW_KEY_INT  : return ParseChar(TA_ARROW_KEY_INT);
      case ARROW_UP_KEY   : return ParseChar(TA_ARROW_UP_KEY);
      case ARROW_DOWN_KEY : return ParseChar(TA_ARROW_DOWN_KEY);
      case ARROW_RIGHT_KEY: return ParseChar(TA_ARROW_RIGHT_KEY);
      case ARROW_LEFT_KEY : return ParseChar(TA_ARROW_LEFT_KEY);
//      case ARROW_KEY_BEGIN: return ParseChar(TA_ARROW_KEY_BEGIN);
//      case ARROW_KEY_END  : return ParseChar(TA_ARROW_KEY_END);
      case MOD_KEY_FLAG   : return ParseChar(TA_MOD_KEY_FLAG);
// comment out because MOD_KEY_INT == ARROW_KEY_INT
// Uncomment it out if yours are different
//      case MOD_KEY_INT    : return ParseChar(TA_MOD_KEY_INT);
      case HOME_KEY       : return ParseChar(TA_HOME_KEY);
      case INSERT_KEY     : return ParseChar(TA_INSERT_KEY);
      case DELETE_KEY     : return ParseChar(TA_DELETE_KEY);
      case END_KEY        : return ParseChar(TA_END_KEY);
      case PG_UP_KEY      : return ParseChar(TA_PG_UP_KEY);
      case PG_DOWN_KEY    : return ParseChar(TA_PG_DOWN_KEY);
//      case MOD_KEY_BEGIN  : return ParseChar(TA_MOD_KEY_BEGIN);
//      case MOD_KEY_END    : return ParseChar(TA_MOD_KEY_END);
      case MOD_KEY_DUMMY  : return ParseChar(TA_MOD_KEY_DUMMY);
      case UNDEFINED_KEY  : return ParseChar(TA_UNDEFINED_KEY);
      case BEEP_CHAR      : return ParseChar(TA_BEEP_CHAR);
      case BACK_SPACE_CHAR: return ParseChar(TA_BACK_SPACE_CHAR);
      default             : return ParseChar(ch);
   }
#endif
}

