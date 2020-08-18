#pragma once

#include <crispy/Atlas.h>
#include <crispy/AtlasRenderer.h>

#include <terminal/Image.h>
#include <terminal/Size.h>

#include <vector>

namespace terminal::view {

// an exemplary RenderImage command.
struct RenderImage
{
    ImageRef image;
    Size extent;

    Coordinate offset = {0, 0}; // render only between [offset .. extent] instead of [(0, 0) .. extent]
    ImageResize resize = ImageResize::ResizeToFit;
    ImageAlignment alignment = ImageAlignment::MiddleCenter;

    std::vector<bool> mask {};
};

/// Image Rendering API.
///
/// Can render any arbitrary RGBA image (for example Sixel Graphics images).
class ImageRenderer
{
  public:
    ImageRenderer(
         crispy::atlas::CommandListener& _commandListener,
         crispy::atlas::TextureAtlasAllocator& _colorAtlasAllocator,
         Size const& _cellSize
    );

    /// Reconfigures the slicing properties of existing images.
    void setCellSize(Size const& _cellSize);

    void renderImage(Image const& _image,
                     Coordinate _offset,
                     Size _extent);

    struct Slice {
        Size offset;
    };
    using TextureAtlas = crispy::atlas::MetadataTextureAtlas<ImageRef, Slice>;
    using DataRef = TextureAtlas::DataRef;

    void clearCache();

  private:
    ImagePool imagePool_;
    crispy::atlas::CommandListener& commandListener_;
    Size cellSize_;
    TextureAtlas atlas_;
};

}
