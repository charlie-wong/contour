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
#include "LoggingSink.h"

#include <crispy/escape.h>
#include <crispy/overloaded.h>

#include <fmt/format.h>

#include <array>
#include <fstream>

using namespace std;
using namespace terminal;
using namespace crispy;

LoggingSink::LoggingSink(LogMask _logMask, FileSystem::path _logfile) :
    logMask_{ _logMask },
    ownedSink_{ make_unique<ofstream>(_logfile.string(), ios::trunc) },
    sink_{ ownedSink_.get() }
{
}

LoggingSink::LoggingSink(LogMask _logMask, std::ostream* _sink) :
    logMask_{ _logMask },
    ownedSink_{},
    sink_{ _sink }
{
}

void LoggingSink::keyPress(Key _key, Modifier _modifier)
{
    log(TraceInputEvent{ fmt::format("key: {} {}", to_string(_key), to_string(_modifier)) });
}

void LoggingSink::keyPress(char32_t _char, Modifier _modifier)
{
    if (_char <= 0x7F && isprint(_char))
        log(TraceInputEvent{ fmt::format("char: {} ({})", static_cast<char>(_char), to_string(_modifier)) });
    else
        log(TraceInputEvent{ fmt::format("char: 0x{:04X} ({})", static_cast<uint32_t>(_char), to_string(_modifier)) });
}

void LoggingSink::log(LogEvent const& _event)
{
    LogMask const m = visit(overloaded{
            [&](ParserErrorEvent const&) {
                return LogMask::ParserError;
            },
            [&](RawInputEvent const&) {
                return LogMask::RawInput;
            },
            [&](RawOutputEvent const&) {
                return LogMask::RawOutput;
            },
            [&](InvalidOutputEvent const&) {
                return LogMask::InvalidOutput;
            },
            [&](UnsupportedOutputEvent const&) {
                return LogMask::UnsupportedOutput;
            },
            [&](TraceInputEvent const&) {
                return LogMask::TraceInput;
            },
            [&](TraceOutputEvent const&) {
                return LogMask::TraceOutput;
            },
        },
        _event
    );

    if ((logMask_ & m) != LogMask::None)
        *sink_ << fmt::format("{}\n", _event);
}

void LoggingSink::flush()
{
    if (sink_)
        sink_->flush();
}
