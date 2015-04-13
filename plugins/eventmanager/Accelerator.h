#pragma once

#include "ieventmanager.h"

#include "Event.h"

#include <list>
#include <iostream>

/* greebo: An Accelerator consists of a key/modifier combination plus a connected Event object.
 *
 * Use the match() method to test if the accelerator matches a certain key/modifier combination.
 * Use the connectCommand() method to assign a command to this accelerator.
 * Use the keyUp()/keyDown() methods to trigger the keyup/keydown command callbacks.
 */

class Accelerator :
	public IAccelerator
{
	// The internally stored key/modifier combination
	unsigned int _key;
	unsigned int _modifiers;

	// The connected event
	IEventPtr _event;

public:
	// Construct an accelerator out of the key/modifier plus a command
	Accelerator(const unsigned int key, const unsigned int modifiers, const IEventPtr& event);

	// Copy Constructor
	Accelerator(const Accelerator& other);

	// Destructor
	virtual ~Accelerator() {}

	// Returns true if the key/modifier combination matches this accelerator
	bool match(const unsigned int key, const unsigned int modifiers) const;

	// Returns true if the event is attached to this Accelerator
	bool match(const IEventPtr& event) const;

	// Reads out the interal key/modifier combination of this Accelerator
	unsigned int getKey() const;
	unsigned int getModifiers() const;

	// Make the accelerator use this key/modifier
	void setKey(const unsigned int key);
	void setModifiers(const unsigned int modifiers);

	// Connect this modifier to the specified command
	void connectEvent(const IEventPtr& event);

	// Retrieve the contained event pointer
	IEventPtr getEvent();

	// Call the connected event keyup/keydown callbacks
	void keyUp();
	void keyDown();

	/**
	 * Converts a string representation of a key to the corresponding
	 * wxKeyCode, e.g. "A" => 65, "TAB" => wxKeyCode::WXK_TAB.
	 *
	 * Note that "a" will also result in the uppercase value of 'A' == 65.
	 *
	 * In GDK we had gdk_keyval_from_name, which seems to be missing
	 * in wxWidgets.
	 *
	 * Will return WXK_NONE if no conversion exists.
	 */
	static unsigned int getKeyCodeFromName(const std::string& name);

	/**
	 * Converts the given keycode to a string to serialize it into
	 * a settings or XML file. The given keycode will be converted
	 * to its uppercase pendant if possible. 'a' => 'A'
	 */
	static std::string getNameFromKeyCode(unsigned int keyCode);
};

typedef std::list<Accelerator> AcceleratorList;
