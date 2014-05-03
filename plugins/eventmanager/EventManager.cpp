#include "EventManager.h"

#include "imodule.h"
#include "iradiant.h"
#include "itextstream.h"
#include "icommandsystem.h"
#include "iselection.h"
#include <iostream>
#include <typeinfo>

#include <gdk/gdkevents.h>
#include <gdk/gdkkeysyms.h>
#include <gtkmm/window.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/editable.h>
#include <gtkmm/textview.h>

#include <wx/wxprec.h>
#include <wx/toolbar.h>

#include "registry/registry.h"
#include "xmlutil/Node.h"

#include "Statement.h"
#include "Toggle.h"
#include "WidgetToggle.h"
#include "RegistryToggle.h"
#include "KeyEvent.h"
#include "SaveEventVisitor.h"

#include "debugging/debugging.h"
#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

	namespace {
		const std::string RKEY_DEBUG = "debug/ui/debugEventManager";
	}

// RegisterableModule implementation
const std::string& EventManager::getName() const {
	static std::string _name(MODULE_EVENTMANAGER);
	return _name;
}

const StringSet& EventManager::getDependencies() const {
	static StringSet _dependencies;

	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_XMLREGISTRY);
	}

	return _dependencies;
}

void EventManager::initialiseModule(const ApplicationContext& ctx)
{
	rMessage() << "EventManager::initialiseModule called." << std::endl;

	_modifiers.loadModifierDefinitions();
	_mouseEvents.initialise();

	_debugMode = registry::getValue<bool>(RKEY_DEBUG);

	// Deactivate the empty event, so it's safe to return it as NullEvent
	_emptyEvent->setEnabled(false);

	// Create an empty GClosure
	_accelGroup =  Gtk::AccelGroup::create();

	if (_debugMode) {
		rMessage() << "EventManager intitialised in debug mode." << std::endl;
	}
	else {
		rMessage() << "EventManager successfully initialised." << std::endl;
	}
}

void EventManager::shutdownModule()
{
	rMessage() << "EventManager: shutting down." << std::endl;
	saveEventListToRegistry();

	_handlers.clear();
	_dialogWindows.clear();
	_accelerators.clear();
	_events.clear();

	_accelGroup.reset();
}

// Constructor
EventManager::EventManager() :
	_emptyEvent(new Event()),
	_emptyAccelerator(0, 0, _emptyEvent),
	_modifiers(),
	_mouseEvents(_modifiers),
	_debugMode(false)
{}

// Destructor
EventManager::~EventManager()
{
	rMessage() << "EventManager successfully shut down.\n";
}

void EventManager::connectSelectionSystem(SelectionSystem* selectionSystem)
{
	_mouseEvents.connectSelectionSystem(selectionSystem);
}

// Returns a reference to the mouse event mapper
IMouseEvents& EventManager::MouseEvents() {
	return _mouseEvents;
}

IAccelerator& EventManager::addAccelerator(const std::string& key, const std::string& modifierStr)
{
	unsigned int keyVal = Accelerator::getKeyCodeFromName(key);
	unsigned int modifierFlags = _modifiers.getModifierFlags(modifierStr);

	Accelerator accel(keyVal, modifierFlags, _emptyEvent);

	// Add a new Accelerator to the list
	_accelerators.push_back(accel);

	// return the reference to the last accelerator in the list
	AcceleratorList::reverse_iterator i = _accelerators.rbegin();

	return (*i);
}

IAccelerator& EventManager::addAccelerator(GdkEventKey* event)
{
	// Create a new accelerator with the given arguments
	Accelerator accel(event->keyval, 0 /*wxTODO_modifiers.getKeyboardFlags(event->state)*/, _emptyEvent);

	// Add a new Accelerator to the list
	_accelerators.push_back(accel);

	// return the reference to the last accelerator in the list
	AcceleratorList::reverse_iterator i = _accelerators.rbegin();

	return (*i);
}

IAccelerator& EventManager::addAccelerator(wxKeyEvent& ev)
{
	int keyCode = ev.GetKeyCode();
	unsigned int modifierFlags = _modifiers.getKeyboardFlags(ev);

	// Create a new accelerator with the given arguments
	Accelerator accel(keyCode, modifierFlags, _emptyEvent);

	// Add a new Accelerator to the list
	_accelerators.push_back(accel);

	// return the reference to the last accelerator in the list
	return *_accelerators.rbegin();
}

IEventPtr EventManager::findEvent(const std::string& name) {
	// Try to lookup the command
	EventMap::iterator i = _events.find(name);

	if (i != _events.end()) {
		// Return the pointer to the command
		return i->second;
	}
	else {
		// Nothing found, return the NullEvent
		return _emptyEvent;
	}
}

IEventPtr EventManager::findEvent(GdkEventKey* event)
{
	// Retrieve the accelerators for this eventkey
	AcceleratorList accelList = findAccelerator(/* wXTODO event*/ 0,0);

	// Did we find any matching accelerators?
	if (accelList.size() > 0) {
		// Take the first found accelerator
		Accelerator& accel = *accelList.begin();

		return accel.getEvent();
	}
	else {
		// No accelerators found
		return _emptyEvent;
	}
}

IEventPtr EventManager::findEvent(wxKeyEvent& ev)
{
	// Retrieve the accelerators for this eventkey
	AcceleratorList accelList = findAccelerator(ev);

	// Did we find any matching accelerators? If yes, take the first found accelerator
	return !accelList.empty() ? accelList.begin()->getEvent() : _emptyEvent;
}

std::string EventManager::getEventName(const IEventPtr& event)
{
	// Try to lookup the given eventptr
	for (EventMap::iterator i = _events.begin(); i != _events.end(); i++) {
		if (i->second == event) {
			return i->first;
		}
	}

	return "";
}

std::string EventManager::getAcceleratorStr(const IEventPtr& event, bool forMenu)
{
	std::string returnValue = "";

	IAccelerator& accelerator = findAccelerator(event);

	unsigned int keyVal = accelerator.getKey();
	const std::string keyStr = (keyVal != 0) ? Accelerator::getNameFromKeyCode(keyVal) : "";

	if (!keyStr.empty())
	{
		// Return a modifier string for a menu
		const std::string modifierStr = getModifierStr(accelerator.getModifiers(), forMenu);

		const std::string connector = (forMenu) ? "~" : "+";

		returnValue = modifierStr;
		returnValue += (modifierStr != "") ? connector : "";
		returnValue += keyStr;
	}

	return returnValue;
}

// Checks if the eventName is already registered and writes to rMessage, if so
bool EventManager::alreadyRegistered(const std::string& eventName) {
	// Try to find the command and see if it's already registered
	IEventPtr foundEvent = findEvent(eventName);

	if (foundEvent->empty()) {
		return false;
	}
	else {
		rWarning() << "EventManager: Event " << eventName
			<< " already registered!" << std::endl;
		return true;
	}
}

// Add the given command to the internal list
IEventPtr EventManager::addCommand(const std::string& name, const std::string& statement, bool reactOnKeyUp) {

	if (!alreadyRegistered(name)) {
		// Add the command to the list
		_events[name] = IEventPtr(new Statement(statement, reactOnKeyUp));

		// Return the pointer to the newly created event
		return _events[name];
	}

	return _emptyEvent;
}

IEventPtr EventManager::addKeyEvent(const std::string& name, const ui::KeyStateChangeCallback& keyStateChangeCallback)
{
	if (!alreadyRegistered(name))
	{
		// Add the new keyevent to the list (implicitly cast onto Event&)
		_events[name] = IEventPtr(new KeyEvent(keyStateChangeCallback));

		// Return the pointer to the newly created event
		return _events[name];
	}

	return _emptyEvent;
}

IEventPtr EventManager::addWidgetToggle(const std::string& name) {

	if (!alreadyRegistered(name)) {
		// Add the command to the list (implicitly cast the pointer on Event&)
		_events[name] = IEventPtr(new WidgetToggle());

		// Return the pointer to the newly created event
		return _events[name];
	}

	return _emptyEvent;
}

IEventPtr EventManager::addRegistryToggle(const std::string& name, const std::string& registryKey)
{
	if (!alreadyRegistered(name)) {
		// Add the command to the list (implicitly cast the pointer on Event&)
		_events[name] = IEventPtr(new RegistryToggle(registryKey));

		// Return the pointer to the newly created event
		return _events[name];
	}

	return _emptyEvent;
}

IEventPtr EventManager::addToggle(const std::string& name, const ToggleCallback& onToggled)
{
	if (!alreadyRegistered(name)) {
		// Add the command to the list (implicitly cast the pointer on Event&)
		_events[name] = IEventPtr(new Toggle(onToggled));

		// Return the pointer to the newly created event
		return _events[name];
	}

	return _emptyEvent;
}

void EventManager::setToggled(const std::string& name, const bool toggled)
{
	// Check could be placed here by boost::shared_ptr's dynamic_pointer_cast
	if (!findEvent(name)->setToggled(toggled)) {
		rWarning() << "EventManager: Event " << name
			<< " is not a Toggle." << std::endl;
	}
}

// Connects the given accelerator to the given command (identified by the string)
void EventManager::connectAccelerator(IAccelerator& accelerator, const std::string& command) {
	IEventPtr event = findEvent(command);

	if (!event->empty()) {
		// Command found, connect it to the accelerator by passing its pointer
		accelerator.connectEvent(event);
	}
	else {
		// Command NOT found
		rWarning() << "EventManager: Unable to connect command: " << command << std::endl;
	}
}

void EventManager::disconnectAccelerator(const std::string& command) {
	IEventPtr event = findEvent(command);

	if (!event->empty()) {
		// Cycle through the accelerators and check for matches
		for (AcceleratorList::iterator i = _accelerators.begin(); i != _accelerators.end(); i++) {
			if (i->match(event)) {
				// Connect the accelerator to the empty event (disable the accelerator)
				i->connectEvent(_emptyEvent);
				i->setKey(0);
				i->setModifiers(0);
			}
		}
	}
	else {
		// Command NOT found
		rWarning() << "EventManager: Unable to disconnect command: " << command << std::endl;
	}
}

void EventManager::disableEvent(const std::string& eventName) {
	findEvent(eventName)->setEnabled(false);
}

void EventManager::enableEvent(const std::string& eventName) {
	findEvent(eventName)->setEnabled(true);
}

void EventManager::removeEvent(const std::string& eventName) {
	// Try to lookup the command
	EventMap::iterator i = _events.find(eventName);

	if (i != _events.end()) {
		// Remove all accelerators beforehand
		disconnectAccelerator(eventName);

		// Remove the event from the list
		_events.erase(i);
	}
}

// Catches the key/mouse press/release events from the given GtkObject
void EventManager::connect(Gtk::Widget* widget)
{
	std::pair<HandlerMap::iterator, bool> result = _handlers.insert(
		HandlerMap::value_type(widget, ConnectionPair()));

	if (result.second == false)
	{
		// Widget already connected
		return;
	}

	// Key press
	result.first->second.first = widget->signal_key_press_event().connect(
		sigc::bind(sigc::mem_fun(*this, &EventManager::onKeyPress), widget));

	// Key release
	result.first->second.second = widget->signal_key_release_event().connect(
		sigc::bind(sigc::mem_fun(*this, &EventManager::onKeyRelease), widget));
}

void EventManager::disconnect(Gtk::Widget* widget)
{
	HandlerMap::iterator found = _handlers.find(widget);

	if (found != _handlers.end())
	{
		found->second.first.disconnect();
		found->second.second.disconnect();

		_handlers.erase(found);
	}
	else
	{
		// Widget not connected
		return;
	}
}

void EventManager::connect(wxWindow& widget)
{
	widget.Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(EventManager::onKeyPressWx), NULL, this);
	widget.Connect(wxEVT_KEY_UP, wxKeyEventHandler(EventManager::onKeyReleaseWx), NULL, this);
}

void EventManager::disconnect(wxWindow& widget)
{
	widget.Disconnect(wxEVT_KEY_UP, wxKeyEventHandler(EventManager::onKeyReleaseWx), NULL, this);
	widget.Disconnect(wxEVT_KEY_DOWN, wxKeyEventHandler(EventManager::onKeyPressWx), NULL, this);
}

void EventManager::disconnectToolbar(wxToolBar* toolbar)
{
	std::for_each(_events.begin(), _events.end(), [&] (EventMap::value_type& pair)
	{
		for (std::size_t tool = 0; tool < toolbar->GetToolsCount(); tool++)
		{
			pair.second->disconnectToolItem(const_cast<wxToolBarToolBase*>(toolbar->GetToolByPos(tool)));
		}
	});
}

/* greebo: This connects an dialog window to the event handler. This means the following:
 *
 * An incoming key-press event reaches the static method onDialogKeyPress which
 * passes the key event to the connect dialog FIRST, before the key event has a
 * chance to be processed by the standard shortcut processor. IF the dialog window
 * standard handler returns TRUE, that is. If the gtk_window_propagate_key_event()
 * function returns FALSE, the window couldn't find a use for this specific key event
 * and the event can be passed safely to the onKeyPress() method.
 *
 * This way it is ensured that the dialog window can handle, say, text entries without
 * firing global shortcuts all the time.
 */
void EventManager::connectDialogWindow(Gtk::Window* window)
{
	std::pair<HandlerMap::iterator, bool> result = _handlers.insert(
		HandlerMap::value_type(window, ConnectionPair()));

	if (result.second == false)
	{
		rError() << "EventManager::connect: Widget is already connected." << std::endl;
	}

	// Key press
	result.first->second.first = window->signal_key_press_event().connect(
		sigc::bind(sigc::mem_fun(*this, &EventManager::onDialogKeyPress), window));

	// Key release
	result.first->second.second = window->signal_key_release_event().connect(
		sigc::bind(sigc::mem_fun(*this, &EventManager::onDialogKeyRelease), window));
}

void EventManager::disconnectDialogWindow(Gtk::Window* window)
{
	HandlerMap::iterator found = _dialogWindows.find(window);

	if (found != _dialogWindows.end())
	{
		found->second.first.disconnect();
		found->second.second.disconnect();

		_dialogWindows.erase(found);
	}
	else
	{
		rWarning()  << "EventManager::disconnect: Widget is not connected." << std::endl;
	}
}

void EventManager::connectAccelGroup(Gtk::Window* window)
{
	window->add_accel_group(_accelGroup);
}

void EventManager::connectAccelGroup(const Glib::RefPtr<Gtk::Window>& window)
{
	window->add_accel_group(_accelGroup);
}

// Loads the default shortcuts from the registry
void EventManager::loadAccelerators()
{
	if (_debugMode) {
		std::cout << "EventManager: Loading accelerators...\n";
	}

	// Register all custom statements as events too to make them shortcut-bindable
	// before going ahead
	GlobalCommandSystem().foreachStatement([&] (const std::string& statementName)
	{
		addCommand(statementName, statementName, false);
	}, true); // custom statements only

	xml::NodeList shortcutSets = GlobalRegistry().findXPath("user/ui/input//shortcuts");

	if (_debugMode) {
		std::cout << "Found " << shortcutSets.size() << " sets.\n";
	}

	// If we have two sets of shortcuts, delete the default ones
	if (shortcutSets.size() > 1) {
		GlobalRegistry().deleteXPath("user/ui/input//shortcuts[@name='default']");
	}

	// Find all accelerators
	xml::NodeList shortcutList = GlobalRegistry().findXPath("user/ui/input/shortcuts//shortcut");

	if (!shortcutList.empty())
	{
		rMessage() << "EventManager: Shortcuts found in Registry: " << shortcutList.size() << std::endl;

		for (std::size_t i = 0; i < shortcutList.size(); i++)
		{
			const std::string key = shortcutList[i].getAttributeValue("key");
			const std::string cmd = shortcutList[i].getAttributeValue("command");

			if (_debugMode) 
			{
				std::cout << "Looking up command: " << cmd << "\n";
				std::cout << "Key is: >> " << key << " << \n";
			}

			// Try to lookup the command
			IEventPtr event = findEvent(cmd);

			// Check for a non-empty key string
			if (!key.empty())
			{
				 // Check for valid command definitions were found
				if (!event->empty())
				{
					// Get the modifier string (e.g. "SHIFT+ALT")
					const std::string modifierStr = shortcutList[i].getAttributeValue("modifiers");

					if (!duplicateAccelerator(key, modifierStr, event))
					{
						// Create the accelerator object
						IAccelerator& accelerator = addAccelerator(key, modifierStr);

						// Connect the newly created accelerator to the command
						accelerator.connectEvent(event);
					}
				}
				else
				{
					rWarning() << "EventManager: Cannot load shortcut definition (command invalid): " 
						<< cmd << std::endl;
				}
			}
		}
	}
	else 
	{
		// No accelerator definitions found!
		rWarning() << "EventManager: No shortcut definitions found..." << std::endl;
	}
}

void EventManager::foreachEvent(IEventVisitor& eventVisitor) {
	// Cycle through the event and pass them to the visitor class
	for (EventMap::iterator i = _events.begin(); i != _events.end(); i++) {
		const std::string eventName = i->first;
		IEventPtr event = i->second;

		eventVisitor.visit(eventName, event);
	}
}

// Tries to locate an accelerator, that is connected to the given command
IAccelerator& EventManager::findAccelerator(const IEventPtr& event) {
	// Cycle through the accelerators and check for matches
	for (AcceleratorList::iterator i = _accelerators.begin(); i != _accelerators.end(); i++) {
		if (i->match(event)) {
			// Return the reference to the found accelerator
			return (*i);
		}
	}

	// Return an empty accelerator if nothing is found
	return _emptyAccelerator;
}

// Returns the string representation of the given modifier flags
std::string EventManager::getModifierStr(const unsigned int modifierFlags, bool forMenu) {
	// Pass the call to the modifiers helper class
	return _modifiers.getModifierStr(modifierFlags, forMenu);
}

unsigned int EventManager::getModifierState()
{
	return _modifiers.getState();
}

void EventManager::saveEventListToRegistry() {
	const std::string rootKey = "user/ui/input";

	// The visitor class to save each event definition into the registry
	// Note: the SaveEventVisitor automatically wipes all the existing shortcuts from the registry
	SaveEventVisitor visitor(rootKey, this);

	foreachEvent(visitor);
}

EventManager::AcceleratorList EventManager::findAccelerator(
	const std::string& key, const std::string& modifierStr)
{
	guint keyVal = getGDKCode(key);
	unsigned int modifierFlags = _modifiers.getModifierFlags(modifierStr);

	return findAccelerator(keyVal, modifierFlags);
}

bool EventManager::duplicateAccelerator(const std::string& key,
										const std::string& modifiers,
										const IEventPtr& event)
{
	AcceleratorList accelList = findAccelerator(key, modifiers);

	for (AcceleratorList::iterator i = accelList.begin(); i != accelList.end(); i++) {
		// If one of the accelerators in the list matches the event, return true
		if (i->match(event)) {
			return true;
		}
	}

	return false;
}

EventManager::AcceleratorList EventManager::findAccelerator(guint keyVal,
															const unsigned int modifierFlags)
{
	AcceleratorList returnList;

	// Cycle through the accelerators and check for matches
	for (AcceleratorList::iterator i = _accelerators.begin(); i != _accelerators.end(); i++) {

		if (i->match(keyVal, modifierFlags))
		{
			// Add the pointer to the found accelerators
			returnList.push_back((*i));
		}
	}

	return returnList;
}

// Returns the pointer to the accelerator for the given GdkEvent, but convert the key to uppercase before passing it
/*EventManager::AcceleratorList EventManager::findAccelerator(GdkEventKey* event)
{
	unsigned int keyval = gdk_keyval_to_upper(event->keyval);

	// greebo: I saw this in the original GTKRadiant code, maybe this is necessary to catch GTK_ISO_Left_Tab...
	if (keyval == GDK_ISO_Left_Tab) {
		keyval = GDK_Tab;
	}

	return findAccelerator(keyval, _modifiers.getKeyboardFlags(event->state));
}*/

EventManager::AcceleratorList EventManager::findAccelerator(wxKeyEvent& ev)
{
	int keyval = ev.GetKeyCode(); // is always uppercase
	
	return findAccelerator(keyval, _modifiers.getKeyboardFlags(ev));
}

// The GTK keypress callback
bool EventManager::onDialogKeyPress(GdkEventKey* ev, Gtk::Window* window)
{
	// Pass the key event to the connected dialog window and see if it can process it (returns TRUE)
	// greebo: gtkmm is not exposing this function??
	bool keyProcessed = gtk_window_propagate_key_event(window->gobj(), ev);

	// Get the focus widget, is it an editable widget?
	Gtk::Widget* focus = window->get_focus();
	bool isEditableWidget = dynamic_cast<Gtk::Editable*>(focus) != NULL || dynamic_cast<Gtk::TextView*>(focus) != NULL;

	// never pass onKeyPress event to the accelerator manager if an editable widget is focused
	// the only exception is the ESC key
	if (isEditableWidget && ev->keyval != GDK_Escape)
	{
		return keyProcessed;
	}

	if (!keyProcessed)
	{
		// The dialog window returned FALSE, pass the key on to the default onKeyPress handler
		onKeyPress(ev, window);
	}

	// If we return true here, the dialog window could process the key, and the GTK callback chain is stopped
	return keyProcessed;
}

// The GTK keyrelease callback
bool EventManager::onDialogKeyRelease(GdkEventKey* ev, Gtk::Window* window)
{
	// Pass the key event to the connected dialog window and see if it can process it (returns TRUE)
	// greebo: gtkmm is not exposing this function??
	bool keyProcessed = gtk_window_propagate_key_event(window->gobj(), ev);

	// Get the focus widget, is it an editable widget?
	Gtk::Widget* focus = window->get_focus();
	bool isEditableWidget = dynamic_cast<Gtk::Editable*>(focus) != NULL || dynamic_cast<Gtk::TextView*>(focus) != NULL;

	if (isEditableWidget && ev->keyval != GDK_Escape)
	{
		// never pass onKeyPress event to the accelerator manager if an editable widget is focused
		return keyProcessed;
	}

	if (!keyProcessed)
	{
		// The dialog window returned FALSE, pass the key on to the default key handler
		onKeyRelease(ev, window);
	}

	// If we return true here, the dialog window could process the key, and the GTK callback chain is stopped
	return keyProcessed;
}

void EventManager::updateStatusText(GdkEventKey* event, bool keyPress)
{
	// Make a copy of the given event key
	GdkEventKey eventKey = *event;

	// Sometimes the ALT modifier is not set, so this is a workaround for this
	if (eventKey.keyval == GDK_Alt_L || eventKey.keyval == GDK_Alt_R) {
		if (keyPress) {
			eventKey.state |= GDK_MOD1_MASK;
		}
		else {
			eventKey.state &= ~GDK_MOD1_MASK;
		}
	}

	// wxTODO _mouseEvents.updateStatusText(&eventKey);
}

void EventManager::updateStatusText(wxKeyEvent& ev, bool keyPress)
{
	// Make a copy of the given event key
	//GdkEventKey eventKey = *event;

	/*// Sometimes the ALT modifier is not set, so this is a workaround for this
	if (eventKey.keyval == GDK_Alt_L || eventKey.keyval == GDK_Alt_R)
	{
		if (keyPress)
		{
			eventKey.state |= GDK_MOD1_MASK;
		}
		else {
			eventKey.state &= ~GDK_MOD1_MASK;
		}
	}*/

	_mouseEvents.updateStatusText(ev);
}

void EventManager::onKeyPressWx(wxKeyEvent& ev)
{
	/*if (dynamic_cast<wxFrame*>(ev.GetEventObject()) != NULL)
	{
		wxFrame* window = static_cast<wxFrame*>(ev.GetEventObject());

		// Pass the key event to the connected window and see if it can process it (returns TRUE)
		bool keyProcessed = gtk_window_propagate_key_event(window->gobj(), ev);

		// Get the focus widget, is it an editable widget?
		Gtk::Widget* focus = window->get_focus();
		bool isEditableWidget = dynamic_cast<Gtk::Editable*>(focus) != NULL || dynamic_cast<Gtk::TextView*>(focus) != NULL;

		// Never propagate keystrokes if editable widgets are focused
		if ((isEditableWidget && ev->keyval != GDK_Escape) || keyProcessed)
		{
			return keyProcessed;
		}
	}*/

	// Try to find a matching accelerator
	AcceleratorList accelList = findAccelerator(ev);

	if (!accelList.empty())
	{
		// Release any modifiers
		_modifiers.clearState();

		// Fake a "non-modifier" event to the MouseEvents class
		//GdkEventKey eventKey = *ev;
		//eventKey.state &= ~(GDK_MOD1_MASK|GDK_SHIFT_MASK|GDK_CONTROL_MASK);
		//_mouseEvents.updateStatusText(&eventKey);

		// Pass the execute() call to all found accelerators
		for (AcceleratorList::iterator i = accelList.begin(); i != accelList.end(); ++i)
		{
			i->keyDown();
		}

		ev.StopPropagation();
		return;
	}

	_modifiers.updateState(ev, true);

	updateStatusText(ev, true);
}

void EventManager::onKeyReleaseWx(wxKeyEvent& ev)
{
	/*if (dynamic_cast<Gtk::Window*>(widget) != NULL)
	{
		Gtk::Window* window = static_cast<Gtk::Window*>(widget);

		// Pass the key event to the connected window and see if it can process it (returns TRUE)
		bool keyProcessed = gtk_window_propagate_key_event(window->gobj(), ev);

		// Get the focus widget, is it an editable widget?
		Gtk::Widget* focus = window->get_focus();
		bool isEditableWidget = dynamic_cast<Gtk::Editable*>(focus) != NULL || dynamic_cast<Gtk::TextView*>(focus) != NULL;

		// Never propagate keystrokes if editable widgets are focused
		if ((isEditableWidget && ev->keyval != GDK_Escape) || keyProcessed)
		{
			return keyProcessed;
		}
	}*/

	// Try to find a matching accelerator
	AcceleratorList accelList = findAccelerator(ev);

	if (!accelList.empty())
	{
		// Pass the execute() call to all found accelerators
		for (AcceleratorList::iterator i = accelList.begin(); i != accelList.end(); ++i)
		{
			i->keyUp();
		}

		ev.StopPropagation();
		return;
	}

	_modifiers.updateState(ev, false);

	updateStatusText(ev, false);
}

bool EventManager::onKeyPress(GdkEventKey* ev, Gtk::Widget* widget)
{
	if (dynamic_cast<Gtk::Window*>(widget) != NULL)
	{
		Gtk::Window* window = static_cast<Gtk::Window*>(widget);

		// Pass the key event to the connected window and see if it can process it (returns TRUE)
		bool keyProcessed = gtk_window_propagate_key_event(window->gobj(), ev);

		// Get the focus widget, is it an editable widget?
		Gtk::Widget* focus = window->get_focus();
		bool isEditableWidget = dynamic_cast<Gtk::Editable*>(focus) != NULL || dynamic_cast<Gtk::TextView*>(focus) != NULL;

		// Never propagate keystrokes if editable widgets are focused
		if ((isEditableWidget && ev->keyval != GDK_Escape) || keyProcessed)
		{
			return keyProcessed;
		}
	}

	// Try to find a matching accelerator
	AcceleratorList accelList = findAccelerator(/* wxTODO ev*/ 0,0);

	if (!accelList.empty())
	{
		// Release any modifiers
		_modifiers.clearState();

		// Fake a "non-modifier" event to the MouseEvents class
		GdkEventKey eventKey = *ev;
		eventKey.state &= ~(GDK_MOD1_MASK|GDK_SHIFT_MASK|GDK_CONTROL_MASK);
		// wxTODO _mouseEvents.updateStatusText(&eventKey);

		// Pass the execute() call to all found accelerators
		for (AcceleratorList::iterator i = accelList.begin(); i != accelList.end(); ++i)
		{
			i->keyDown();
		}

		return true;
	}

	_modifiers.updateState(ev, true);

	updateStatusText(ev, true);

	return false;
}

bool EventManager::onKeyRelease(GdkEventKey* ev, Gtk::Widget* widget)
{
	if (dynamic_cast<Gtk::Window*>(widget) != NULL)
	{
		Gtk::Window* window = static_cast<Gtk::Window*>(widget);

		// Pass the key event to the connected window and see if it can process it (returns TRUE)
		bool keyProcessed = gtk_window_propagate_key_event(window->gobj(), ev);

		// Get the focus widget, is it an editable widget?
		Gtk::Widget* focus = window->get_focus();
		bool isEditableWidget = dynamic_cast<Gtk::Editable*>(focus) != NULL || dynamic_cast<Gtk::TextView*>(focus) != NULL;

		// Never propagate keystrokes if editable widgets are focused
		if ((isEditableWidget && ev->keyval != GDK_Escape) || keyProcessed)
		{
			return keyProcessed;
		}
	}

	// Try to find a matching accelerator
	AcceleratorList accelList = findAccelerator(/* wxTODO ev*/0,0);

	if (!accelList.empty())
	{
		// Pass the execute() call to all found accelerators
		for (AcceleratorList::iterator i = accelList.begin(); i != accelList.end(); ++i)
		{
			i->keyUp();
		}

		return true;
	}

	_modifiers.updateState(ev, false);

	updateStatusText(ev, false);

	return false;
}

guint EventManager::getGDKCode(const std::string& keyStr)
{
	guint returnValue = gdk_keyval_to_upper(gdk_keyval_from_name(keyStr.c_str()));

	if (returnValue == GDK_VoidSymbol) {
		rWarning() << "EventManager: Could not recognise key " << keyStr << std::endl;
	}

	return returnValue;
}

bool EventManager::isModifier(wxKeyEvent& ev)
{
	int keyCode = ev.GetKeyCode();

	return (keyCode == WXK_SHIFT || keyCode == WXK_CONTROL ||
		keyCode == WXK_ALT || keyCode == WXK_WINDOWS_LEFT ||
		keyCode == WXK_WINDOWS_MENU || keyCode == WXK_WINDOWS_RIGHT);
}

namespace
{

// Took this from the wxWidgets examples
// helper function that returns textual description of wx virtual keycode
const char* getVirtualKeyCodeName(int keycode)
{
    switch ( keycode )
    {
#define WXK_(x) \
        case WXK_##x: return #x;

        WXK_(BACK)
        WXK_(TAB)
        WXK_(RETURN)
        WXK_(ESCAPE)
        WXK_(SPACE)
        WXK_(DELETE)
        WXK_(START)
        WXK_(LBUTTON)
        WXK_(RBUTTON)
        WXK_(CANCEL)
        WXK_(MBUTTON)
        WXK_(CLEAR)
        WXK_(SHIFT)
        WXK_(ALT)
        WXK_(CONTROL)
        WXK_(MENU)
        WXK_(PAUSE)
        WXK_(CAPITAL)
        WXK_(END)
        WXK_(HOME)
        WXK_(LEFT)
        WXK_(UP)
        WXK_(RIGHT)
        WXK_(DOWN)
        WXK_(SELECT)
        WXK_(PRINT)
        WXK_(EXECUTE)
        WXK_(SNAPSHOT)
        WXK_(INSERT)
        WXK_(HELP)
        WXK_(NUMPAD0)
        WXK_(NUMPAD1)
        WXK_(NUMPAD2)
        WXK_(NUMPAD3)
        WXK_(NUMPAD4)
        WXK_(NUMPAD5)
        WXK_(NUMPAD6)
        WXK_(NUMPAD7)
        WXK_(NUMPAD8)
        WXK_(NUMPAD9)
        WXK_(MULTIPLY)
        WXK_(ADD)
        WXK_(SEPARATOR)
        WXK_(SUBTRACT)
        WXK_(DECIMAL)
        WXK_(DIVIDE)
        WXK_(F1)
        WXK_(F2)
        WXK_(F3)
        WXK_(F4)
        WXK_(F5)
        WXK_(F6)
        WXK_(F7)
        WXK_(F8)
        WXK_(F9)
        WXK_(F10)
        WXK_(F11)
        WXK_(F12)
        WXK_(F13)
        WXK_(F14)
        WXK_(F15)
        WXK_(F16)
        WXK_(F17)
        WXK_(F18)
        WXK_(F19)
        WXK_(F20)
        WXK_(F21)
        WXK_(F22)
        WXK_(F23)
        WXK_(F24)
        WXK_(NUMLOCK)
        WXK_(SCROLL)
        WXK_(PAGEUP)
        WXK_(PAGEDOWN)
        WXK_(NUMPAD_SPACE)
        WXK_(NUMPAD_TAB)
        WXK_(NUMPAD_ENTER)
        WXK_(NUMPAD_F1)
        WXK_(NUMPAD_F2)
        WXK_(NUMPAD_F3)
        WXK_(NUMPAD_F4)
        WXK_(NUMPAD_HOME)
        WXK_(NUMPAD_LEFT)
        WXK_(NUMPAD_UP)
        WXK_(NUMPAD_RIGHT)
        WXK_(NUMPAD_DOWN)
        WXK_(NUMPAD_PAGEUP)
        WXK_(NUMPAD_PAGEDOWN)
        WXK_(NUMPAD_END)
        WXK_(NUMPAD_BEGIN)
        WXK_(NUMPAD_INSERT)
        WXK_(NUMPAD_DELETE)
        WXK_(NUMPAD_EQUAL)
        WXK_(NUMPAD_MULTIPLY)
        WXK_(NUMPAD_ADD)
        WXK_(NUMPAD_SEPARATOR)
        WXK_(NUMPAD_SUBTRACT)
        WXK_(NUMPAD_DECIMAL)
        WXK_(NUMPAD_DIVIDE)

        WXK_(WINDOWS_LEFT)
        WXK_(WINDOWS_RIGHT)
#ifdef __WXOSX__
        WXK_(RAW_CONTROL)
#endif
#undef WXK_

    default:
        return NULL;
    }
}

std::string getKeyString(wxKeyEvent& ev)
{
	int keycode = ev.GetKeyCode();
	const char* virtualKeyCodeName = getVirtualKeyCodeName(keycode);

	if (virtualKeyCodeName != NULL)
	{
        return virtualKeyCodeName;
	}

    if (keycode > 0 && keycode < 32)
	{
        return wxString::Format("Ctrl-%c", (unsigned char)('A' + keycode - 1)).ToStdString();
	}

    if (keycode >= 32 && keycode < 128)
	{
		return wxString::Format("%c", (unsigned char)keycode).ToStdString();
	}

	return "unknown";
}

} // namespace

std::string EventManager::getEventStr(wxKeyEvent& ev)
{
	std::string returnValue("");

	// Don't react on modifiers only (no actual key like A, 2, U, etc.)
	if (isModifier(ev))
	{
		return returnValue;
	}

	// Convert the GDKEvent state into modifier flags
	const unsigned int modifierFlags = _modifiers.getKeyboardFlags(ev);

	// Construct the complete string
	returnValue += _modifiers.getModifierStr(modifierFlags, true);
	returnValue += (returnValue != "") ? "-" : "";

	returnValue += getKeyString(ev);

	return returnValue;
}

extern "C" void DARKRADIANT_DLLEXPORT RegisterModule(IModuleRegistry& registry)
{
	registry.registerModule(EventManagerPtr(new EventManager));

	// Initialise the streams using the given application context
	module::initialiseStreams(registry.getApplicationContext());

	// Remember the reference to the ModuleRegistry
	module::RegistryReference::Instance().setRegistry(registry);

	// Set up the assertion handler
	GlobalErrorHandler() = registry.getApplicationContext().getErrorHandlingFunction();
}
