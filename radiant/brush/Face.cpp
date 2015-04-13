#include "Face.h"

#include "ivolumetest.h"
#include "ifilter.h"
#include "itextstream.h"
#include "irenderable.h"

#include "shaderlib.h"
#include "Winding.h"

#include "Brush.h"
#include "BrushNode.h"
#include "BrushModule.h"
#include "ui/surfaceinspector/SurfaceInspector.h"

// The structure that is saved in the undostack
class Face::SavedState :
    public IUndoMemento
{
public:
    FacePlane::SavedState _planeState;
    FaceTexdef::SavedState _texdefState;
    std::string _materialName;

    SavedState(const Face& face) :
        _planeState(face.getPlane()),
        _texdefState(face.getTexdef()),
        _materialName(face.getShader())
    {}

    virtual ~SavedState() {}

    void exportState(Face& face) const {
        _planeState.exportState(face.getPlane());
        face.setShader(_materialName);
        _texdefState.exportState(face.getTexdef());
    }
};

Face::Face(Brush& owner) :
    _owner(owner),
    _shader(texdef_name_default(), _owner.getBrushNode().getRenderSystem()),
    m_texdef(_shader, TextureProjection(), false),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    _shader.attachObserver(*this);
    m_plane.initialiseFromPoints(
        Vector3(0, 0, 0), Vector3(64, 0, 0), Vector3(0, 64, 0)
    );
    m_texdef.setBasis(m_plane.getPlane().normal());
    planeChanged();
    shaderChanged();
}

Face::Face(
    Brush& owner,
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& p2,
    const std::string& shader,
    const TextureProjection& projection
) :
    _owner(owner),
    _shader(shader, _owner.getBrushNode().getRenderSystem()),
    m_texdef(_shader, projection),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    _shader.attachObserver(*this);
    m_plane.initialiseFromPoints(p0, p1, p2);
    m_texdef.setBasis(m_plane.getPlane().normal());
    planeChanged();
    shaderChanged();
}

Face::Face(Brush& owner, const Plane3& plane) :
    _owner(owner),
    _shader("", _owner.getBrushNode().getRenderSystem()),
    m_texdef(_shader, TextureProjection()),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    _shader.attachObserver(*this);
    m_plane.setPlane(plane);
    m_texdef.setBasis(m_plane.getPlane().normal());
    planeChanged();
    shaderChanged();
}

Face::Face(Brush& owner, const Plane3& plane, const Matrix4& texdef,
           const std::string& shader) :
    _owner(owner),
    _shader(shader, _owner.getBrushNode().getRenderSystem()),
    m_texdef(_shader, TextureProjection()),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    _shader.attachObserver(*this);
    m_plane.setPlane(plane);
    m_texdef.setBasis(m_plane.getPlane().normal());

    m_texdef.m_projection.m_brushprimit_texdef = BrushPrimitTexDef(texdef);
    m_texdef.m_projectionInitialised = true;
    m_texdef.m_scaleApplied = true;

    planeChanged();
    shaderChanged();
}

Face::Face(Brush& owner, const Face& other) :
    IFace(other),
    IUndoable(other),
    SurfaceShader::Observer(other),
    _owner(owner),
    m_plane(other.m_plane),
    _shader(other._shader.getMaterialName(), _owner.getBrushNode().getRenderSystem()),
    m_texdef(_shader, other.getTexdef().normalised()),
    _undoStateSaver(nullptr),
    _faceIsVisible(other._faceIsVisible)
{
    _shader.attachObserver(*this);
    planepts_assign(m_move_planepts, other.m_move_planepts);
    m_texdef.setBasis(m_plane.getPlane().normal());
    planeChanged();
}

Face::~Face()
{
    _shader.detachObserver(*this);
}

Brush& Face::getBrush()
{
    return _owner;
}

void Face::planeChanged()
{
    revertTransform();
    _owner.onFacePlaneChanged();
}

void Face::realiseShader()
{
    _owner.onFaceShaderChanged();
}

void Face::unrealiseShader() {
}

void Face::connectUndoSystem(IMapFileChangeTracker& changeTracker)
{
    assert(!_undoStateSaver);

    _shader.setInUse(true);

	_undoStateSaver = GlobalUndoSystem().getStateSaver(*this, changeTracker);
}

void Face::disconnectUndoSystem(IMapFileChangeTracker& changeTracker)
{
    assert(_undoStateSaver);
    _undoStateSaver = nullptr;
    GlobalUndoSystem().releaseStateSaver(*this);

    _shader.setInUse(false);
}

void Face::undoSave()
{
    if (_undoStateSaver)
	{
        _undoStateSaver->save(*this);
    }
}

// undoable
IUndoMementoPtr Face::exportState() const
{
    return IUndoMementoPtr(new SavedState(*this));
}

void Face::importState(const IUndoMementoPtr& data)
{
    undoSave();

	std::static_pointer_cast<SavedState>(data)->exportState(*this);

    planeChanged();
    _owner.onFaceConnectivityChanged();
    texdefChanged();
    _owner.onFaceShaderChanged();
}

void Face::flipWinding() {
    m_plane.reverse();
    planeChanged();
}

bool Face::intersectVolume(const VolumeTest& volume) const
{
    if (!m_winding.empty())
    {
        const Plane3& plane = m_planeTransformed.getPlane();
        return volume.TestPlane(Plane3(plane.normal(), -plane.dist()));
    }
    else
    {
        // Empty winding, return false
        return false;
    }
}

bool Face::intersectVolume(const VolumeTest& volume, const Matrix4& localToWorld) const
{
    if (m_winding.size() > 0)
    {
        return volume.TestPlane(Plane3(plane3().normal(), -plane3().dist()), localToWorld);
    }
    else
    {
        // Empty winding, return false
        return false;
    }
}

void Face::submitRenderables(RenderableCollector& collector,
                             const Matrix4& localToWorld,
                             const IRenderEntity& entity) const
{
    collector.SetState(_shader.getGLShader(), RenderableCollector::eFullMaterials);
    collector.addRenderable(m_winding, localToWorld, entity);
}

void Face::setRenderSystem(const RenderSystemPtr& renderSystem)
{
    _shader.setRenderSystem(renderSystem);

    // Update the visibility flag, we might have switched shaders
    const ShaderPtr& shader = _shader.getGLShader();

    if (shader)
    {
        _faceIsVisible = shader->getMaterial()->isVisible();
    }
    else
    {
        _faceIsVisible = false; // no shader => not visible
    }
}

void Face::translate(const Vector3& translation)
{
    if (GlobalBrush().textureLockEnabled())
    {
        m_texdefTransformed.transformLocked(_shader.getWidth(), _shader.getHeight(), 
            m_plane.getPlane(), Matrix4::getTranslation(translation));
    }

    m_planeTransformed.translate(translation);
    _owner.onFacePlaneChanged();
    updateWinding();
}

void Face::transform(const Matrix4& matrix)
{
    if (GlobalBrush().textureLockEnabled()) 
    {
        m_texdefTransformed.transformLocked(_shader.getWidth(), _shader.getHeight(), m_plane.getPlane(), matrix);
    }

    // Transform the FacePlane using the given matrix
    m_planeTransformed.transform(matrix);
    _owner.onFacePlaneChanged();
    updateWinding();
}

void Face::assign_planepts(const PlanePoints planepts)
{
    m_planeTransformed.initialiseFromPoints(
        planepts[0], planepts[1], planepts[2]
    );
    _owner.onFacePlaneChanged();
    updateWinding();
}

/// \brief Reverts the transformable state of the brush to identity.
void Face::revertTransform() {
    m_planeTransformed = m_plane;
    planepts_assign(m_move_planeptsTransformed, m_move_planepts);
    m_texdefTransformed = m_texdef.m_projection;
    updateWinding();
}

void Face::freezeTransform() {
    undoSave();
    m_plane = m_planeTransformed;
    planepts_assign(m_move_planepts, m_move_planeptsTransformed);
    m_texdef.m_projection = m_texdefTransformed;
    updateWinding();
}

void Face::updateWinding() {
    m_winding.updateNormals(m_plane.getPlane().normal());
}

void Face::update_move_planepts_vertex(std::size_t index, PlanePoints planePoints) {
    std::size_t numpoints = getWinding().size();
    ASSERT_MESSAGE(index < numpoints, "update_move_planepts_vertex: invalid index");

    std::size_t opposite = getWinding().opposite(index);
    std::size_t adjacent = getWinding().wrap(opposite + numpoints - 1);
    planePoints[0] = getWinding()[opposite].vertex;
    planePoints[1] = getWinding()[index].vertex;
    planePoints[2] = getWinding()[adjacent].vertex;
    // winding points are very inaccurate, so they must be quantised before using them to generate the face-plane
    planepts_quantise(planePoints, GRID_MIN);
}

void Face::snapto(float snap) {
    if (contributes()) {
        PlanePoints planePoints;
        update_move_planepts_vertex(0, planePoints);
        planePoints[0].snap(snap);
        planePoints[1].snap(snap);
        planePoints[2].snap(snap);
        assign_planepts(planePoints);
        freezeTransform();
        SceneChangeNotify();
        if (!m_plane.getPlane().isValid()) {
            rError() << "WARNING: invalid plane after snap to grid\n";
        }
    }
}

void Face::testSelect(SelectionTest& test, SelectionIntersection& best) {
    m_winding.testSelect(test, best);
}

void Face::testSelect_centroid(SelectionTest& test, SelectionIntersection& best) {
    test.TestPoint(m_centroid, best);
}

void Face::shaderChanged()
{
    EmitTextureCoordinates();
    _owner.onFaceShaderChanged();

    // Update the visibility flag, but leave out the contributes() check
    const ShaderPtr& shader = getFaceShader().getGLShader();

    if (shader)
    {
        _faceIsVisible = shader->getMaterial()->isVisible();
    }
    else
    {
        _faceIsVisible = false; // no shader => not visible
    }

    planeChanged();
    SceneChangeNotify();
}

const std::string& Face::getShader() const
{
    return _shader.getMaterialName();
}

void Face::setShader(const std::string& name)
{
    undoSave();
    _shader.setMaterialName(name);
    shaderChanged();
}

void Face::revertTexdef() {
    m_texdefTransformed = m_texdef.m_projection;
}

void Face::texdefChanged()
{
    revertTexdef();
    EmitTextureCoordinates();

    // Update the Texture Tools
    ui::SurfaceInspector::update();
}

void Face::GetTexdef(TextureProjection& projection) const {
    projection = m_texdef.normalised();
}

void Face::SetTexdef(const TextureProjection& projection) {
    undoSave();
    m_texdef.setTexdef(projection);
    texdefChanged();
}

void Face::applyShaderFromFace(const Face& other) {
    // Retrieve the textureprojection from the source face
    TextureProjection projection;
    other.GetTexdef(projection);

    setShader(other.getShader());
    SetTexdef(projection);

    // The list of shared vertices
    std::vector<Winding::const_iterator> thisVerts, otherVerts;

    // Let's see whether this face is sharing any 3D coordinates with the other one
    for (Winding::const_iterator i = other.m_winding.begin(); i != other.m_winding.end(); ++i) {
        for (Winding::const_iterator j = m_winding.begin(); j != m_winding.end(); ++j) {
            // Check if the vertices are matching
            if (j->vertex.isEqual(i->vertex, 0.001))
            {
                // Match found, add to list
                thisVerts.push_back(j);
                otherVerts.push_back(i);
            }
        }
    }

    if (thisVerts.empty() || thisVerts.size() != otherVerts.size()) {
        return; // nothing to do
    }

    // Calculate the distance in texture space of the first shared vertices
    Vector2 dist = thisVerts[0]->texcoord - otherVerts[0]->texcoord;

    // Scale the translation (ShiftTexDef() is scaling this down again, yes this is weird).
    dist[0] *= getFaceShader().getWidth();
    dist[1] *= getFaceShader().getHeight();

    // Shift the texture to match
    shiftTexdef(dist.x(), dist.y());
}

void Face::shiftTexdef(float s, float t) {
    undoSave();
    m_texdef.shift(s, t);
    texdefChanged();
}

void Face::scaleTexdef(float s, float t) {
    undoSave();
    m_texdef.scale(s, t);
    texdefChanged();
}

void Face::rotateTexdef(float angle) {
    undoSave();
    m_texdef.rotate(angle);
    texdefChanged();
}

void Face::fitTexture(float s_repeat, float t_repeat) {
    undoSave();
    m_texdef.fit(m_plane.getPlane().normal(), m_winding, s_repeat, t_repeat);
    texdefChanged();
}

void Face::flipTexture(unsigned int flipAxis) {
    undoSave();
    m_texdef.flipTexture(flipAxis);
    texdefChanged();
}

void Face::alignTexture(EAlignType align)
{
    undoSave();
    m_texdef.alignTexture(align, m_winding);
    texdefChanged();
}

void Face::EmitTextureCoordinates() {
    m_texdefTransformed.emitTextureCoordinates(m_winding, plane3().normal(), Matrix4::getIdentity());
}

const Vector3& Face::centroid() const {
    return m_centroid;
}

void Face::construct_centroid() {
    // Take the plane and let the winding calculate the centroid
    m_centroid = m_winding.centroid(plane3());
}

const Winding& Face::getWinding() const {
    return m_winding;
}
Winding& Face::getWinding() {
    return m_winding;
}

const Plane3& Face::plane3() const
{
    _owner.onFaceEvaluateTransform();
    return m_planeTransformed.getPlane();
}

const Plane3& Face::getPlane3() const
{
    return m_plane.getPlane();
}

FacePlane& Face::getPlane() {
    return m_plane;
}
const FacePlane& Face::getPlane() const {
    return m_plane;
}

FaceTexdef& Face::getTexdef() {
    return m_texdef;
}
const FaceTexdef& Face::getTexdef() const {
    return m_texdef;
}

Matrix4 Face::getTexDefMatrix() const
{
    return m_texdef.m_projection.m_brushprimit_texdef.getTransform();
}

SurfaceShader& Face::getFaceShader() {
    return _shader;
}
const SurfaceShader& Face::getFaceShader() const {
    return _shader;
}

bool Face::contributes() const {
    return m_winding.size() > 2;
}

bool Face::is_bounded() const {
    for (Winding::const_iterator i = m_winding.begin(); i != m_winding.end(); ++i) {
        if (i->adjacent == c_brush_maxFaces) {
            return false;
        }
    }
    return true;
}

void Face::normaliseTexture() {
    undoSave();

    Winding::const_iterator nearest = m_winding.begin();

    // Find the vertex with the minimal distance to the origin
    for (Winding::const_iterator i = m_winding.begin(); i != m_winding.end(); ++i) {
        if (nearest->texcoord.getLength() > i->texcoord.getLength()) {
            nearest = i;
        }
    }

    Vector2 texcoord = nearest->texcoord;

    // The floored values
    Vector2 floored(floor(fabs(texcoord[0])), floor(fabs(texcoord[1])));

    // The signs of the original texcoords (needed to know which direction it should be shifted)
    Vector2 sign(texcoord[0]/fabs(texcoord[0]), texcoord[1]/fabs(texcoord[1]));

    Vector2 shift;
    shift[0] = (fabs(texcoord[0]) > 1.0E-4) ? -floored[0] * sign[0] * m_texdef.m_shader.getWidth() : 0.0f;
    shift[0] = (fabs(texcoord[1]) > 1.0E-4) ? -floored[1] * sign[1] * m_texdef.m_shader.getHeight() : 0.0f;

    // Shift the texture (note the minus sign, the FaceTexDef negates it yet again).
    m_texdef.shift(-shift[0], shift[1]);

    texdefChanged();
}

void Face::updateFaceVisibility()
{
    _faceIsVisible = contributes() && getFaceShader().getGLShader()->getMaterial()->isVisible();
}
