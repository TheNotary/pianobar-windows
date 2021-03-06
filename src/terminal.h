/*
Copyright (c) 2008-2010
	Lars-Dominik Braun <lars@6xq.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _TERMINAL_H
#define _TERMINAL_H

#ifdef _WIN32

#include <winsock2.h>

HANDLE BarConsoleGetStdInHandleWin32 (void);
HANDLE BarConsoleGetStdOutHandleWin32 (void);
void BarConsoleSetCursorPositionWin32 (COORD position);
COORD BarConsoleGetCursorPositionWin32 (void);
COORD BarConsoleMoveCursorWin32 (int xoffset);
void BarConsoleClearLineWin32 (void);
void BarConsoleSetSizeWin32 (int width, int height);
void BarConsoleSetTitle (const char* title);
void BarConsoleInitialize (void);

#else

void BarTermSetEcho (char);
void BarTermSetBuffer (char);
void BarTermSave (struct termios *);
void BarTermRestore (struct termios *termOrig);

#endif

#endif /* _TERMINAL_H */
