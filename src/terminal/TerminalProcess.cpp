/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019-2020 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <terminal/TerminalProcess.h>

using namespace terminal;
using namespace std;

TerminalProcess::TerminalProcess(Process::ExecInfo const& _shell,
                                 Size _winSize,
                                 Terminal::Events& _eventListener,
                                 optional<size_t> _maxHistoryLineCount,
                                 chrono::milliseconds _cursorBlinkInterval,
                                 chrono::steady_clock::time_point _now,
                                 string const& _wordDelimiters,
                                 CursorDisplay _cursorDisplay,
                                 CursorShape _cursorShape,
                                 Logger _logger) :
    Terminal(
        _winSize,
        _eventListener,
        _maxHistoryLineCount,
        _cursorBlinkInterval,
        _now,
        move(_logger),
        _wordDelimiters
    ),
    Process{_shell, terminal().device()}
{
    terminal().setCursorDisplay(_cursorDisplay);
    terminal().setCursorShape(_cursorShape);
}

TerminalProcess::~TerminalProcess()
{
    // Closing the terminal I/O.
    // Maybe the process is still alive, but we need to disconnect from the PTY,
    // so that the Process will be notified via SIGHUP.
    // NB: We MUST close the PTY device before waiting for the process to terminate.
    terminal().device().close();

    // Wait until the process is actually terminated.
    (void) Process::wait();
}
