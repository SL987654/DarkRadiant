#include "ModuleLoader.h"

#include "itextstream.h"
#include <iostream>
#include "imodule.h"

#include "os/dir.h"
#include "os/path.h"

#include "DynamicLibraryLoader.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/case_conv.hpp>

namespace module {

	namespace {
		const std::string PLUGINS_DIR = "plugins/"; ///< name of plugins directory
		const std::string MODULES_DIR = "modules/"; ///< name of modules directory
	}

// Constructor sets platform-specific extension to match
Loader::Loader(const std::string& path) :
	_path(path),
#if defined(WIN32)
	  _ext(".dll")
#elif defined(POSIX)
	  _ext(".so")
#endif
{}

// Functor operator, gets invoked on directory traversal
void Loader::operator() (const boost::filesystem::path& file) const
{
	// Check for the correct extension of the visited file
	if (boost::algorithm::to_lower_copy(file.extension().string()) == _ext)
	{
		std::string fullName = file.string();
		rMessage() << "ModuleLoader: Loading module '" << fullName << "'" << std::endl;

		// Create the encapsulator class
		DynamicLibraryPtr library(new DynamicLibrary(fullName));

		// greebo: Invoke the library loader, which will add the library to the list
		// on success. If the load fails, the shared pointer doesn't get added and
		// self-destructs at the end of this scope.
		DynamicLibraryLoader(library, _dynamicLibraryList);
	}
}

/** Load all of the modules in the DarkRadiant install directory. Modules
 * are loaded from modules/ and plugins/.
 *
 * @root: The root directory to search.
 */
void Loader::loadModules(const std::string& root) {

    // Get standardised paths
    std::string stdRoot = os::standardPathWithSlash(root);
    std::string modulesPath = stdRoot + MODULES_DIR;
    std::string pluginsPath = stdRoot + PLUGINS_DIR;

    // Load modules and plugins
	Loader modulesLoader(modulesPath);
	Loader pluginsLoader(pluginsPath);

	os::foreachItemInDirectory(modulesPath, modulesLoader);

    // Plugins are optional, so catch the exception
    try
	{
    	os::foreachItemInDirectory(pluginsPath, pluginsLoader);
    }
    catch (os::DirectoryNotFoundException&)
	{
        std::cout << "Loader::loadModules(): plugins directory '"
                  << pluginsPath << "' not found." << std::endl;
    }
}

void Loader::unloadModules()
{
	while (!_dynamicLibraryList.empty())
	{
		DynamicLibraryPtr lib = _dynamicLibraryList.back();

		_dynamicLibraryList.pop_back();

		lib.reset();
	}
}

// Initialise the static DLL list
DynamicLibraryList Loader::_dynamicLibraryList;

} // namespace module
