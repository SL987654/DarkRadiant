#include "EntityClassChooser.h"
#include "EntityClassTreePopulator.h"

#include "i18n.h"
#include "imainframe.h"
#include "iuimanager.h"
#include "ithread.h"

#include <glibmm.h>

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/splitter.h>

#include "gtkutil/TreeModel.h"
#include "string/string.h"
#include "registry/bind.h"
#include "eclass.h"

#include "debugging/ScopedDebugTimer.h"

#include <boost/make_shared.hpp>

namespace ui
{

namespace
{
    const char* const ECLASS_CHOOSER_TITLE = N_("Create entity");
    const std::string RKEY_SPLIT_POS = "user/ui/entityClassChooser/splitPos";
}

// Local class for loading entity class definitions in a separate thread
class EntityClassChooser::ThreadedEntityClassLoader
{
    // Column specification struct
    const EntityClassChooser::TreeColumns& _columns;

    // The tree store to populate. We must operate on our own tree store, since
    // updating the EntityClassChooser's tree store from a different thread
    // wouldn't be safe
    wxutil::TreeModel* _treeStore;

	// The class to be notified on finish
	wxEvtHandler* _finishedHandler;

public:

    // Construct and initialise variables
    ThreadedEntityClassLoader(const EntityClassChooser::TreeColumns& cols, 
							  wxEvtHandler* finishedHandler) : 
		_columns(cols),
		_finishedHandler(finishedHandler)
    {}

    // The worker function that will execute in the thread
    void run()
    {
        ScopedDebugTimer timer("ThreadedEntityClassLoader::run()");

        // Create new treestoree
		_treeStore = new wxutil::TreeModel(_columns);

        // Populate it with the list of entity classes by using a visitor class.
        EntityClassTreePopulator visitor(_treeStore, _columns);
        GlobalEntityClassManager().forEachEntityClass(visitor);

        // Insert the data, using the same walker class as Visitor
        visitor.forEachNode(visitor);

        // Ensure model is sorted before giving it to the tree view
		_treeStore->SortModelFoldersFirst(_columns.name, _columns.isFolder);

		wxutil::TreeModel::PopulationFinishedEvent finishedEvent;
		finishedEvent.SetTreeModel(_treeStore);

		_finishedHandler->AddPendingEvent(finishedEvent);
    }
};

// Main constructor
EntityClassChooser::EntityClassChooser()
: wxutil::DialogBase(_(ECLASS_CHOOSER_TITLE)),
  _treeStore(NULL),
  _treeView(NULL),
  _eclassLoader(new ThreadedEntityClassLoader(_columns, this)),
  _selectedName("")
{
	// Connect the finish callback to load the treestore
	Connect(wxutil::EV_TREEMODEL_POPULATION_FINISHED, 
		TreeModelPopulationFinishedHandler(EntityClassChooser::onTreeStorePopulationFinished), NULL, this);

	loadNamedPanel(this, "EntityClassChooserMainPanel");

    // Connect button signals
    findNamedObject<wxButton>(this, "EntityClassChooserAddButton")->Connect(
        wxEVT_BUTTON, wxCommandEventHandler(EntityClassChooser::onOK), NULL, this);
	findNamedObject<wxButton>(this, "EntityClassChooserAddButton")->SetBitmap(
		wxArtProvider::GetBitmap(wxART_PLUS));

	findNamedObject<wxButton>(this, "EntityClassChooserCancelButton")->Connect(
        wxEVT_BUTTON, wxCommandEventHandler(EntityClassChooser::onCancel), NULL, this);

    // Add model preview to right-hand-side of main container
	wxPanel* rightPanel = findNamedObject<wxPanel>(this, "EntityClassChooserRightPane");

	_modelPreview.reset(new wxutil::ModelPreview(rightPanel));

	rightPanel->GetSizer()->Add(_modelPreview->getWidget(), 1, wxEXPAND);

    // Listen for defs-reloaded signal (cannot bind directly to
    // ThreadedEntityClassLoader method because it is not sigc::trackable)
    GlobalEntityClassManager().defsReloadedSignal().connect(
        sigc::mem_fun(this, &EntityClassChooser::loadEntityClasses)
    );

    // Setup the tree view and invoke threaded loader to get the entity classes
    setupTreeView();
    loadEntityClasses();

    // Persist layout to registry
    // wxTODO registry::bindPropertyToKey(mainPaned->property_position(), RKEY_SPLIT_POS);

	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(EntityClassChooser::onDeleteEvent), NULL, this);

	FitToScreen(0.7f, 0.6f);

	// Set the model preview height to something significantly smaller than the
    // window's height to allow shrinking
	_modelPreview->getWidget()->SetMinClientSize(
		wxSize(GetSize().GetWidth() * 0.4f, GetSize().GetHeight() * 0.2f));
}

// Display the singleton instance
std::string EntityClassChooser::chooseEntityClass(const std::string& preselectEclass)
{
	if (!preselectEclass.empty())
	{
		Instance().setSelectedEntityClass(preselectEclass);
	}

    if (Instance().ShowModal() == wxID_OK)
    {
        return Instance().getSelectedEntityClass();
    }
    else
    {
        return ""; // Empty selection on cancel
    }
}

EntityClassChooser& EntityClassChooser::Instance()
{
    EntityClassChooserPtr& instancePtr = InstancePtr();

    if (instancePtr == NULL)
    {
        // Not yet instantiated, do it now
        instancePtr.reset(new EntityClassChooser);

        // Register this instance with GlobalRadiant() at once
        GlobalRadiant().signal_radiantShutdown().connect(
            sigc::mem_fun(*instancePtr, &EntityClassChooser::onRadiantShutdown)
        );
    }

    return *instancePtr;
}

EntityClassChooserPtr& EntityClassChooser::InstancePtr()
{
    static EntityClassChooserPtr _instancePtr;
    return _instancePtr;
}

void EntityClassChooser::onRadiantShutdown()
{
    rMessage() << "EntityClassChooser shutting down." << std::endl;

    _modelPreview.reset();

    // Final step at shutdown, release the shared ptr
	Instance().SendDestroyEvent();
    InstancePtr().reset();
}

void EntityClassChooser::loadEntityClasses()
{
    assert(_eclassLoader);

    GlobalRadiant().getThreadManager().execute(
        boost::bind(&ThreadedEntityClassLoader::run, _eclassLoader.get())
    );
}

void EntityClassChooser::setSelectedEntityClass(const std::string& eclass)
{
    // Select immediately if possible, otherwise remember class name for later
    // selection
    if (_treeStore != NULL)
    {
		wxDataViewItem item = _treeStore->FindString(eclass, _columns.name.getColumnIndex());

		if (item.IsOk())
		{
			_treeView->Select(item);
			_classToHighlight.clear();

			return;
		}
    }
     
	_classToHighlight = eclass;
}

const std::string& EntityClassChooser::getSelectedEntityClass() const
{
    return _selectedName;
}

void EntityClassChooser::onDeleteEvent(wxCloseEvent& ev)
{
	// greebo: Clear the selected name on hide, we don't want to create another entity when
    // the user clicks on the X in the upper right corner.
    _selectedName.clear();

	EndModal(wxID_CANCEL); // break main loop
	Hide();
}

int EntityClassChooser::ShowModal()
{
	// Update the member variables
    updateSelection();

    // Focus on the treeview
	_treeView->SetFocus();

	return DialogBase::ShowModal();
}

void EntityClassChooser::setTreeViewModel()
{
	_treeView->AssociateModel(_treeStore);
	_treeStore->DecRef();

	// Expand the first layer
	wxDataViewItemArray children;
	_treeStore->GetChildren(_treeStore->GetRoot(), children);

	std::for_each(children.begin(), children.end(), [&] (const wxDataViewItem& item)
	{
		_treeView->Expand(item);
	});

    // Pre-select the given class if requested by setSelectedEntityClass()
    if (!_classToHighlight.empty())
    {
        assert(_treeStore);
        setSelectedEntityClass(_classToHighlight);
    }
}

void EntityClassChooser::setupTreeView()
{
    // Use the TreeModel's full string search function
	_treeStore = new wxutil::TreeModel(_columns);
	wxutil::TreeModel::Row row = _treeStore->AddItem();

	row[_columns.name] = wxVariant(wxDataViewIconText(_("Loading...")));
	
	wxPanel* parent = findNamedObject<wxPanel>(this, "EntityClassChooserLeftPane");

    _treeView = wxutil::TreeView::CreateWithModel(parent, _treeStore);

    // wxTODO view->set_search_equal_func(sigc::ptr_fun(gtkutil::TreeModel::equalFuncStringContains));

	_treeView->Connect(wxEVT_DATAVIEW_SELECTION_CHANGED, 
		wxDataViewEventHandler(EntityClassChooser::onSelectionChanged), NULL, this);

    // Single column with icon and name
	_treeView->AppendIconTextColumn(_("Classname"), _columns.name.getColumnIndex(), 
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

	parent->GetSizer()->Prepend(_treeView, 1, wxEXPAND | wxBOTTOM, 6);
}

// Update the usage information
void EntityClassChooser::updateUsageInfo(const std::string& eclass)
{
    // Lookup the IEntityClass instance
    IEntityClassPtr e = GlobalEntityClassManager().findOrInsert(eclass, true);

    // Set the usage panel to the IEntityClass' usage information string
    wxTextCtrl* usageText = findNamedObject<wxTextCtrl>(this, "EntityClassChooserUsageText");
    usageText->SetValue(eclass::getUsage(*e));
}

void EntityClassChooser::updateSelection()
{
	wxDataViewItem item = _treeView->GetSelection();

	if (item.IsOk())
    {
        wxutil::TreeModel::Row row(item, *_treeStore);

        if (!row[_columns.isFolder].getBool())
        {
            // Make the OK button active
            findNamedObject<wxButton>(this, "EntityClassChooserAddButton")->Enable(true);

            // Set the panel text with the usage information
			wxDataViewIconText iconAndName = static_cast<wxDataViewIconText>(row[_columns.name]);
			std::string selName = iconAndName.GetText();

            updateUsageInfo(selName);

            // Lookup the IEntityClass instance
            IEntityClassPtr eclass = GlobalEntityClassManager().findClass(selName);

            if (eclass != NULL)
            {
                _modelPreview->setModel(eclass->getAttribute("model").getValue());
                _modelPreview->setSkin(eclass->getAttribute("skin").getValue());
            }

            // Update the _selectionName field
            _selectedName = selName;

            return; // success
        }
    }

    // Nothing selected
    _modelPreview->setModel("");
    _modelPreview->setSkin("");

    findNamedObject<wxButton>(this, "EntityClassChooserAddButton")->Enable(false);
}

void EntityClassChooser::onCancel(wxCommandEvent& ev)
{
	_selectedName.clear();

	EndModal(wxID_CANCEL); // break main loop
	Hide();
}

void EntityClassChooser::onOK(wxCommandEvent& ev)
{
    EndModal(wxID_OK); // break main loop
	Hide();
}

void EntityClassChooser::onSelectionChanged(wxDataViewEvent& ev)
{
	updateSelection();
}

void EntityClassChooser::onTreeStorePopulationFinished(wxutil::TreeModel::PopulationFinishedEvent& ev)
{
	_treeStore = ev.GetTreeModel();
    setTreeViewModel();
}


} // namespace ui
