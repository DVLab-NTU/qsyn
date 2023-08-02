/****************************************************************************
  FileName     [ cmdCharDef.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Process keyboard inputs ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "cmdCharDef.h"

#include <ctype.h>
#include <termios.h>

#include <iostream>

#include "cmdParser.h"

using namespace std;

//----------------------------------------------------------------------
//    Global funcitons
//----------------------------------------------------------------------

void mybeep() {
    cout << (char)detail::getKeyCode(ParseChar::BEEP_CHAR);
}

void clearConsole() {
#ifdef _WIN32
    int result = system("cls");
#else
    int result = system("clear");
#endif
    if (result != 0) {
        cerr << "Error clearing the console!!" << endl;
    }
}

//----------------------------------------------------------------------
//    keypress detection details
//----------------------------------------------------------------------

namespace detail {
static struct termios stored_settings;

static auto reset_keypress(void) {
    tcsetattr(0, TCSANOW, &stored_settings);
}

static auto set_keypress(void) {
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

static auto mygetc(istream& istr) -> char {
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

static auto toParseChar(int ch) -> ParseChar {
    return ParseChar(ch);
};

}  // namespace detail

ParseChar
CmdParser::getChar(istream& istr) const {
    using namespace detail;
    using enum ParseChar;
    char ch = mygetc(istr);
    ParseChar parseChar{ch};

    if (istr.eof())
        return INTERRUPT_KEY;
    switch (parseChar) {
        // Simple keys: one code for one key press
        // -- The following should be platform-independent
        case LINE_BEGIN_KEY:     // Ctrl-a
        case LINE_END_KEY:       // Ctrl-e
        case INPUT_END_KEY:      // Ctrl-d
        case TAB_KEY:            // tab('\t') or Ctrl-i
        case NEWLINE_KEY:        // enter('\n') or ctrl-m
        case CLEAR_CONSOLE_KEY:  // Clear console (Ctrl-l)
            return parseChar;

        // -- The following simple/combo keys are platform-dependent
        //    You should test to check the returned codes of these key presses
        // -- You should either modify the "enum ParseChar" definitions in
        //    "cmdCharDef.h", or revise the control flow of the "case ESC" below
        case BACK_SPACE_KEY:
            return parseChar;
        case BACK_SPACE_CHAR:
            return BACK_SPACE_KEY;

        // Combo keys: multiple codes for one key press
        // -- Usually starts with ESC key, so we check the "case ESC"
        case ESC_KEY: {
            char combo = mygetc(istr);
            // Note: ARROW_KEY_INT == MOD_KEY_INT, so we only check MOD_KEY_INT
            if (combo == char(MOD_KEY_INT)) {
                char key = mygetc(istr);
                if ((key >= char(MOD_KEY_BEGIN)) && (key <= char(MOD_KEY_END))) {
                    if (mygetc(istr) == getKeyCode(MOD_KEY_DUMMY))
                        return toParseChar(int(key) + getKeyCode(MOD_KEY_FLAG));
                    else
                        return UNDEFINED_KEY;
                } else if ((key >= char(ARROW_KEY_BEGIN)) &&
                           (key <= char(ARROW_KEY_END)))
                    return toParseChar(int(key) + getKeyCode(ARROW_KEY_FLAG));
                else
                    return UNDEFINED_KEY;
            } else {
                mybeep();
                return getChar(istr);
            }
        }
        // For the remaining printable and undefined keys
        default:
            if (isprint(ch))
                return parseChar;
            else
                return UNDEFINED_KEY;
    }

    return UNDEFINED_KEY;
}
