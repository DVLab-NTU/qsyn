/****************************************************************************
  FileName     [ cmdReader.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Read the command from the standard input or dofile ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <cassert>  // for assert
#include <cstring>  // for strcpy
#include <iostream>

#include "cmdCharDef.h"  // for ParseChar, ParseChar::BACK_SPACE_CHAR, Parse...
#include "cmdParser.h"   // for CmdParser, PG_OFFSET

using namespace std;

//----------------------------------------------------------------------
//    Extrenal funcitons
//----------------------------------------------------------------------
void mybeep();

void clearConsole();

//----------------------------------------------------------------------
//    Member Function for class CmdParser
//----------------------------------------------------------------------
bool CmdParser::readCmd(istream& istr) {
    resetBufAndPrintPrompt();

    bool newCmd = false;
    // listen for keystrokes
    while (!newCmd) {
        ParseChar pch = getChar(istr);
        if (pch == INPUT_END_KEY) {
            if (_dofile != 0)
                closeDofile();
            cout << "\nquit" << endl;
            exit(0);
        }
        // Note: actual ctrl-c triggers SIGINT, and is therefore handled by
        // `void CmdParser::sigintHandler(int signum);`
        // This INTERRUPT_KEY is sent when EOF of a dofile is reached
        if (pch == INTERRUPT_KEY) {
            if (_dofile != 0)
                closeDofile();
            newCmd = addHistory();
            cout << char(NEWLINE_KEY);
            // if (!newCmd) resetBufAndPrintPrompt();
            break;
        }
        switch (pch) {
            case LINE_BEGIN_KEY:
            case HOME_KEY:
                moveCursor(0);
                break;
            case LINE_END_KEY:
            case END_KEY:
                moveCursor(_readBuf.size());
                break;
            case BACK_SPACE_KEY:
                if (moveCursor(_cursorPosition - 1))
                    deleteChar();
                break;
            case DELETE_KEY:
                deleteChar();
                break;
            case NEWLINE_KEY:
                newCmd = addHistory();
                cout << char(NEWLINE_KEY);
                if (!newCmd) resetBufAndPrintPrompt();
                break;
            case CLEAR_CONSOLE_KEY:
                clearConsole();
                cout << char(NEWLINE_KEY);
                resetBufAndPrintPrompt();
                break;
            case ARROW_UP_KEY:
                moveToHistory(_historyIdx - 1);
                break;
            case ARROW_DOWN_KEY:
                moveToHistory(_historyIdx + 1);
                break;
            case ARROW_RIGHT_KEY:
                moveCursor(_cursorPosition + 1);
                break;
            case ARROW_LEFT_KEY:
                moveCursor((int)_cursorPosition - 1);
                break;
            case PG_UP_KEY:
                moveToHistory(_historyIdx - PG_OFFSET);
                break;
            case PG_DOWN_KEY:
                moveToHistory(_historyIdx + PG_OFFSET);
                break;
            case TAB_KEY: {
                ++_tabPressCount;
                listCmd(stripLeadingWhitespaces(_readBuf.substr(0, _cursorPosition)));
                break;
            }
            case INSERT_KEY:  // not yet supported; fall through to UNDEFINE
            case UNDEFINED_KEY:
                mybeep();
                break;
            default:  // printable character
                insertChar(char(pch));
                break;
        }
    }
    return newCmd;
}

// This function moves _readBufPtr to the "ptr" pointer
// It is used by left/right arrowkeys, home/end, etc.
//
// Suggested steps:
// 1. Make sure ptr is within [_readBuf, _readBufEnd].
//    If not, make a beep sound and return false. (DON'T MOVE)
// 2. Move the cursor to the left or right, depending on ptr
// 3. Update _readBufPtr accordingly. The content of the _readBuf[] will
//    not be changed
//
// [Note] This function can also be called by other member functions below
//        to move the _readBufPtr to proper position.
bool CmdParser::moveCursor(int idx) {
    if (idx < 0 || (size_t)idx > _readBuf.size()) {
        mybeep();
        return false;
    }

    // move left
    if (_cursorPosition > (size_t)idx) {
        cout << string(_cursorPosition - idx, char(BACK_SPACE_CHAR));
    }

    // move right
    if (_cursorPosition < (size_t)idx) {
        cout << _readBuf.substr(_cursorPosition, idx - _cursorPosition);
    }
    _cursorPosition = idx;
    return true;
}

// [Notes]
// 1. Delete the char at _readBufPtr
// 2. mybeep() and return false if at _readBufEnd
// 3. Move the remaining string left for one character
// 4. The cursor should stay at the same position
// 5. Remember to update _readBufEnd accordingly.
// 6. Don't leave the tailing character.
// 7. Call "moveBufPtr(...)" if needed.
//
// For example,
//
// cmd> This is the command
//              ^                (^ is the cursor position)
//
// After calling deleteChar()---
//
// cmd> This is he command
//              ^
//
bool CmdParser::deleteChar() {
    if (_cursorPosition == _readBuf.size()) {
        mybeep();
        return false;
    }
    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.

    cout << _readBuf.substr(_cursorPosition + 1);  // will move cursor to the end
    cout << " " << char(BACK_SPACE_CHAR);          // get rid of the last character
    _readBuf.erase(_cursorPosition, 1);

    int idx = _cursorPosition;
    _cursorPosition = _readBuf.size();  // before moving cursor, reflect the change in actual cursor location
    moveCursor(idx);                    // move the cursor back to where it should be
    return true;
}

// 1. Insert character 'ch' for "repeat" times at _readBufPtr
// 2. Move the remaining string right for "repeat" characters
// 3. The cursor should move right for "repeats" positions afterwards
// 4. Default value for "repeat" is 1. You should assert that (repeat >= 1).
//
// For example,
//
// cmd> This is the command
//              ^                (^ is the cursor position)
//
// After calling insertChar('k', 3) ---
//
// cmd> This is kkkthe command
//                 ^
//
void CmdParser::insertChar(char ch) {
    _readBuf.insert(_cursorPosition, 1, ch);

    cout << _readBuf.substr(_cursorPosition + 1);
    moveCursor(_cursorPosition + 1);
}

// 1. Delete the line that is currently shown on the screen
// 2. Reset _readBufPtr and _readBufEnd to _readBuf
// 3. Make sure *_readBufEnd = 0
//
// For example,
//
// cmd> This is the command
//              ^                (^ is the cursor position)
//
// After calling deleteLine() ---
//
// cmd>
//      ^
//
void CmdParser::deleteLine() {
    moveCursor(_readBuf.size());
    cout << string(_cursorPosition, '\b') << string(_cursorPosition, ' ') << string(_cursorPosition, '\b');
    _readBuf.clear();
}

// Reprint the current command to a newline
// cursor should be restored to the original location
void CmdParser::reprintCmd() {
    cout << endl;

    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.
    int idx = _cursorPosition;
    _cursorPosition = _readBuf.size();  // before moving cursor, reflect the change in actual cursor location
    printPrompt();
    cout << _readBuf;
    moveCursor(idx);  // move the cursor back to where it should be
}

// This functions moves _historyIdx to index and display _history[index]
// on the screen.
//
// Need to consider:
// If moving up... (i.e. index < _historyIdx)
// 1. If already at top (i.e. _historyIdx == 0), beep and do nothing.
// 2. If at bottom, temporarily record _readBuf to history.
//    (Do not remove spaces, and set _tempCmdStored to "true")
// 3. If index < 0, let index = 0.
//
// If moving down... (i.e. index > _historyIdx)
// 1. If already at bottom, beep and do nothing
// 2. If index >= _history.size(), let index = _history.size() - 1.
//
// Assign _historyIdx to index at the end.
//
// [Note] index should not = _historyIdx
//
void CmdParser::moveToHistory(int index) {
    if (index < _historyIdx) {  // move up
        if (_historyIdx == 0) {
            mybeep();
            return;
        }
        if (size_t(_historyIdx) == _history.size()) {  // mv away from new str
            _tempCmdStored = true;
            _history.push_back(_readBuf);
        } else if (_tempCmdStored &&  // the last _history is a stored temp cmd
                   size_t(_historyIdx) == size_t(_history.size() - 1))
            _history.back() = _readBuf;  // => update it
        if (index < 0)
            index = 0;
    } else if (index > _historyIdx) {  // move down
        if ((_tempCmdStored &&
             (size_t(_historyIdx) == size_t(_history.size() - 1))) ||
            (!_tempCmdStored && (size_t(_historyIdx) == _history.size()))) {
            mybeep();
            return;
        }
        if (size_t(index) >= size_t(_history.size() - 1))
            index = int(_history.size() - 1);
    } else                             // index == _historyIdx
        assert(index != _historyIdx);  // must fail!!

    _historyIdx = index;
    retrieveHistory();
}

/**
 * @brief Add the command in buffer to _history. This function trim the comment, leading/trailing whitespace of the entered comments
 *
 * @return `true` if a new command is added to _history, `false` if not
 */
bool CmdParser::addHistory() {
    string cmd = stripWhitespaces(stripComments(_readBuf));

    if (_tempCmdStored) {
        _history.pop_back();
        _tempCmdStored = false;
    }

    bool newCmd = false;
    if (cmd.size()) {
        // add to _history
        _history.emplace_back(cmd);
        newCmd = true;
    }

    _historyIdx = int(_history.size());

    return newCmd;
}

// 1. Replace current line with _history[_historyIdx] on the screen
// 2. Set _readBufPtr and _readBufEnd to end of line
//
// [Note] Do not change _history.size().
//
void CmdParser::retrieveHistory() {
    deleteLine();
    _readBuf = _history[_historyIdx];
    cout << _readBuf;
    _cursorPosition = _history[_historyIdx].size();
}
