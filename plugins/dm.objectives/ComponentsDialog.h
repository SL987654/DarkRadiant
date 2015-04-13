#pragma once

#include "ce/ComponentEditor.h"
#include "DifficultyPanel.h"
#include "Objective.h"

#include <map>
#include <vector>
#include <memory>
#include <string>

#include "wxutil/XmlResourceBasedWidget.h"
#include "wxutil/dialog/DialogBase.h"
#include "wxutil/TreeView.h"
#include <sigc++/connection.h>

class wxChoice;
class wxTextCtrl;
class wxPanel;

namespace objectives
{

/**
 * Dialog for displaying and editing the properties and components (conditions)
 * attached to a particular objective.
 */
class ComponentsDialog :
	public wxutil::DialogBase,
	private wxutil::XmlResourceBasedWidget
{
private:
	// The objective we are editing
	Objective& _objective;

	struct ComponentListColumns :
		public wxutil::TreeModel::ColumnRecord
	{
		ComponentListColumns() :
			index(add(wxutil::TreeModel::Column::Integer)),
			description(add(wxutil::TreeModel::Column::String))
		{}

		wxutil::TreeModel::Column index;
		wxutil::TreeModel::Column description;
	};

	// List store for the components
	ComponentListColumns _columns;
	wxutil::TreeModel::Ptr _componentList;
	wxutil::TreeView* _componentView;

	// Currently-active ComponentEditor (if any)
	ce::ComponentEditorPtr _componentEditor;

	// The widgets needed for editing the difficulty levels
	std::unique_ptr<DifficultyPanel> _diffPanel;

	// Working set of components we're editing (get written to the objective
	// as soon as the "Save" button is pressed.
	Objective::ComponentMap _components;

	// TRUE while the widgets are populated to disable callbacks
	bool _updateMutex;

	wxTextCtrl* _objDescriptionEntry;
	wxChoice* _objStateCombo;
	wxTextCtrl* _enablingObjs;
	wxTextCtrl* _successLogic;
	wxTextCtrl* _failureLogic;

	wxTextCtrl* _completionScript;
	wxTextCtrl* _failureScript;
	wxTextCtrl* _completionTarget;
	wxTextCtrl* _failureTarget;

	wxCheckBox* _objMandatoryFlag;
	wxCheckBox* _objIrreversibleFlag;
	wxCheckBox* _objOngoingFlag;
	wxCheckBox* _objVisibleFlag;

	wxPanel* _editPanel;
	wxChoice* _typeCombo;

	wxCheckBox* _stateFlag;
	wxCheckBox* _irreversibleFlag;
	wxCheckBox* _invertedFlag;
	wxCheckBox* _playerResponsibleFlag;

	wxPanel* _compEditorPanel;

	sigc::connection _componentChanged;

	bool _updateNeeded;

private:
	// Construction helpers
	void setupObjectiveEditPanel();
	void createListView();
	void setupEditPanel();

	// Populate the list of components from the Objective's component map
	void populateComponents();

	// Updates the list store contents without removing any components
	void updateComponents();

	// Populate the edit panel widgets with the specified component number
	void populateEditPanel(int index);

	// Populate the objective properties
	void populateObjectiveEditPanel();

	// Get the index of the selected Component, or -1 if there is no selection
	int getSelectedIndex();

    // Change the active ComponentEditor
    void changeComponentEditor(Component& compToEdit);

    // If there is a ComponentEditor, write its contents to the Component
    void checkWriteComponent();

	// Writes the data from the widgets to the data structures
	void save();

	// callbacks
	void _onDelete();
	void handleSelectionChange();
	void _onSelectionChanged(wxDataViewEvent& ev);
	void _onCompToggleChanged(wxCommandEvent& ev);

	void _onAddComponent(wxCommandEvent& ev);
	void _onDeleteComponent(wxCommandEvent& ev);

	void handleTypeChange();
	void _onTypeChanged(wxCommandEvent& ev);
    void _onApplyComponentChanges();

	void _onComponentChanged();

public:

	/**
	 * Constructor creates widgets.
	 *
	 * @param parent
	 * The parent window for which this dialog should be a transient.
	 *
	 * @param objective
	 * The Objective object for which conditions should be displayed and edited.
	 */
	ComponentsDialog(wxWindow* parent, Objective& objective);

	// Override DialogBase
	int ShowModal();

	// Destructor performs cleanup
	~ComponentsDialog();
};

} // namespace
