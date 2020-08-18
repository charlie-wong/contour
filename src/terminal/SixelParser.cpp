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
#include <terminal/SixelParser.h>

#include <algorithm>

using std::clamp;
using std::fill;
using std::vector;

namespace terminal {

namespace
{
    constexpr bool isDigit(char _value) noexcept
    {
        return _value >= '0' && _value <= '9';
    }

    constexpr int toDigit(char _value) noexcept
    {
        return static_cast<int>(_value) - '0';
    }

    constexpr bool isSixel(char _value) noexcept
    {
        return _value >= 63 && _value <= 126;
    }

    constexpr char toSixel(char _value) noexcept
    {
        return static_cast<char>(static_cast<int>(_value) - 63);
    }
}

auto constexpr inline MaxColorCount = 256; // TODO: configurable?

SixelParser::SixelParser(Events& _events) :
    colorpalette_(256, RGBColor{}),
    events_{ _events }
{
}

void SixelParser::parse(char _value)
{
    switch (state_)
    {
        case State::Ground:
            fallback(_value);
            break;

        case State::RepeatIntroducer:
            // '!' NUMBER BYTE
            if (isDigit(_value))
                paramShiftAndAddDigit(toDigit(_value));
            else if (isSixel(_value))
            {
                auto const sixel = toSixel(_value);
                for (int i = 0; i < params_[0]; ++i)
                    events_.render(sixel);
                transitionTo(State::Ground);
            }
            else
                fallback(_value);
            break;

        case State::ColorIntroducer:
            if (isDigit(_value))
            {
                paramShiftAndAddDigit(toDigit(_value));
                transitionTo(State::ColorParam);
            }
            else
                fallback(_value);
            break;

        case State::ColorParam:
            if (isDigit(_value))
                paramShiftAndAddDigit(toDigit(_value));
            else if (_value == ';')
                params_.push_back(0);
            else
                fallback(_value);
            break;

        case State::RasterSettings:
            if (isDigit(_value))
                paramShiftAndAddDigit(toDigit(_value));
            else if (_value == ';')
                params_.push_back(0);
            else
                fallback(_value);
            break;
    }
}

void SixelParser::fallback(char _value)
{
    if (_value == '#')
        transitionTo(State::ColorIntroducer);
    else if (_value == '!')
        transitionTo(State::RepeatIntroducer);
    else if (_value == '"')
        transitionTo(State::RasterSettings);
    else if (_value == '$')
    {
        transitionTo(State::Ground);
        events_.rewind();
    }
    else if (_value == '-')
    {
        transitionTo(State::Ground);
        events_.newline();
    }
    else
    {
        if (state_ != State::Ground)
            transitionTo(State::Ground);

        if (isSixel(_value))
            events_.render(toSixel(_value));
    }

    // ignore any other input value
}

void SixelParser::done()
{
    transitionTo(State::Ground); // this also ensures current state's leave action is invoked
}

void SixelParser::paramShiftAndAddDigit(int _value)
{
    int& number = params_.back();
    number = number * 10 + _value;
}

void SixelParser::transitionTo(State _newState)
{
    leaveState();
    state_ = _newState;
    enterState();
}

void SixelParser::enterState()
{
    switch (state_)
    {
        case State::ColorIntroducer:
        case State::RepeatIntroducer:
        case State::RasterSettings:
            params_.clear();
            params_.push_back(0);
            break;

        case State::Ground:
        case State::ColorParam:
            break;
    }
}

void SixelParser::leaveState()
{
    switch (state_)
    {
        case State::Ground:
        case State::ColorIntroducer:
        case State::RepeatIntroducer:
            break;

        case State::RasterSettings:
            if (params_.size() == 4)
            {
                auto const pan = params_[0];
                auto const pad = params_[1];
                auto const xPixels = params_[2];
                auto const yPixels = params_[3];
                events_.setRaster(pan, pad, Size{xPixels, yPixels});
                state_ = State::Ground;
            }
            break;

        case State::ColorParam:
            if (params_.size() == 1)
            {
                auto const index = params_[0];
                auto const& color = colorpalette_[index % colorpalette_.size()];
                events_.useColor(color);
            }
            else if (params_.size() == 5)
            {
                auto constexpr convertValue = [](int _value) {
                    // converts a color from range 0..100 to 0..255
                    return static_cast<uint8_t>(static_cast<int>((static_cast<float>(_value) * 255.0f) / 100.0f) % 256);
                };
                auto const index = params_[0];
                auto const colorSpace = params_[1] == 2 ? Colorspace::RGB : Colorspace::HSL;
                if (colorSpace == Colorspace::RGB)
                {
                    auto const p1 = convertValue(params_[2]);
                    auto const p2 = convertValue(params_[3]);
                    auto const p3 = convertValue(params_[4]);
                    auto const color = RGBColor{p1, p2, p3}; // TODO: convert HSL if requested
                    setColor(index, color);
                }
            }
            break;
    }
}

void SixelParser::setColor(int _index, RGBColor const& _color)
{
    if (_index <= MaxColorCount)
    {
        if (_index >= static_cast<int>(colorpalette_.size()))
            colorpalette_.resize(_index);

        colorpalette_[_index] = _color;
    }
}

// =================================================================================

SixelImageBuilder::SixelImageBuilder(Size const& _maxSize, RGBColor const& _defaultColor) :
    maxSize_{ _maxSize },
    defaultColor_{ _defaultColor },
    size_{ _maxSize },
    buffer_(size_.width * size_.height * 4),
    sixelCursor_{ 0, 0 }
{
    clear();
}

void SixelImageBuilder::clear()
{
    sixelCursor_ = {0, 0};

    auto p = &buffer_[0];
    for (int i = 0; i < size_.width * size_.height; ++i)
    {
        *p++ = defaultColor_.red;
        *p++ = defaultColor_.green;
        *p++ = defaultColor_.blue;
        *p++ = 0xFF;
    }
}

RGBColor SixelImageBuilder::at(Coordinate const& _coord) const noexcept
{
    auto const row = _coord.row % size_.height;
    auto const col = _coord.column % size_.width;
    auto const base = row * size_.width * 4 + col * 4;
    auto const color = &buffer_[base];
    return RGBColor{color[0], color[1], color[2]};
}

void SixelImageBuilder::write(Coordinate const& _coord, RGBColor const& _value) noexcept
{
    if (_coord.row >= 0 && _coord.row < size_.height && _coord.column >= 0 && _coord.column < size_.width)
    {
        auto const base = _coord.row * size_.width * 4 + _coord.column * 4;
        buffer_[base + 0] = _value.red;
        buffer_[base + 1] = _value.green;
        buffer_[base + 2] = _value.blue;
        buffer_[base + 3] = 0xFF;
    }
}

void SixelImageBuilder::useColor(RGBColor const& _color)
{
    currentColor_ = _color;
}

void SixelImageBuilder::rewind()
{
    sixelCursor_.column = 0;
}

void SixelImageBuilder::newline()
{
    sixelCursor_.column = 0;

    if (sixelCursor_.row + 6 < size_.height)
        sixelCursor_.row += 6;
}

void SixelImageBuilder::setRaster(int _pan, int _pad, Size const& _imageSize)
{
    aspectRatio_.nominator = _pan;
    aspectRatio_.denominator = _pad;
    size_.width = clamp(_imageSize.width, 0, maxSize_.width);
    size_.height = clamp(_imageSize.height, 0, maxSize_.height);

    buffer_.resize(size_.width * size_.height * 4);
}

void SixelImageBuilder::render(int8_t _sixel)
{
    auto const x = sixelCursor_.column;
    if (x < size_.width)
    {
        for (int i = 0; i < 6; ++i)
        {
            auto const y = sixelCursor_.row + i;
            auto const pos = Coordinate{y, x};
            auto const pin = 1 << i;
            auto const pinned = (_sixel & pin) != 0;
            if (pinned)
                write(pos, currentColor_);
        }
        sixelCursor_.column++;
    }
}

}
