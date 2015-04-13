#pragma once

#include "idialogmanager.h"
#include "DialogBase.h"
#include <map>

class wxFlexGridSizer;

namespace wxutil
{

class DialogManager;

class DialogElement;
typedef std::shared_ptr<DialogElement> DialogElementPtr;

/**
 * greebo: A customisable Dialog window featuring Ok and Cancel buttons.
 *
 * Dialog elements can be added to the window using the addLabel(),
 * addButton(), add*() methods etc., which are returning a unique Handle.
 *
 * Use the getElementValue() and setElementValue() methods to
 * get and set the values of these dialog elements.
 *
 * Once the run() method is invoked, the Dialog enters a main loop,
 * showing the dialog and blocking the application. Use the result
 * returned by run() to see which action the user has taken.
 */
class Dialog :
	public ui::IDialog
{
protected:
	// The actual dialog implementation 
	// We don't inherit from the dialog since wxWidgets has its
	// own memory management routines and requires dialogs to be
	// deallocated by calling Destroy(), not by directly deleting them.
	DialogBase* _dialog;

	ui::IDialog::Result _result;

	// The table carrying the elements
	wxFlexGridSizer* _elementsTable;

	// Whether all widgets have been created
	bool _constructed;

	// The elements added to this dialog, indexed by handle
	typedef std::map<Handle, DialogElementPtr> ElementMap;
	ElementMap _elements;

	Handle _highestUsedHandle;

public:
	Dialog(const std::string& title, wxWindow* parent = NULL);

	virtual ~Dialog();

	virtual void setTitle(const std::string& title);

	virtual Handle addLabel(const std::string& text);
	virtual Handle addComboBox(const std::string& label, const ComboBoxOptions& options);
	virtual Handle addEntryBox(const std::string& label);
	virtual Handle addPathEntry(const std::string& label, bool foldersOnly = false);
	virtual Handle addSpinButton(const std::string& label, double min, double max, double step, unsigned int digits);
	virtual Handle addCheckbox(const std::string& label);

	virtual void setElementValue(const Handle& handle, const std::string& value);
	virtual std::string getElementValue(const Handle& handle);

	void setDefaultSize(int width, int height);

	// Enter the main loop
	virtual ui::IDialog::Result run();

	// Add a custom DialogElement
	ui::IDialog::Handle addElement(const DialogElementPtr& element);

	// get the parent widget needed to construct custom elements
	wxWindow* getElementParent();

protected:
	// Constructs the dialog (is invoked right before entering the main loop)
	virtual void construct();

	// Overridable button creation method
	virtual void createButtons();

public:
	// Static methods to display pre-fabricated dialogs

	/**
	 * Display a text entry dialog with the given title and prompt text. Returns a
	 * std::string with the entered value, or throws EntryAbortedException if the
	 * dialog was cancelled. The text entry will be filled with the given defaultText
	 * at start.
	 */
	static std::string TextEntryDialog(const std::string& title,
									   const std::string& prompt,
									   const std::string& defaultText,
									   wxWindow* mainFrame);

};
typedef std::shared_ptr<Dialog> DialogPtr;

} // namespace wxutil
