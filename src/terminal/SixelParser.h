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
#pragma once

#include <terminal/Color.h>
#include <terminal/Commands.h> // Coordinate
#include <terminal/Size.h>

#include <crispy/range.h>

#include <array>
#include <string_view>
#include <vector>

namespace terminal {

/// Sixel Stream Parser API.
///
/// Parses a sixel stream without any Sixel introducer CSI or ST to leave sixel mode,
/// that must be done by the parent parser.
///
/// TODO: make this parser O(1) with state table lookup tables, just like the VT parser.
class SixelParser
{
  public:
    enum class State {
        Ground,             // Sixel data
        RasterSettings,     // '"', configuring the raster
        RepeatIntroducer,   // '!'
        ColorIntroducer,    // '#', color-set or color-use
        ColorParam          // color parameter
    };

    enum class Colorspace { RGB, HSL };

    /// SixelParser's event handler
    class Events
    {
      public:
        virtual ~Events() = default;

        /// Uses the given color for future paints
        virtual void useColor(RGBColor const& _color) = 0;

        /// moves sixel-cursor to the left border
        virtual void rewind() = 0;

        /// moves the sixel-cursorto the left border of the next sixel-band
        virtual void newline() = 0;

        /// Defines the aspect ratio (pan / pad = aspect ratio) and image dimensions in pixels for
        /// the upcoming pixel data.
        virtual void setRaster(int _pan, int _pad, Size const& _imageSize) = 0;

        /// renders a given sixel at the current sixel-cursor position.
        virtual void render(int8_t _sixel) = 0;
    };

    explicit SixelParser(Events& _events);

    using iterator = char const*;

    void parseFragment(iterator _begin, iterator _end)
    {
        for (auto const ch : crispy::range(_begin, _end))
            parse(ch);
    }

    void parseFragment(std::string_view _range)
    {
        parseFragment(_range.data(), _range.data() + _range.size());
    }

    void parse(char _value);
    void done();

    static void parse(std::string_view _range, Events& _events)
    {
        auto parser = SixelParser{_events};
        parser.parseFragment(_range.data(), _range.data() + _range.size());
        parser.done();
    }

  private:
    void paramShiftAndAddDigit(int _value);
    void transitionTo(State _newState);
    void enterState();
    void leaveState();
    void setColor(int _index, RGBColor const& _color);
    void fallback(char _value);

  private:
    State state_ = State::Ground;
    std::vector<int> params_;
    std::vector<RGBColor> colorpalette_;

    Events& events_;
};

/// Sixel Image Builder API
///
/// Implements the SixelParser::Events event listener to construct a Sixel image.
class SixelImageBuilder : public SixelParser::Events
{
  public:
    using Buffer = std::vector<uint8_t>;

    SixelImageBuilder(Size const& _maxSize, RGBColor const& _defaultColor);

    RGBColor const& defaultColor() const noexcept { return defaultColor_; }
    Size const& maxSize() const noexcept { return maxSize_; }

    Size const& size() const noexcept { return size_; }
    int aspectRatioNominator() const noexcept { return aspectRatio_.nominator; }
    int aspectRatioDenominator() const noexcept { return aspectRatio_.denominator; }
    RGBColor const& currentColor() const noexcept { return currentColor_; }

    RGBColor at(Coordinate const& _coord) const noexcept;

    Buffer const& data() const noexcept { return buffer_; }
    Buffer& data() noexcept { return buffer_; }

    void clear();

    void useColor(RGBColor const& _color) override;
    void rewind() override;
    void newline() override;
    void setRaster(int _pan, int _pad, Size const& _imageSize) override;
    void render(int8_t _sixel) override;

    Coordinate const& sixelCursor() const noexcept { return sixelCursor_; }

  private:
    void write(Coordinate const& _coord, RGBColor const& _value) noexcept;

  private:
    Size const maxSize_;
    RGBColor const defaultColor_;
    Size size_;
    Buffer buffer_; /// RGBA buffer
    Coordinate sixelCursor_;
    RGBColor currentColor_;
    struct {
        int nominator;
        int denominator;
    } aspectRatio_;
};

} // end namespace
