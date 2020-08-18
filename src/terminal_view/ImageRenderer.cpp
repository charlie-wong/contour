#include <terminal_view/ImageRenderer.h>

namespace terminal::view {

ImageRenderer::ImageRenderer(crispy::atlas::CommandListener& _commandListener,
                             crispy::atlas::TextureAtlasAllocator& _colorAtlasAllocator,
                             Size const& _cellSize) :
    imagePool_{},
    commandListener_{ _commandListener },
    cellSize_{ _cellSize },
    atlas_{ _colorAtlasAllocator }
{
    (void) _colorAtlasAllocator; // TODO
}

void ImageRenderer::setCellSize(Size const& _cellSize)
{
    cellSize_ = _cellSize;
    // TODO: recompute slices here?
}


void ImageRenderer::renderImage(Image const& _image,
                                Coordinate _offset,
                                Size _extent)
{
    (void) _image;
    (void) _offset;
    (void) _extent;
}

void ImageRenderer::clearCache()
{
    atlas_.clear();
}

} // end namespace
