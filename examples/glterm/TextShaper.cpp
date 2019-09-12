#include "TextShaper.h"

#include <stdexcept>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H

#if defined(_MSC_VER)
// XXX purely for IntelliSense
#include <freetype/freetype.h>
#endif

#if defined(__linux__)
#include <fontconfig/fontconfig.h>
#endif

using namespace std;

std::string getFontFilePath(
    [[maybe_unused]] string const& _fontPattern,
    [[maybe_unused]] bool _bold,
    [[maybe_unused]] bool _italic)
{
    #if defined(FC_FAMILY)
    string const pattern = _fontPattern; // TODO: append bold/italic if needed

    FcConfig* fcConfig = FcInitLoadConfigAndFonts();
    FcPattern* fcPattern = FcNameParse((FcChar8 const*) pattern.c_str());

    FcDefaultSubstitute(fcPattern);
    FcConfigSubstitute(fcConfig, fcPattern, FcMatchPattern);

    FcResult fcResult;
    FcPattern* matchedPattern = FcFontMatch(fcConfig, fcPattern, &fcResult);
    auto path = string{};
    if (fcResult == FcResultMatch && matchedPattern)
    {
        char* resultPath{};
        if (FcPatternGetString(matchedPattern, FC_FILE, 0, (FcChar8**) &resultPath) == FcResultMatch)
            path = string{resultPath};
        FcPatternDestroy(matchedPattern);
    }
    FcPatternDestroy(fcPattern);
    FcConfigDestroy(fcConfig);
    return path;
    #elif defined(_MSC_VER)
    // TODO: Read https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-enumfontfamiliesexa
    return "C:\\WINDOWS\\FONTS\\CONSOLA.TTF";
    #endif
}

TextShaper::TextShaper(string const& _fontFamily, unsigned int _fontSize, glm::mat4 const& _projectionMatrix) :
    fontSize_{ _fontSize },
    shader_{ vertexShaderCode(), fragmentShaderCode() }
{
    if (FT_Init_FreeType(&ft_))
        throw runtime_error{ "Failed to initialize FreeType." };

    string fontPathRegular = getFontFilePath(_fontFamily, false, false);

    if (FT_New_Face(ft_, fontPathRegular.c_str(), 0, &face_)) // Consolas font on Windows
        throw runtime_error{ "Failed to load font." };

    FT_Error ec = FT_Select_Charmap(face_, FT_ENCODING_UNICODE);
    if (ec)
        throw runtime_error{ string{"Failed to set charmap. "} + freetypeErrorString(ec) };

    ec = FT_Set_Pixel_Sizes(face_, 0, _fontSize);
    if (ec)
        throw runtime_error{ string{"Failed to set font pixel size. "} + freetypeErrorString(ec) };

    // disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Configure VAO/VBO for texture quads
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    setProjection(_projectionMatrix);
}

TextShaper::~TextShaper()
{
    FT_Done_Face(face_);
    FT_Done_FreeType(ft_);
}

void TextShaper::setProjection(glm::mat4 const& _projectionMatrix)
{
	shader_.use();
	shader_.setMat4("projection", _projectionMatrix);
}

string const& TextShaper::vertexShaderCode()
{
	static string const code = R"(
		#version 330 core
		layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
		out vec2 TexCoords;

		uniform mat4 projection;

		void main()
		{
			gl_Position = projection * vec4(vertex.xy, 0.1, 1.0);
			TexCoords = vertex.zw;
		}
	)";
	return code;
}

string const& TextShaper::fragmentShaderCode()
{
	static string const code = R"(
		#version 330 core
		in vec2 TexCoords;
		out vec4 color;

		uniform sampler2D text;
		uniform vec4 textColor;

		void main()
		{
			vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
			color = textColor * sampled;
		}
	)";
	return code;
}

unsigned TextShaper::render(glm::ivec2 _pos, char32_t _char, glm::vec4 const& _color, TextStyle _style)
{
    unsigned const _x = _pos[0];
    unsigned const _y = _pos[1];

    shader_.use();
    shader_.setVec4("textColor", _color);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);

    //.
    Glyph const& ch = *getGlyph(_char);

    glColor3f(1.0, 1.0, 1.0);

    auto const xpos = static_cast<GLfloat>(_x + ch.bearing.x);
    auto const ypos = static_cast<GLfloat>(_y + baseline() - ch.descender);
    auto const w = static_cast<GLfloat>(ch.size.x);
    auto const h = static_cast<GLfloat>(ch.size.y);

    // Update VBO for each character
    GLfloat vertices[6][4] = {
        { xpos,     ypos + h,   0.0, 0.0 },
        { xpos,     ypos,       0.0, 1.0 },
        { xpos + w, ypos,       1.0, 1.0 },

        { xpos,     ypos + h,   0.0, 0.0 },
        { xpos + w, ypos,       1.0, 1.0 },
        { xpos + w, ypos + h,   1.0, 0.0 }
    };

    // Render glyph texture over quad
    glBindTexture(GL_TEXTURE_2D, ch.texturedID);

    // Update content of VBO memory
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // Be sure to use glBufferSubData and not glBufferData
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    return ch.advance;
}

TextShaper::Glyph* TextShaper::getGlyph(char32_t _char)
{
    if (auto i = glyphCache_.find(_char); i != end(glyphCache_))
        return &i->second;

    auto const glyphIndex = FT_Get_Char_Index(face_, _char);

    FT_Error ec = FT_Load_Glyph(face_, glyphIndex, FT_LOAD_RENDER);
    if (ec != FT_Err_Ok)
        throw runtime_error{ string{"Error loading glyph. "} + freetypeErrorString(ec) };

    // Generate texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        face_->glyph->bitmap.width,
        face_->glyph->bitmap.rows,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        face_->glyph->bitmap.buffer
    );

    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // store character for later use
    auto const descender = face_->glyph->metrics.height / 64 - face_->glyph->bitmap_top;
    Glyph& glyph = glyphCache_.emplace(make_pair(_char, Glyph{
        texture,
        glm::ivec2{(unsigned)face_->glyph->bitmap.width, (unsigned)face_->glyph->bitmap.rows},
        glm::ivec2{(unsigned)face_->glyph->bitmap_left, (unsigned)face_->glyph->bitmap_top},
        static_cast<unsigned>(face_->height) / 64,
        static_cast<unsigned>(descender),
        static_cast<unsigned>(face_->glyph->advance.x / 64)
    })).first->second;

    return &glyph;
}

string TextShaper::freetypeErrorString(FT_Error _errorCode)
{
    #undef __FTERRORS_H__
    #define FT_ERROR_START_LIST     switch (_errorCode) {
    #define FT_ERRORDEF(e, v, s)    case e: return s;
    #define FT_ERROR_END_LIST       }
    #include FT_ERRORS_H
    return "(Unknown error)";
}