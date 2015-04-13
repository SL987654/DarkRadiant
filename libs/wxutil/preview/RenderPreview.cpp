#include "RenderPreview.h"

#include "ifilter.h"
#include "i18n.h"
#include "igl.h"
#include "iscenegraphfactory.h"
#include "irendersystemfactory.h"
#include "iuimanager.h"

#include "math/AABB.h"
#include "util/ScopedBoolLock.h"

#include "../GLWidget.h"
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/dcclient.h>

#include <functional>

namespace wxutil
{

namespace
{
    const GLfloat PREVIEW_FOV = 60;
    const unsigned int MSEC_PER_FRAME = 16;

    // Widget names
    const std::string BOTTOM_BOX("bottomBox");
    const std::string PAUSE_BUTTON("pauseButton");
    const std::string STOP_BUTTON("stopButton");
}

RenderPreview::RenderPreview(wxWindow* parent, bool enableAnimation) :
    _mainPanel(loadNamedPanel(parent, "RenderPreviewPanel")),
	_glWidget(new wxutil::GLWidget(_mainPanel, std::bind(&RenderPreview::drawPreview, this), "RenderPreview")),
    _renderSystem(GlobalRenderSystemFactory().createRenderSystem()),
    _sceneWalker(_renderer, _volumeTest),
    _renderingInProgress(false),
    _timer(this),
    _previewWidth(0),
    _previewHeight(0),
    _filtersMenu(GlobalUIManager().createFilterMenu())
{
	Connect(wxEVT_TIMER, wxTimerEventHandler(RenderPreview::_onFrame), NULL, this);

    // Insert GL widget
	_mainPanel->GetSizer()->Prepend(_glWidget, 1, wxEXPAND);

	_glWidget->Connect(wxEVT_SIZE, wxSizeEventHandler(RenderPreview::onSizeAllocate), NULL, this);
	_glWidget->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(RenderPreview::onGLScroll), NULL, this);
	_glWidget->Connect(wxEVT_MOTION, wxMouseEventHandler(RenderPreview::onGLMotion), NULL, this);
	_glWidget->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(RenderPreview::onGLMouseClick), NULL, this);
    _glWidget->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(RenderPreview::onGLMouseClick), NULL, this);

	wxToolBar* toolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewAnimToolbar");

	_toolbarSizer = toolbar->GetContainingSizer();

	// Set up the toolbar
    if (enableAnimation)
    {
        connectToolbarSignals();
    }
    else
    {
		toolbar->Hide();
    }

	// Connect filters menu to toolbar
	wxToolBar* filterToolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewFilterToolbar");

	wxMenu* filterSubmenu = _filtersMenu->getMenuWidget();

	wxToolBarToolBase* filterTool = filterToolbar->AddTool(wxID_ANY, _("Filters"), 
		wxArtProvider::GetBitmap(GlobalUIManager().ArtIdPrefix() + "iconFilter16.png"), 
		_("Filters"), wxITEM_DROPDOWN);
	filterToolbar->SetDropdownMenu(filterTool->GetId(), filterSubmenu);

	filterToolbar->Realize();

    // Get notified of filter changes
    GlobalFilterSystem().filtersChangedSignal().connect(
        sigc::mem_fun(this, &RenderPreview::filtersChanged)
    );
}

void RenderPreview::connectToolbarSignals()
{
	wxToolBar* toolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewAnimToolbar");

	toolbar->Connect(getToolBarToolByLabel(toolbar, "startTimeButton")->GetId(), 
		wxEVT_TOOL, wxCommandEventHandler(RenderPreview::onStartPlaybackClick), NULL, this);
	toolbar->Connect(getToolBarToolByLabel(toolbar, "pauseTimeButton")->GetId(), 
		wxEVT_TOOL, wxCommandEventHandler(RenderPreview::onPausePlaybackClick), NULL, this);
	toolbar->Connect(getToolBarToolByLabel(toolbar, "stopTimeButton")->GetId(), 
		wxEVT_TOOL, wxCommandEventHandler(RenderPreview::onStopPlaybackClick), NULL, this);

	toolbar->Connect(getToolBarToolByLabel(toolbar, "prevButton")->GetId(), 
		wxEVT_TOOL, wxCommandEventHandler(RenderPreview::onStepBackClick), NULL, this);
	toolbar->Connect(getToolBarToolByLabel(toolbar, "nextButton")->GetId(), 
		wxEVT_TOOL, wxCommandEventHandler(RenderPreview::onStepForwardClick), NULL, this);
}

RenderPreview::~RenderPreview()
{
    _timer.Stop();
}

void RenderPreview::filtersChanged()
{
    if (!getScene()->root()) return;

    GlobalFilterSystem().updateSubgraph(getScene()->root());
    queueDraw();
}

void RenderPreview::addToolbar(wxToolBar* toolbar)
{
	_toolbarSizer->Add(toolbar, 0, wxEXPAND);
}

void RenderPreview::queueDraw()
{
	_glWidget->Refresh();
}

void RenderPreview::setSize(int width, int height)
{
	_glWidget->SetClientSize(width, height);
}

void RenderPreview::initialisePreview()
{
#if 0
#ifdef WIN32
    // greebo: Unfortunate hack to fix the grey GL renderviews in the EntityChooser
    // and other windows that are hidden instead of destroyed when closed.
    Gtk::Container* container = get_parent();
    bool wasShown = get_visible();

    if (container != NULL)
    {
        if (wasShown)
        {
            hide();
        }

        container->remove(*this);
        container->add(*this);

        if (wasShown)
        {
            show();
        }
    }

#endif
#endif
	// Grab the GL widget with sentry object
	wxPaintDC dc(_glWidget);
	_glWidget->SetCurrent(GlobalOpenGL().getwxGLContext());
	
    // Set up the camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(PREVIEW_FOV, 1, 0.1, 10000);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Set up the lights
    glEnable(GL_LIGHTING);

    glEnable(GL_LIGHT0);
    GLfloat l0Amb[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat l0Dif[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat l0Pos[] = { 1.0f, 1.0f, 1.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, l0Amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, l0Dif);
    glLightfv(GL_LIGHT0, GL_POSITION, l0Pos);

    glEnable(GL_LIGHT1);
    GLfloat l1Dif[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat l1Pos[] = { 0.0, 0.0, 1.0, 0.0 };
    glLightfv(GL_LIGHT1, GL_DIFFUSE, l1Dif);
    glLightfv(GL_LIGHT1, GL_POSITION, l1Pos);

    if (_renderSystem->shaderProgramsAvailable())
    {
        _renderSystem->setShaderProgram(
            RenderSystem::SHADER_PROGRAM_INTERACTION
        );
    }
}

const scene::GraphPtr& RenderPreview::getScene()
{
    if (!_scene)
    {
        _scene = GlobalSceneGraphFactory().createSceneGraph();

        setupSceneGraph();

        associateRenderSystem();
    }

    return _scene;
}

void RenderPreview::setupSceneGraph()
{
    // Set our render time to 0
    _renderSystem->setTime(0);
}

void RenderPreview::associateRenderSystem()
{
    if (_scene && _scene->root())
    {
        _scene->root()->setRenderSystem(_renderSystem);
    }
}

Matrix4 RenderPreview::getProjectionMatrix(float near_z, float far_z, float fieldOfView, int width, int height)
{
    const float half_width = near_z * tan(degrees_to_radians(fieldOfView * 0.5f));
    const float half_height = half_width * (static_cast<float>(height) / static_cast<float>(width));

    return Matrix4::getProjectionForFrustum(
        -half_width,
        half_width,
        -half_height,
        half_height,
        near_z,
        far_z
    );
}

Matrix4 RenderPreview::getModelViewMatrix()
{
    // Premultiply with the translations
    Matrix4 modelview;

    {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glTranslatef(0, 0, _camDist); // camera translation
        glMultMatrixd(_rotation); // post multiply with rotations

        // Load the matrix from openGL
        glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    }

    return modelview;
}

void RenderPreview::startPlayback()
{
	if (_timer.IsRunning())
    {
        // Timer is already running, just reset the preview time
        _renderSystem->setTime(0);
    }
    else
    {
        // Timer is not enabled, we're paused or stopped
        _timer.Start();
    }

	wxToolBar* toolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewAnimToolbar");

	toolbar->EnableTool(getToolBarToolByLabel(toolbar, "pauseTimeButton")->GetId(), true);
	toolbar->EnableTool(getToolBarToolByLabel(toolbar, "stopTimeButton")->GetId(), true);
}

void RenderPreview::stopPlayback()
{
    _renderSystem->setTime(0);
    _timer.Stop();

	wxToolBar* toolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewAnimToolbar");

	toolbar->EnableTool(getToolBarToolByLabel(toolbar, "pauseTimeButton")->GetId(), false);
	toolbar->EnableTool(getToolBarToolByLabel(toolbar, "stopTimeButton")->GetId(), false);

    _glWidget->Refresh();
}

bool RenderPreview::onPreRender()
{
    return true;
}

RenderStateFlags RenderPreview::getRenderFlagsFill()
{
    return  RENDER_MASKCOLOUR |
            RENDER_ALPHATEST |
            RENDER_BLEND |
            RENDER_CULLFACE |
            RENDER_OFFSETLINE |
            RENDER_VERTEX_COLOUR |
            RENDER_FILL |
            RENDER_LIGHTING |
            RENDER_TEXTURE_2D |
            RENDER_SMOOTH |
            RENDER_SCALED |
            RENDER_FILL |
            RENDER_TEXTURE_CUBEMAP |
            RENDER_BUMP |
            RENDER_PROGRAM;
}

RenderStateFlags RenderPreview::getRenderFlagsWireframe()
{
    return RENDER_MASKCOLOUR |
           RENDER_ALPHATEST |
           RENDER_BLEND |
           RENDER_CULLFACE |
           RENDER_OFFSETLINE |
           RENDER_VERTEX_COLOUR |
           RENDER_LIGHTING |
           RENDER_SMOOTH |
           RENDER_SCALED |
           RENDER_TEXTURE_CUBEMAP |
           RENDER_BUMP |
           RENDER_PROGRAM;
}

void RenderPreview::drawPreview()
{
    if (_renderingInProgress) return; // avoid double-entering this method

    util::ScopedBoolLock lock(_renderingInProgress);

    glViewport(0, 0, _previewWidth, _previewHeight);

    // Set up the render and clear the drawing area in any case
    glDepthMask(GL_TRUE);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set up the camera
    Matrix4 projection = getProjectionMatrix(0.1f, 10000, PREVIEW_FOV, _previewWidth, _previewHeight);

    // Keep the modelview matrix in the volumetest class up to date
    _volumeTest.setModelView(getModelViewMatrix());
    _volumeTest.setProjection(projection);

    // Pre-Render event
    if (!onPreRender())
    {
        // a return value of false means to cancel rendering
        drawTime();
        return;
    }

    // Front-end render phase, collect OpenGLRenderable objects from the scene
    getScene()->foreachVisibleNodeInVolume(_volumeTest, _sceneWalker);

    RenderStateFlags flags = getRenderFlagsFill();

    // Launch the back end rendering
    _renderSystem->render(flags, _volumeTest.GetModelview(), projection);

    // Give subclasses an opportunity to render their own on-screen stuff
    onPostRender();

    // Draw the render time
    drawTime();
}

void RenderPreview::renderWireFrame()
{
    RenderStateFlags flags = getRenderFlagsWireframe();

    // Set up the camera
    Matrix4 projection = getProjectionMatrix(0.1f, 10000, PREVIEW_FOV, _previewWidth, _previewHeight);

    // Front-end render phase, collect OpenGLRenderable objects from the scene
    getScene()->foreachVisibleNodeInVolume(_volumeTest, _sceneWalker);

    // Launch the back end rendering
    _renderSystem->render(flags, _volumeTest.GetModelview(), projection);
}

void RenderPreview::onGLMouseClick(wxMouseEvent& ev)
{
	// Unset the focus on this GL preview, we don't want to 
	// catch mousewheel events all over the place
	wxWindow* parent = _glWidget->GetParent();

	while (parent != NULL && parent->GetParent() != NULL)
	{
		parent = parent->GetParent();
	}

	if (parent != NULL)
	{
		parent->SetFocus();
	}
}

void RenderPreview::onGLMotion(wxMouseEvent& ev)
{
	if (ev.LeftIsDown()) // dragging with mouse button
    {
        static double _lastX = ev.GetX();
        static double _lastY = ev.GetY();

        // Calculate the mouse delta as a vector in the XY plane, and store the
        // current position for the next event.
        Vector3 deltaPos(static_cast<float>(ev.GetX() - _lastX),
                         static_cast<float>(_lastY - ev.GetY()),
                         0);
        _lastX = ev.GetX();
        _lastY = ev.GetY();

        // Calculate the axis of rotation. This is the mouse vector crossed with the Z axis,
        // to give a rotation axis in the XY plane at right-angles to the mouse delta.
        static Vector3 _zAxis(0, 0, 1);
        Vector3 axisRot = deltaPos.crossProduct(_zAxis);

		// Grab the contex for this widget
		if (_glWidget->SetCurrent(GlobalOpenGL().getwxGLContext()))
        {
            // Premultiply the current modelview matrix with the rotation,
            // in order to achieve rotations in eye space rather than object
            // space. At this stage we are only calculating and storing the
            // matrix for the GLDraw callback to use.
            glLoadIdentity();
            glRotated(-2, axisRot.x(), axisRot.y(), axisRot.z());
            glMultMatrixd(_rotation);

            // Save the new GL matrix for GL draw
            glGetDoublev(GL_MODELVIEW_MATRIX, _rotation);

            _glWidget->Refresh(); // trigger the GLDraw method to draw the actual model
        }
    }
}

AABB RenderPreview::getSceneBounds()
{
    return AABB(Vector3(0,0,0), Vector3(64,64,64));
}

void RenderPreview::onGLScroll(wxMouseEvent& ev)
{
    // Scroll increment is a fraction of the AABB radius
    float inc = static_cast<float>(getSceneBounds().getRadius()) * 0.1f;

	if (ev.GetWheelRotation() > 0)
    {
        _camDist += inc;
    }
    else if (ev.GetWheelRotation() < 0)
    {
        _camDist -= inc;
    }

    if (!_renderingInProgress)
    {
        _glWidget->Refresh();
    }
}

void RenderPreview::onStartPlaybackClick(wxCommandEvent& ev)
{
	startPlayback();
}

void RenderPreview::onStopPlaybackClick(wxCommandEvent& ev)
{
	stopPlayback();
}

void RenderPreview::onPausePlaybackClick(wxCommandEvent& ev)
{
	// Disable the button
	wxToolBar* toolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewAnimToolbar");
	toolbar->EnableTool(getToolBarToolByLabel(toolbar, "pauseTimeButton")->GetId(), false);
	
	if (_timer.IsRunning())
    {
        _timer.Stop();
    }
    else
    {
        _timer.Start(); // re-enable playback
    }
}

void RenderPreview::onStepForwardClick(wxCommandEvent& ev)
{
    // Disable the button
	wxToolBar* toolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewAnimToolbar");
	toolbar->EnableTool(getToolBarToolByLabel(toolbar, "pauseTimeButton")->GetId(), false);

	if (_timer.IsRunning())
    {
        _timer.Stop();
    }

    _renderSystem->setTime(_renderSystem->getTime() + MSEC_PER_FRAME);
    _glWidget->Refresh();
}

void RenderPreview::onStepBackClick(wxCommandEvent& ev)
{
    // Disable the button
    wxToolBar* toolbar = findNamedObject<wxToolBar>(_mainPanel, "RenderPreviewAnimToolbar");
	toolbar->EnableTool(getToolBarToolByLabel(toolbar, "pauseTimeButton")->GetId(), false);

	if (_timer.IsRunning())
    {
        _timer.Stop();
    }

    if (_renderSystem->getTime() > 0)
    {
        _renderSystem->setTime(_renderSystem->getTime() - MSEC_PER_FRAME);
    }

    _glWidget->Refresh();
}

void RenderPreview::onSizeAllocate(wxSizeEvent& ev)
{
	_previewWidth = ev.GetSize().GetWidth();
    _previewHeight = ev.GetSize().GetHeight();
}

void RenderPreview::drawTime()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, static_cast<float>(_previewWidth), 0, static_cast<float>(_previewHeight), -100, 100);

    glScalef(1, -1, 1);
    glTranslatef(0, -static_cast<float>(_previewHeight), 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (GLEW_VERSION_1_3)
    {
        glClientActiveTexture(GL_TEXTURE0);
        glActiveTexture(GL_TEXTURE0);
    }

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_DEPTH_TEST);

    glColor3f( 1.f, 1.f, 1.f );
    glLineWidth(1);

    glRasterPos3f(1.0f, static_cast<float>(_previewHeight) - 1.0f, 0.0f);

    GlobalOpenGL().drawString((boost::format("%.3f sec.") % (_renderSystem->getTime() * 0.001f)).str());
}

void RenderPreview::_onFrame(wxTimerEvent& ev)
{
    if (!_renderingInProgress)
    {
        _renderSystem->setTime(_renderSystem->getTime() + MSEC_PER_FRAME);
        _glWidget->Refresh();
    }
}

} // namespace
