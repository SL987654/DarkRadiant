#include "ModelPreview.h"
#include "../GLWidget.h"

#include "ifilter.h"
#include "iuimanager.h"
#include "imodelcache.h"
#include "i18n.h"
#include "ieclass.h"
#include "os/path.h"
#include "math/AABB.h"
#include "modelskin.h"
#include "entitylib.h"
#include "scene/Node.h"
#include "scene/BasicRootNode.h"
#include "wxutil/dialog/MessageBox.h"

#include "iuimanager.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace wxutil
{

/* CONSTANTS */

namespace
{
	const char* const FUNC_STATIC_CLASS = "func_static";
}

// Construct the widgets

ModelPreview::ModelPreview(wxWindow* parent) :
    RenderPreview(parent, false),
	_lastModel(""),
	_defaultCamDistanceFactor(6.0f)
{ 
	_defaultTransform = Matrix4::getRotationAboutZDegrees(-45);
	_defaultTransform = _defaultTransform.getMultipliedBy(Matrix4::getRotation(Vector3(1,1,0), 45));
}

// Set the model, this also resets the camera
void ModelPreview::setModel(const std::string& model)
{
	// If the model name is empty, release the model
	if (model.empty())
	{
		if (_modelNode)
		{
			_entity->removeChildNode(_modelNode);
		}

		_modelNode.reset();

		stopPlayback();
		return;
	}

	// Set up the scene
	if (!_entity)
	{
		getScene(); // trigger a setupscenegraph call
	}

	if (_modelNode)
	{
		_entity->removeChildNode(_modelNode);
	}

	_modelNode = GlobalModelCache().getModelNode(model);

	if (_modelNode)
	{
		_entity->addChildNode(_modelNode);

		// Trigger an initial update of the subgraph
		GlobalFilterSystem().updateSubgraph(getScene()->root());

		// Reset camera if the model has changed
		if (model != _lastModel)
		{
			// Reset preview time
			stopPlayback();

			// Reset the rotation to the default one
			_rotation = _defaultTransform;

			// Calculate camera distance so model is appropriately zoomed
			_camDist = -(_modelNode->localAABB().getRadius() * _defaultCamDistanceFactor);
		}

		_lastModel = model;
	}

	// Redraw
	queueDraw();
}

// Set the skin, this does NOT reset the camera

void ModelPreview::setSkin(const std::string& skin) {

	// Load and apply the skin, checking first to make sure the model is valid
	// and not null
	if (_modelNode != NULL)
	{
		model::ModelNodePtr model = Node_getModel(_modelNode);

		if (model)
		{
			ModelSkin& mSkin = GlobalModelSkinCache().capture(skin);
			model->getIModel().applySkin(mSkin);
		}
	}

	// Redraw
	queueDraw();
}

void ModelPreview::setDefaultOrientation(const Matrix4& transform)
{
	_defaultTransform = transform;
}

void ModelPreview::setDefaultCamDistanceFactor(float factor)
{
	_defaultCamDistanceFactor = factor;
}

void ModelPreview::setupSceneGraph()
{
	RenderPreview::setupSceneGraph();

    try
    {
        _rootNode.reset(new scene::BasicRootNode);

        _entity = GlobalEntityCreator().createEntity(
            GlobalEntityClassManager().findClass(FUNC_STATIC_CLASS));

        _rootNode->addChildNode(_entity);

        _entity->enable(scene::Node::eHidden);

        // This entity is acting as our root node in the scene
        getScene()->setRoot(_rootNode);
    }
    catch (std::runtime_error&)
    {
        wxutil::Messagebox::ShowError(
            (boost::format(_("Unable to setup the preview,\n"
            "could not find the entity class %s")) % FUNC_STATIC_CLASS).str());
    }
}

AABB ModelPreview::getSceneBounds()
{
	if (!_modelNode)
	{
		return RenderPreview::getSceneBounds();
	}

	return _modelNode->localAABB();
}

bool ModelPreview::onPreRender()
{
	return _modelNode != NULL;
}

RenderStateFlags ModelPreview::getRenderFlagsFill()
{
	return RenderPreview::getRenderFlagsFill() | RENDER_DEPTHWRITE | RENDER_DEPTHTEST;
}

} // namespace ui
