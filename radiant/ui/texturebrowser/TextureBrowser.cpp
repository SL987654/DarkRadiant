#include "TextureBrowser.h"

#include "i18n.h"
#include "ieventmanager.h"
#include "itextstream.h"
#include "iuimanager.h"
#include "igroupdialog.h"
#include "iradiant.h"
#include "ipreferencesystem.h"

#include "wxutil/menu/IconTextMenuItem.h"
#include "wxutil/GLWidget.h"
#include "wxutil/NonModalEntry.h"
#include "wxutil/MouseButton.h"
#include "wxutil/Modifier.h"

#include "registry/registry.h"
#include "shaderlib.h"
#include "selection/algorithm/Shader.h"
#include "selection/shaderclipboard/ShaderClipboard.h"
#include "ui/mediabrowser/MediaBrowser.h"

#include <boost/algorithm/string/predicate.hpp>
#include <functional>

#include <wx/panel.h>
#include <wx/wxprec.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/scrolbar.h>
#include <wx/sizer.h>

namespace ui
{

namespace
{
	const std::string RKEY_TEXTURES_CLAMP_TO_UNIFORM_SIZE = "user/ui/textures/browser/clampToUniformSize";
    const std::string RKEY_TEXTURES_HIDE_UNUSED = "user/ui/textures/browser/hideUnused";
    const std::string RKEY_TEXTURE_SCALE = "user/ui/textures/browser/textureScale";
    const std::string RKEY_TEXTURE_UNIFORM_SIZE = "user/ui/textures/browser/uniformSize";
    const std::string RKEY_TEXTURE_SHOW_SCROLLBAR = "user/ui/textures/browser/showScrollBar";
    const std::string RKEY_TEXTURE_MOUSE_WHEEL_INCR = "user/ui/textures/browser/mouseWheelIncrement";
    const std::string RKEY_TEXTURE_SHOW_FILTER = "user/ui/textures/browser/showFilter";
    const std::string RKEY_TEXTURE_CONTEXTMENU_EPSILON = "user/ui/textures/browser/contextMenuMouseEpsilon";
    const std::string RKEY_TEXTURE_MAX_NAME_LENGTH = "user/ui/textures/browser/maxShadernameLength";

    const char* const SEEK_IN_MEDIA_BROWSER_TEXT = N_("Seek in Media Browser");
    const char* TEXTURE_ICON = "icon_texture.png";

    inline void doNothing() {}

    inline bool alwaysFalse() { return false; }

    int FONT_HEIGHT()
    {
        static int height = GlobalOpenGL().getFontHeight() * 1.15;
        return height;
    }

    const int VIEWPORT_BORDER = 12;
    const int TILE_BORDER = 6;
}

TextureBrowser::TextureBrowser() :
    _popupX(-1),
    _popupY(-1),
    _startOrigin(-1),
    _epsilon(registry::getValue<float>(RKEY_TEXTURE_CONTEXTMENU_EPSILON)),
    _popupMenu(new wxutil::PopupMenu),
	_filter(NULL),
    _filterIgnoresTexturePath(true),
    _filterIsIncremental(true),
	_wxGLWidget(NULL),
    _heightChanged(true),
    _originInvalid(true),
    _mouseWheelScrollIncrement(registry::getValue<int>(RKEY_TEXTURE_MOUSE_WHEEL_INCR)),
    _textureScale(50),
    _showTextureFilter(registry::getValue<bool>(RKEY_TEXTURE_SHOW_FILTER)),
    _showTextureScrollbar(registry::getValue<bool>(RKEY_TEXTURE_SHOW_SCROLLBAR)),
    _hideUnused(registry::getValue<bool>(RKEY_TEXTURES_HIDE_UNUSED)),
    _resizeTextures(registry::getValue<bool>(RKEY_TEXTURES_CLAMP_TO_UNIFORM_SIZE)),
    _uniformTextureSize(registry::getValue<int>(RKEY_TEXTURE_UNIFORM_SIZE))
{
    observeKey(RKEY_TEXTURES_HIDE_UNUSED);
    observeKey(RKEY_TEXTURE_SCALE);
    observeKey(RKEY_TEXTURE_UNIFORM_SIZE);
    observeKey(RKEY_TEXTURE_SHOW_SCROLLBAR);
    observeKey(RKEY_TEXTURE_MOUSE_WHEEL_INCR);
    observeKey(RKEY_TEXTURE_SHOW_FILTER);
	observeKey(RKEY_TEXTURES_CLAMP_TO_UNIFORM_SIZE);

    _shader = texdef_name_default();

    setScaleFromRegistry();

	_shaderLabel = new wxutil::IconTextMenuItem(_("No shader"), TEXTURE_ICON),

	_popupMenu->addItem(_shaderLabel, doNothing, alwaysFalse); // always insensitive

    // Construct the popup context menu
    _seekInMediaBrowser = new wxutil::IconTextMenuItem(_(SEEK_IN_MEDIA_BROWSER_TEXT), 
		TEXTURE_ICON);

    _popupMenu->addItem(_seekInMediaBrowser,
                        std::bind(&TextureBrowser::onSeekInMediaBrowser, this),
                        std::bind(&TextureBrowser::checkSeekInMediaBrowser, this));

	// Catch the RightUp event during mouse capture
	_freezePointer.connectMouseEvents(
		wxutil::FreezePointer::MouseEventFunction(),
		std::bind(&TextureBrowser::onGLMouseButtonRelease, this, std::placeholders::_1));

	_freezePointer.setCallEndMoveOnMouseUp(false);
}

void TextureBrowser::observeKey(const std::string& key)
{
    GlobalRegistry().signalForKey(key).connect(
        sigc::mem_fun(this, &TextureBrowser::keyChanged)
    );
}

void TextureBrowser::queueDraw()
{
    if (_wxGLWidget != NULL)
    {
		_wxGLWidget->Refresh();
    }
}

void TextureBrowser::textureModeChanged()
{
    queueDraw();
}

void TextureBrowser::setScaleFromRegistry()
{
    int index = registry::getValue<int>(RKEY_TEXTURE_SCALE);

    switch (index) {
        case 0: _textureScale = 10; break;
        case 1: _textureScale = 25; break;
        case 2: _textureScale = 50; break;
        case 3: _textureScale = 100; break;
        case 4: _textureScale = 200; break;
    };

    queueDraw();
}

void TextureBrowser::clearFilter()
{
	_filter->SetValue("");
    queueDraw();
}

void TextureBrowser::filterChanged()
{
    if (_filterIsIncremental)
	{
        queueDraw();
	}
}

void TextureBrowser::keyChanged()
{
    _hideUnused = registry::getValue<bool>(RKEY_TEXTURES_HIDE_UNUSED);
    _showTextureFilter = registry::getValue<bool>(RKEY_TEXTURE_SHOW_FILTER);
    _uniformTextureSize = registry::getValue<int>(RKEY_TEXTURE_UNIFORM_SIZE);
    _showTextureScrollbar = registry::getValue<bool>(RKEY_TEXTURE_SHOW_SCROLLBAR);
    _mouseWheelScrollIncrement = registry::getValue<int>(RKEY_TEXTURE_MOUSE_WHEEL_INCR);
	_resizeTextures = registry::getValue<bool>(RKEY_TEXTURES_CLAMP_TO_UNIFORM_SIZE);

    if (_showTextureScrollbar)
    {
        _scrollbar->Show();
    }
    else
    {
        _scrollbar->Hide();
    }

    if (_showTextureFilter)
    {
        _filter->Show();
    }
    else
    {
        _filter->Hide();
    }

    setScaleFromRegistry();

    heightChanged();
    _originInvalid = true;
}

// Return the display width of a texture in the texture browser
int TextureBrowser::getTextureWidth(const Texture& tex) const
{
    int width;

    if (!_resizeTextures)
    {
        // Don't use uniform size
        width = static_cast<int>(tex.getWidth() * (static_cast<float>(_textureScale) / 100));
    }
    else if (tex.getWidth() >= tex.getHeight())
    {
        // Texture is square, or wider than it is tall
        width = _uniformTextureSize;
    }
    else
    {
        // Otherwise, preserve the texture's aspect ratio
        width = static_cast<int>(
            _uniformTextureSize
            * (static_cast<float>(tex.getWidth()) / tex.getHeight())
        );
    }

    return width;
}

int TextureBrowser::getTextureHeight(const Texture& tex) const
{
    int height;

    if (!_resizeTextures)
    {
        // Don't use uniform size
        height = static_cast<int>(tex.getHeight() * (static_cast<float>(_textureScale) / 100));
    }
    else if (tex.getHeight() >= tex.getWidth())
    {
        // Texture is square, or taller than it is wide
        height = _uniformTextureSize;
    }
    else
    {
        // Otherwise, preserve the texture's aspect ratio
        height = static_cast<int>(
            _uniformTextureSize
            * (static_cast<float>(tex.getHeight()) / tex.getWidth())
        );
    }

    return height;
}

const std::string& TextureBrowser::getSelectedShader() const
{
    return _shader;
}

std::string TextureBrowser::getFilter()
{
	wxString value = _filter->GetValue();
    return std::string(value.begin(), value.end());
}

void TextureBrowser::setSelectedShader(const std::string& newShader)
{
    _shader = newShader;
    focus(_shader);
}

// Data structure keeping track of the virtual position for the next texture to
// be drawn in. Only the getPositionForTexture() method should access the values
// in this structure.
class TextureBrowser::CurrentPosition
{
public:

    CurrentPosition()
    : origin(VIEWPORT_BORDER, -VIEWPORT_BORDER), rowAdvance(0)
    { }

    Vector2i origin;
    int rowAdvance;
};

BasicVector2<int> TextureBrowser::getPositionForTexture(
    CurrentPosition& currentPos, const Texture& tex
) const
{
    int nWidth = getTextureWidth(tex);
    int nHeight = getTextureHeight(tex);

    // Wrap to the next row if there is not enough horizontal space for this
    // texture
    if (currentPos.origin.x() + nWidth > _viewportSize.x() - VIEWPORT_BORDER
        && currentPos.rowAdvance != 0)
    {
        currentPos.origin.x() = VIEWPORT_BORDER;
        currentPos.origin.y() -= currentPos.rowAdvance
                                 + FONT_HEIGHT() + TILE_BORDER;
        currentPos.rowAdvance = 0;
    }

    // Is our texture larger than the row? If so, grow the row height to match
    // it
    if (currentPos.rowAdvance < nHeight)
    {
        currentPos.rowAdvance = nHeight;
    }

    // Store the coordinates where the texture should be placed
    Vector2i texPos = currentPos.origin;

    // Advance the horizontal position for the next texture
    currentPos.origin.x() += std::max(96, nWidth) + 16;

    return texPos;
}

// if texture_showinuse jump over non in-use textures
bool TextureBrowser::shaderIsVisible(const MaterialPtr& shader)
{
    if (shader == NULL)
    {
        return false;
    }

    if (!boost::algorithm::istarts_with(shader->getName(), GlobalTexturePrefix_get()))
    {
        return false;
    }

    if (_hideUnused && !shader->IsInUse())
    {
        return false;
    }

    if (!getFilter().empty())
    {
        std::string textureNameCache(shader->getName());
        const char* textureName = shader_get_textureName(textureNameCache.c_str()); // can't use temporary shader->getName() here

        if (_filterIgnoresTexturePath)
        {
            boost::iterator_range<const char*> lastSlash = boost::find_last(textureName, "/");
            if (lastSlash)
            {
                textureName = lastSlash.end();
            }
        }

        // case insensitive substring match
        if ( !boost::ifind_first(textureName, getFilter().c_str()) )
            return false;
    }

    return true;
}

void TextureBrowser::heightChanged()
{
    _heightChanged = true;

    updateScroll();
    queueDraw();
}

void TextureBrowser::evaluateHeight()
{
    // greebo: Let the texture browser re-evaluate the scrollbar each frame
   _heightChanged = false;
   _entireSpaceHeight = 0;

   if (GlobalMaterialManager().isRealised())
   {
       class HeightWalker :
            public shaders::ShaderVisitor
        {
        private:
            TextureBrowser& _browser;
            CurrentPosition _layout;
            int _totalHeight;

        public:
            HeightWalker(TextureBrowser& browser) :
                _browser(browser),
                _totalHeight(0)
            {}

            void visit(const MaterialPtr& shader)
            {
                if (!_browser.shaderIsVisible(shader))
                {
                    return;
                }

                Vector2i pos = _browser.getPositionForTexture(
                    _layout, *shader->getEditorImage()
                );

                _totalHeight = std::max(
                    _totalHeight,
                    abs(pos.y())
                        + FONT_HEIGHT()
                        + _browser.getTextureHeight(*shader->getEditorImage())
                        + TILE_BORDER
                );
            }

            int getTotalHeight()
            {
                return _totalHeight;
            }
        } _walker(*this);

        GlobalMaterialManager().foreachShader(_walker);

        _entireSpaceHeight = _walker.getTotalHeight();
   }
}

int TextureBrowser::getTotalHeight()
{
    evaluateHeight();
    return _entireSpaceHeight;
}

void TextureBrowser::clampOriginY()
{
    if (_viewportOriginY > 0)
    {
        _viewportOriginY = 0;
    }

    int lower = std::min(_viewportSize.y() - getTotalHeight(), 0);

    if (_viewportOriginY < lower)
    {
        _viewportOriginY = lower;
    }
}

int TextureBrowser::getOriginY()
{
    if (_originInvalid)
    {
        _originInvalid = false;
        clampOriginY();
        updateScroll();
    }

    return _viewportOriginY;
}

void TextureBrowser::setOriginY(int newOriginY)
{
    _viewportOriginY = newOriginY;
    clampOriginY();
    updateScroll();
    queueDraw();
}

void TextureBrowser::onActiveShadersChanged()
{
    heightChanged();
    _originInvalid = true;
}

// Static command target
void TextureBrowser::toggle(const cmd::ArgumentList& args)
{
    GlobalGroupDialog().togglePage("textures");
}

//++timo NOTE: this is a mix of Shader module stuff and texture explorer
// it might need to be split in parts or moved out .. dunno
// scroll origin so the specified texture is completely on screen
// if current texture is not displayed, nothing is changed
void TextureBrowser::focus(const std::string& name)
{
    // scroll origin so the texture is completely on screen
  
    class FocusWalker :
        public shaders::ShaderVisitor
    {
    private:
        TextureBrowser& _browser;
        CurrentPosition _layout;
        int _x;
        int _y;
        const std::string& _name;

    public:
        FocusWalker(TextureBrowser& browser, const std::string& name) :
            _browser(browser),
            _name(name)
        {}

        void visit(const MaterialPtr& shader)
        {
            if (!_browser.shaderIsVisible(shader))
            {
                return;
            }

            Vector2i vec = _browser.getPositionForTexture(
                _layout, *shader->getEditorImage()
            );
            _x = vec.x();
            _y = vec.y();

            TexturePtr q = shader->getEditorImage();
            
            if (!q) return;

            // we have found when texdef->name and the shader name match
            // NOTE: as everywhere else for our comparisons, we are not case sensitive
            if (shader_equal(_name, shader->getName()))
            {
                int textureHeight = _browser.getTextureHeight(*q)
                                     + 2 * FONT_HEIGHT();

                int originy = _browser.getOriginY();

                if (_y > originy)
                {
                    originy = _y;
                }

                if (_y - textureHeight < originy - _browser.getViewportHeight())
                {
                    originy = (_y - textureHeight) + _browser.getViewportHeight();
                }

                _browser.setOriginY(originy);
            }
        }
    } _walker(*this, name);

    GlobalMaterialManager().foreachShader(_walker);
}

MaterialPtr TextureBrowser::getShaderAtCoords(int mx, int my)
{
    my += getOriginY() - _viewportSize.y();

    class MaterialFinder :
        public shaders::ShaderVisitor
    {
    private:
        TextureBrowser& _browser;
        CurrentPosition _layout;
        int _x;
        int _y;
        MaterialPtr _material;

    public:
        MaterialFinder(TextureBrowser& browser, int x, int y) :
            _browser(browser),
            _x(x),
            _y(y)
        {}

        void visit(const MaterialPtr& shader)
        {
            if (!_browser.shaderIsVisible(shader))
            {
                return;
            }

            Vector2i vec = _browser.getPositionForTexture(
                _layout, *shader->getEditorImage()
            );

            TexturePtr tex = shader->getEditorImage();

            if (tex == NULL)
            {
                return;
            }

            int nWidth = _browser.getTextureWidth(*tex);
            int nHeight = _browser.getTextureHeight(*tex);

            if (   _x > vec.x()
                && _x - vec.x() < nWidth
                && _y < vec.y()
                && vec.y() - _y < nHeight + FONT_HEIGHT())
            {
                _material = shader;
            }
        }

        MaterialPtr getMaterial()
        {
            return _material;
        }
    } _walker(*this, mx, my);

    GlobalMaterialManager().foreachShader(_walker);

    return _walker.getMaterial();
}

void TextureBrowser::selectTextureAt(int mx, int my)
{
    MaterialPtr shader = getShaderAtCoords(mx, my);

    if (shader != NULL)
    {
        setSelectedShader(shader->getName());

        // Apply the shader to the current selection
        selection::algorithm::applyShaderToSelection(shader->getName());

		// Copy the shader name to the clipboard too
		GlobalShaderClipboard().setSource(shader->getName());
    }
}

void TextureBrowser::draw()
{
	if (_viewportSize.x() == 0 || _viewportSize.y() == 0)
	{
		return;
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

    GlobalOpenGL().assertNoErrors();

    Vector3 colorBackground = ColourSchemes().getColour("texture_background");
    glClearColor(colorBackground[0], colorBackground[1], colorBackground[2], 0);
    glViewport(0, 0, _viewportSize.x(), _viewportSize.y());
    
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable (GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glOrtho(0, _viewportSize.x(), getOriginY() - _viewportSize.y(), getOriginY(), -100, 100);

	GlobalOpenGL().assertNoErrors();

    glEnable (GL_TEXTURE_2D);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

    // Visitor class to render all textures onto tiles
    class TextureTileRenderer : public shaders::ShaderVisitor
    {
        TextureBrowser& _browser;
        CurrentPosition _layout;
        bool _hideUnused;
        unsigned int _maxNameLength;

    private:

        void drawBorder(const Material& material,
                        const Vector2i& pos,
                        const Vector2i& size)
        {
            // borders rules:
            // if it's the current texture, draw a thick red line, else:
            // shaders have a white border, simple textures don't
            // if !texture_showinuse: (some textures displayed may not be in use)
            // draw an additional square around with 0.5 1 0.5 color
            if (shader_equal(_browser.getSelectedShader(), material.getName()))
            {
                glLineWidth(3);
                glColor3f(1,0,0);
                glDisable(GL_TEXTURE_2D);

                glBegin(GL_LINE_LOOP);
                glVertex2i(pos.x() - TILE_BORDER,
                           pos.y() - FONT_HEIGHT() + TILE_BORDER);
                glVertex2i(pos.x() - TILE_BORDER,
                           pos.y() - FONT_HEIGHT() - size.y() - TILE_BORDER);
                glVertex2i(pos.x() + TILE_BORDER + size.x(),
                           pos.y() - FONT_HEIGHT() - size.y() - TILE_BORDER);
                glVertex2i(pos.x() + TILE_BORDER + size.x(),
                           pos.y() - FONT_HEIGHT() + TILE_BORDER);
                glEnd();

                glEnable (GL_TEXTURE_2D);
                glLineWidth (1);
            }
            else
            {
                glLineWidth(1);

                // material border:
                if (!material.IsDefault())
                {
                    glColor3f(1,1,1);
                    glDisable(GL_TEXTURE_2D);

                    glBegin(GL_LINE_LOOP);
                    glVertex2i(pos.x() - 1,
                               pos.y() + 1 - FONT_HEIGHT());
                    glVertex2i(pos.x() - 1,
                               pos.y() - size.y() - 1 - FONT_HEIGHT());
                    glVertex2i(pos.x() + 1 + size.x(),
                               pos.y() - size.y() - 1 - FONT_HEIGHT());
                    glVertex2i(pos.x() + 1 + size.x(),
                               pos.y() + 1 - FONT_HEIGHT());
                    glEnd();
                    glEnable (GL_TEXTURE_2D);
                }

                // highlight in-use textures
                if (!_hideUnused && material.IsInUse())
                {
                    glColor3f(0.5f, 1 ,0.5f);
                    glDisable(GL_TEXTURE_2D);
                    glBegin(GL_LINE_LOOP);
                    glVertex2i(pos.x() - 3,
                               pos.y() + 3 - FONT_HEIGHT());
                    glVertex2i(pos.x() - 3,
                               pos.y() - size.y() - 3 - FONT_HEIGHT());
                    glVertex2i(pos.x() + 3 + size.x(),
                               pos.y() - size.y() - 3 - FONT_HEIGHT());
                    glVertex2i(pos.x() + 3 + size.x(),
                               pos.y() + 3 - FONT_HEIGHT());
                    glEnd();
                    glEnable (GL_TEXTURE_2D);
                }
            }
        }

        void drawTextureQuad(GLuint num,
                             const Vector2i& pos,
                             const Vector2i& size)
        {
            glBindTexture(GL_TEXTURE_2D, num);
            GlobalOpenGL().assertNoErrors();
            glColor3f(1,1,1);

            glBegin(GL_QUADS);
            glTexCoord2i(0,0);
            glVertex2i(pos.x(), pos.y() - FONT_HEIGHT());
            glTexCoord2i(1,0);
            glVertex2i(pos.x() + size.x(), pos.y() - FONT_HEIGHT());
            glTexCoord2i(1,1);
            glVertex2i(pos.x() + size.x(), pos.y() - FONT_HEIGHT() - size.y());
            glTexCoord2i (0,1);
            glVertex2i(pos.x(), pos.y() - FONT_HEIGHT() - size.y());
            glEnd();
        }

        void drawTextureName(const Material& material,
                             const Vector2i& pos,
                             const Vector2i& size)
        {
            glDisable (GL_TEXTURE_2D);
            glColor3f (1,1,1);

            const static int FONT_OFFSET = 6;
            glRasterPos2i (pos.x(), pos.y() - FONT_HEIGHT() + FONT_OFFSET);

            // don't draw the directory name
            std::string name = material.getName();
            name = name.substr(name.rfind("/") + 1);

            // Ellipsize the name if it's too long
            if (name.size() > _maxNameLength)
            {
                name = name.substr(0, _maxNameLength/2) +
                    "..." +
                    name.substr(name.size() - _maxNameLength/2);
            }

            GlobalOpenGL().drawString(name);
            glEnable(GL_TEXTURE_2D);
        }

    public:
        TextureTileRenderer(TextureBrowser& browser, bool hideUnused) :
            _browser(browser),
            _hideUnused(hideUnused),
            _maxNameLength(registry::getValue<int>(RKEY_TEXTURE_MAX_NAME_LENGTH))
        {}

        void visit(const MaterialPtr& material)
        {
            if (!_browser.shaderIsVisible(material))
            {
                return;
            }

            TexturePtr q = material->getEditorImage();
            if (!q) return;

            Vector2i pos = _browser.getPositionForTexture(_layout, *q);
            Vector2i size(_browser.getTextureWidth(*q),
                          _browser.getTextureHeight(*q));

            // Is this texture visible?
            if ((pos.y() - size.y() - FONT_HEIGHT() < _browser.getOriginY()) && 
                (pos.y() > _browser.getOriginY() - _browser.getViewportHeight()))
            {
                drawBorder(*material, pos, size);
                drawTextureQuad(q->getGLTexNum(), pos, size);
                drawTextureName(*material, pos, size);
            }
        }

    } _walker(*this, _hideUnused);

    GlobalMaterialManager().foreachShader(_walker);

	GlobalOpenGL().assertNoErrors();

    // reset the current texture
    glBindTexture(GL_TEXTURE_2D, 0);
    GlobalOpenGL().assertNoErrors();

	glPopAttrib();
}

void TextureBrowser::doMouseWheel(bool wheelUp)
{
    int originy = getOriginY();

    if (wheelUp) {
        originy += static_cast<int>(_mouseWheelScrollIncrement);
    }
    else {
        originy -= static_cast<int>(_mouseWheelScrollIncrement);
    }

    setOriginY(originy);
}

bool TextureBrowser::checkSeekInMediaBrowser()
{
    if (_popupX > 0 && _popupY > 0)
    {
        MaterialPtr shader = getShaderAtCoords(_popupX, _popupY);

        return (shader != NULL);
    }

    return false;
}

void TextureBrowser::openContextMenu() {

    std::string shaderText = _("No shader");

    if (_popupX > 0 && _popupY > 0)
    {
        MaterialPtr shader = getShaderAtCoords(_popupX, _popupY);

        if (shader != NULL)
        {
            shaderText = shader->getName();
            shaderText = shaderText.substr(shaderText.rfind("/")+1);
        }
    }

	_shaderLabel->SetText(shaderText);

    _popupMenu->show(_wxGLWidget);
}

void TextureBrowser::onSeekInMediaBrowser()
{
    if (_popupX > 0 && _popupY > 0)
    {
        MaterialPtr shader = getShaderAtCoords(_popupX, _popupY);

        if (shader != NULL)
        {
            // Focus the MediaBrowser selection to the given shader
            GlobalGroupDialog().setPage("mediabrowser");
            MediaBrowser::getInstance().setSelection(shader->getName());
        }
    }

    _popupX = -1;
    _popupY = -1;
}

void TextureBrowser::onFrozenMouseMotion(int x, int y, unsigned int state)
{
    if (y != 0)
    {
        int scale = 1;

		if ((state & wxutil::Modifier::SHIFT) != 0)
        {
            scale = 4;
        }

        int originy = getOriginY();
        originy += y * scale;

        setOriginY(originy);
    }
}

void TextureBrowser::onFrozenMouseCaptureLost()
{
	_freezePointer.endCapture();
}

void TextureBrowser::scrollChanged(double value)
{
    setOriginY(-static_cast<int>(value));
}

void TextureBrowser::updateScroll()
{
    if (_showTextureScrollbar)
    {
        int totalHeight = getTotalHeight();

        totalHeight = std::max(totalHeight, _viewportSize.y());

		_scrollbar->SetScrollbar(-getOriginY(), _viewportSize.y(), totalHeight, _viewportSize.y());
    }
}

int TextureBrowser::getViewportHeight()
{
    return _viewportSize.y();
}

wxWindow* TextureBrowser::constructWindow(wxWindow* parent)
{
    GlobalMaterialManager().addActiveShadersObserver(shared_from_this());

	wxPanel* hbox = new wxPanel(parent, wxID_ANY);
	hbox->SetSizer(new wxBoxSizer(wxHORIZONTAL));

	wxPanel* texbox = new wxPanel(hbox, wxID_ANY);
	texbox->SetSizer(new wxBoxSizer(wxVERTICAL));

	// Load the texture toolbar from the registry
	{
        IToolbarManager& tbCreator = GlobalUIManager().getToolbarManager();

		_textureToolbar = tbCreator.getToolbar("texture", texbox);

		if (_textureToolbar != NULL)
		{
			// Pack it into the main window
			texbox->GetSizer()->Add(_textureToolbar, 0, wxEXPAND);
		}
		else
		{
			rWarning() << "TextureBrowser: Cannot instantiate texture toolbar!" << std::endl;
		}
	}

	// Filter text entry
	{
        _filter = new wxutil::NonModalEntry(texbox,
            std::bind(&TextureBrowser::queueDraw, this),
            std::bind(&TextureBrowser::clearFilter, this),
            std::bind(&TextureBrowser::filterChanged, this),
            false);

		texbox->GetSizer()->Add(_filter, 0, wxEXPAND);

        if (_showTextureFilter)
        {
            _filter->Show();
        }
        else
        {
            _filter->Hide();
        }
    }

	// GL drawing area
	{
		_wxGLWidget = new wxutil::GLWidget(texbox, std::bind(&TextureBrowser::onRender, this), "TextureBrowser");

		_wxGLWidget->Connect(wxEVT_SIZE, wxSizeEventHandler(TextureBrowser::onGLResize), NULL, this);
		_wxGLWidget->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TextureBrowser::onGLMouseScroll), NULL, this);

		_wxGLWidget->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(TextureBrowser::onGLMouseButtonPress), NULL, this);
        _wxGLWidget->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(TextureBrowser::onGLMouseButtonPress), NULL, this);
		_wxGLWidget->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(TextureBrowser::onGLMouseButtonRelease), NULL, this);
		_wxGLWidget->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(TextureBrowser::onGLMouseButtonPress), NULL, this);
        _wxGLWidget->Connect(wxEVT_RIGHT_DCLICK, wxMouseEventHandler(TextureBrowser::onGLMouseButtonPress), NULL, this);
		_wxGLWidget->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(TextureBrowser::onGLMouseButtonRelease), NULL, this);

		texbox->GetSizer()->Add(_wxGLWidget, 1, wxEXPAND);
	}

	hbox->GetSizer()->Add(texbox, 1, wxEXPAND);

	// Scrollbar
    {
        _scrollbar = new wxScrollBar(hbox, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);
        _scrollbar->Connect(wxEVT_SCROLL_CHANGED,
			wxScrollEventHandler(TextureBrowser::onScrollChanged), NULL, this);
		_scrollbar->Connect(wxEVT_SCROLL_THUMBTRACK,
			wxScrollEventHandler(TextureBrowser::onScrollChanged), NULL, this);

		hbox->GetSizer()->Add(_scrollbar, 0, wxEXPAND);

        if (_showTextureScrollbar)
        {
            _scrollbar->Show();
        }
        else
        {
            _scrollbar->Hide();
        }
    }

    updateScroll();

	return hbox;
}

void TextureBrowser::destroyWindow()
{
	if (_textureToolbar != NULL)
	{
		GlobalEventManager().disconnectToolbar(_textureToolbar);
		_textureToolbar = NULL;
	}

    GlobalMaterialManager().removeActiveShadersObserver(shared_from_this());

    _wxGLWidget = NULL;
    _scrollbar = NULL;
    _filter = NULL;
}

void TextureBrowser::registerPreferencesPage()
{
    // Add a page to the given group
    PreferencesPagePtr page = GlobalPreferenceSystem().getPage(_("Settings/Texture Browser"));

    // Create the string list containing the texture scalings
    std::list<std::string> textureScaleList;

    textureScaleList.push_back("10%");
    textureScaleList.push_back("25%");
    textureScaleList.push_back("50%");
    textureScaleList.push_back("100%");
    textureScaleList.push_back("200%");

    page->appendCombo(_("Texture Thumbnail Scale"), RKEY_TEXTURE_SCALE, textureScaleList);

    page->appendEntry(_("Uniform texture thumbnail size (pixels)"), RKEY_TEXTURE_UNIFORM_SIZE);
    page->appendCheckBox("", _("Texture scrollbar"), RKEY_TEXTURE_SHOW_SCROLLBAR);
    page->appendEntry(_("Mousewheel Increment"), RKEY_TEXTURE_MOUSE_WHEEL_INCR);
    page->appendSpinner(_("Max shadername length"), RKEY_TEXTURE_MAX_NAME_LENGTH, 4, 100, 1);

    page->appendCheckBox("", _("Show Texture Filter"), RKEY_TEXTURE_SHOW_FILTER);
}

void TextureBrowser::construct()
{
	GlobalEventManager().addRegistryToggle("TextureThumbsUniform", RKEY_TEXTURES_CLAMP_TO_UNIFORM_SIZE);
    GlobalEventManager().addRegistryToggle("ShowInUse", RKEY_TEXTURES_HIDE_UNUSED);
    GlobalCommandSystem().addCommand("ViewTextures", TextureBrowser::toggle);
    GlobalEventManager().addCommand("ViewTextures", "ViewTextures");

    TextureBrowser::registerPreferencesPage();
}

void TextureBrowser::destroy()
{
    InstancePtr().reset();
}

TextureBrowser& TextureBrowser::Instance()
{
    TextureBrowserPtr& instancePtr = InstancePtr();

    if (instancePtr == NULL)
    {
        instancePtr.reset(new TextureBrowser);
    }

    return *instancePtr;
}

TextureBrowserPtr& TextureBrowser::InstancePtr()
{
    static TextureBrowserPtr _instance;
    return _instance;
}

void TextureBrowser::update()
{
    heightChanged();
}

void TextureBrowser::onScrollChanged(wxScrollEvent& ev)
{
	setOriginY(-ev.GetPosition());
	queueDraw();
}

void TextureBrowser::onGLResize(wxSizeEvent& ev)
{
	_viewportSize = Vector2i(ev.GetSize().GetWidth(), ev.GetSize().GetHeight());

    heightChanged();
    _originInvalid = true;
    queueDraw();

	ev.Skip();
}

void TextureBrowser::onGLMouseScroll(wxMouseEvent& ev)
{
	if (ev.GetWheelRotation() > 0)
	{
		doMouseWheel(true);
	}
	else if (ev.GetWheelRotation() < 0)
	{
		doMouseWheel(false);
	}
}

void TextureBrowser::onGLMouseButtonPress(wxMouseEvent& ev)
{
	if (ev.RightDown())
    {
        _freezePointer.startCapture(_wxGLWidget, 
			std::bind(&TextureBrowser::onFrozenMouseMotion, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			std::bind(&TextureBrowser::onFrozenMouseCaptureLost, this));

        // Store the coords of the mouse pointer for later reference
        _popupX = static_cast<int>(ev.GetX());
        _popupY = _viewportSize.y() - 1 - static_cast<int>(ev.GetY());
        _startOrigin = _viewportOriginY;
    }
	else if (ev.LeftDown())
    {
        selectTextureAt(
            static_cast<int>(ev.GetX()),
            _viewportSize.y() - 1 - static_cast<int>(ev.GetY())
        );
    }
}

void TextureBrowser::onGLMouseButtonRelease(wxMouseEvent& ev)
{
	if (ev.RightUp())
    {
		_freezePointer.endCapture();

		// See how much we've been scrolling since mouseDown
		int delta = abs(_viewportOriginY - _startOrigin);

		if (delta <= _epsilon)
		{
			openContextMenu();
		}

		_startOrigin = -1;
    }
}

void TextureBrowser::onRender()
{
	GlobalOpenGL().assertNoErrors();

    evaluateHeight();
    updateScroll();
    draw();

    GlobalOpenGL().assertNoErrors();
}

} // namespace ui

/** greebo: The accessor method, use this to call non-static TextureBrowser methods
 */
ui::TextureBrowser& GlobalTextureBrowser()
{
    return ui::TextureBrowser::Instance();
}
