#pragma once

#include "iradiant.h"
#include <memory>

namespace map {

class StartupMapLoader: public sigc::trackable
{
public:
	// This gets called as soon as the mainframe starts up
	void onRadiantStartup();

	// Called when the mainframe shuts down
	void onRadiantShutdown();
};
typedef std::shared_ptr<StartupMapLoader> StartupMapLoaderPtr;

} // namespace map
