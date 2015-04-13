#pragma once

#include <list>
#include <vector>
#include "imodule.h"

// A list containing possible values for a combo box widgets
typedef std::list<std::string> ComboBoxValueList;
typedef std::vector<std::string> IconList;
typedef std::vector<std::string> IconDescriptionList;

/* greebo: This is the interface the preference page has to provide for adding
 * elements to the dialog page. */
class PreferencesPage
{
public:
    // destructor
	virtual ~PreferencesPage() {}

	/** greebo: Allows to set a custom title of this page. The default title
	 * 			upon construction is "guessed" by adding a " Settings" to
	 * 			the page name, "Settings/Patch" gets assigned
	 * 			a "Patch Settings" as default title.
	 * 			Use this method to change this to fit your needs.
	 */
	virtual void setTitle(const std::string& title) = 0;

	// greebo: Use this to add a checkbox to the preference dialog that is connected to a registry value
	virtual void appendCheckBox(const std::string& name, const std::string& flag, const std::string& registryKey) = 0;

	/* greebo: This adds a horizontal slider to the internally referenced VBox and connects
	 * it to the given registryKey. */
	virtual void appendSlider(const std::string& name, const std::string& registryKey, bool drawValue,
							  double value, double lower, double upper,
							  double step_increment, double page_increment, double page_size) = 0;

   /**
    * \brief
    * Add a drop-down combo box to the preference page.
    *
    * \param name
    * The name to be displayed next to the combo box.
    *
    * \param registryKey
    * The registry key which stores the value of the combo box.
    *
    * \param valueList
    * List of strings containing the values that should be displayed in the
    * combo box.
    *
    * \param storeValueNotIndex
    * If true, store the selected text in the registry key. If false, store the
    * numeric index of the selected item in the registry key. The default is
    * false.
    */
   virtual void appendCombo(const std::string& name,
                            const std::string& registryKey,
                            const ComboBoxValueList& valueList,
                            bool storeValueNotIndex = false) = 0;

	/* greebo: Appends an entry field with <name> as caption which is connected to the given registryKey
	 */
	virtual void appendEntry(const std::string& name, const std::string& registryKey) = 0;

	/* greebo: Appends an entry field with spinner buttons which retrieves its value from the given
	 * RegistryKey. The lower and upper values have to be passed as well.
	 */
	virtual void appendSpinner(const std::string& name, const std::string& registryKey,
									 double lower, double upper, int fraction) = 0;

	// greebo: Adds a PathEntry to choose files or directories (depending on the given boolean)
	virtual void appendPathEntry(const std::string& name,
									   const std::string& registryKey,
									   bool browseDirectories) = 0;

	// Appends a static label (to add some text to the preference page)
	virtual void appendLabel(const std::string& caption) = 0;
};
typedef std::shared_ptr<PreferencesPage> PreferencesPagePtr;

const std::string MODULE_PREFERENCESYSTEM("PreferenceSystem");

class IPreferenceSystem :
	public RegisterableModule
{
public:
	/** greebo: Retrieves the page for the given path, for example:
	 *
	 * 			"Settings/Patch Settings"
	 * 			(spaces are ok, slashes are treated as delimiters, don't use them)
	 *
	 * 			Use the PreferencesPage interface to add widgets
	 * 			and connect them to the registry.
	 *
	 * @path: The path to lookup
	 *
	 * @returns: the PreferencesPage pointer.
	 */
	virtual PreferencesPagePtr getPage(const std::string& path) = 0;
};

inline IPreferenceSystem& GlobalPreferenceSystem() {
	// Cache the reference locally
	static IPreferenceSystem& _prefSystem(
		*std::static_pointer_cast<IPreferenceSystem>(
			module::GlobalModuleRegistry().getModule(MODULE_PREFERENCESYSTEM)
		)
	);
	return _prefSystem;
}
