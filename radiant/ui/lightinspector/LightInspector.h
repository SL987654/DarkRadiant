#pragma once

#include "iselection.h"
#include "icommandsystem.h"
#include "iundo.h"
#include "iradiant.h"
#include "ui/common/ShaderSelector.h"
#include "gtkutil/window/TransientWindow.h"
#include "gtkutil/XmlResourceBasedWidget.h"

#include <map>
#include <string>

/* FORWARD DECLS */
class Entity;
class wxColourPickerEvent;

namespace ui
{

/** Dialog to allow adjustment of properties on lights, including the conversion
 * between projected and point lights.
 */
class LightInspector;
typedef boost::shared_ptr<LightInspector> LightInspectorPtr;

class LightInspector
: public wxutil::TransientWindow,
  public SelectionSystem::Observer,
  public ShaderSelector::Client,
  public UndoSystem::Observer,
  private wxutil::XmlResourceBasedWidget
{
private:
	// Projected light flag
	bool _isProjected;

	// Texture selection combo
	ShaderSelector* _texSelector;

	// The light entity to edit
    typedef std::vector<Entity*> EntityList;
    EntityList _lightEntities;

	// Table of original value keys, to avoid replacing them with defaults
	typedef std::map<std::string, std::string> StringMap;
	StringMap _valueMap;

	// Disables GTK callbacks if set to TRUE (during widget updates)
	bool _updateActive;

private:
	// This is where the static shared_ptr of the singleton instance is held.
	static LightInspectorPtr& InstancePtr();

	// Constructor creates GTK widgets
	LightInspector();

	// TransientWindow callbacks
	virtual void _preShow();
	virtual void _preHide();

	// Widget construction functions
	void setupPointLightPanel();
	void setupProjectedPanel();
	void setupOptionsPanel();
	void setupTextureWidgets();

	// Callbacks
	void _onProjToggle(wxCommandEvent& ev);
	void _onPointToggle(wxCommandEvent& ev);
	void _onOK(wxCommandEvent& ev);
	void _onColourChange(wxColourPickerEvent& ev);
	void _onOptionsToggle(wxCommandEvent& ev);

	// Update the dialog widgets from keyvals on the first selected entity
	void getValuesFromEntity();

	// Write the widget contents to the given entity
	void setValuesOnEntity(Entity* entity);

    // Write contents to all light entities
    void writeToAllEntities();

    // Set the given key/value pair on ALL entities in the list of lights
    void setKeyValueAllLights(const std::string& k, const std::string& v);

	/** greebo: Gets called when the shader selection gets changed, so that
	 * 			the displayed texture info can be updated.
	 */
	void shaderSelectionChanged(const std::string& shader, wxutil::TreeModel* listStore);

public:

	// Gets called by the SelectionSystem when the selection is changed
	void selectionChanged(const scene::INodePtr& node, bool isComponent);

	/** Toggle the visibility of the dialog instance, constructing it if necessary.
	 */
	static void toggleInspector(const cmd::ArgumentList& args);

	/** greebo: This is the actual home of the static instance
	 */
	static LightInspector& Instance();

	// Update the sensitivity of the widgets
	void update();

	// UndoSystem::Observer implementation
	void postUndo();
	void postRedo();

	// Safely disconnects this dialog from all the systems
	// and saves the window size/position to the registry
	void onRadiantShutdown();
};

} // namespace
