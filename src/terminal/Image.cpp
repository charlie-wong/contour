#include <terminal/Image.h>

namespace terminal {

ImageRef ImagePool::create(Image::Data _data, Size _size)
{
    images_.emplace_back(move(_data), _size, [this](Image const& _image) { remove(_image); });
    return ImageRef{images_.back()};
}

ImageRef ImagePool::create(std::vector<RGBColor> const& _data, Size _size)
{
    std::vector<uint8_t> data;
    data.resize(_size.width * _size.height * 4);

    size_t i = 0;
    for (RGBColor const& color : _data)
    {
        data[i++] = color.red;
        data[i++] = color.green;
        data[i++] = color.blue;
        data[i++] = 0xFF;
    }

    return create(data, _size);
}

void ImagePool::remove(Image const& _image)
{
    auto i = images_.begin();
    while (i != images_.end() && &*i != &_image)
        ++i;
    if (i != images_.end())
        images_.erase(i);
}

} // end namespace
