/****************************************************************************
  FileName     [ cmdCharDef.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Process keyboard inputs ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "cmdCharDef.h"  // for ParseChar, ParseChar::UNDEFINED_KEY, ParseCh...

#include <ctype.h>    // for isprint
#include <termios.h>  // for termios, tcgetattr, tcsetattr, TCSANOW, cc_t

#include <iostream>  // for istream, cout

#include "cmdParser.h"  // for CmdParser

using namespace std;

void clearConsole();

//----------------------------------------------------------------------
//    Global static funcitons
//----------------------------------------------------------------------
static struct termios stored_settings;

static void reset_keypress(void) {
    tcsetattr(0, TCSANOW, &stored_settings);
}

static void set_keypress(void) {
    struct termios new_settings;
    tcgetattr(0, &stored_settings);
    new_settings = stored_settings;
    new_settings.c_lflag &= (~ICANON);
    new_settings.c_lflag &= (~ECHO);
    new_settings.c_cc[VTIME] = 0;
    tcgetattr(0, &stored_settings);
    new_settings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new_settings);
}

//----------------------------------------------------------------------
//    Global funcitons
//----------------------------------------------------------------------
static char mygetc(istream& istr) {
    char ch;
    set_keypress();
    istr.unsetf(ios_base::skipws);
    istr >> ch;
    istr.setf(ios_base::skipws);
    reset_keypress();
#ifdef TEST_ASC
    cout << left << setw(6) << int(ch);
#endif  // TEST_ASC
    return ch;
}

void mybeep() {
    cout << char(BEEP_CHAR);
}

inline static ParseChar returnCh(int);

ParseChar
CmdParser::getChar(istream& istr) const {
    char ch = mygetc(istr);

    if (istr.eof())
        return returnCh(INTERRUPT_KEY);
    switch (ch) {
        // Simple keys: one code for one key press
        // -- The following should be platform-independent
        case LINE_BEGIN_KEY:     // Ctrl-a
        case LINE_END_KEY:       // Ctrl-e
        case INPUT_END_KEY:      // Ctrl-d
        case TAB_KEY:            // tab('\t') or Ctrl-i
        case NEWLINE_KEY:        // enter('\n') or ctrl-m
        case CLEAR_CONSOLE_KEY:  // Clear console (Ctrl-l)
            return returnCh(ch);

        // -- The following simple/combo keys are platform-dependent
        //    You should test to check the returned codes of these key presses
        // -- You should either modify the "enum ParseChar" definitions in
        //    "cmdCharDef.h", or revise the control flow of the "case ESC" below
        case BACK_SPACE_KEY:
            return returnCh(ch);
        case char(8):
            return returnCh(BACK_SPACE_KEY);

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
                    else
                        return returnCh(UNDEFINED_KEY);
                } else if ((key >= char(ARROW_KEY_BEGIN)) &&
                           (key <= char(ARROW_KEY_END)))
                    return returnCh(int(key) + ARROW_KEY_FLAG);
                else
                    return returnCh(UNDEFINED_KEY);
            } else {
                mybeep();
                return getChar(istr);
            }
        }
        // For the remaining printable and undefined keys
        default:
            if (isprint(ch))
                return returnCh(ch);
            else
                return returnCh(UNDEFINED_KEY);
    }

    return returnCh(UNDEFINED_KEY);
}

inline static ParseChar
returnCh(int ch) {
    return ParseChar(ch);
}

void clearConsole() {
    int result;
#ifdef _WIN32
    result = system("cls");
#else
    result = system("clear");
#endif
    if (result != 0) {
        cerr << "Error clearing the console!!" << endl;
    }
}
