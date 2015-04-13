#pragma once

#include "iradiant.h"
#include "icommandsystem.h"

#include <memory>

namespace radiant
{

class RadiantThreadManager;

/// IRadiant implementation class.
class RadiantModule :
	public IRadiant
{
    // Our signals
    sigc::signal<void> _radiantStarted;
    sigc::signal<void> _radiantShutdown;

    // Thread manager instance
    mutable std::unique_ptr<RadiantThreadManager> _threadManager;

public:

    /// Broadcast shutdwon signal and clear all listeners
	void broadcastShutdownEvent();

	/// Broadcasts startup signal
	void broadcastStartupEvent();

    // IRadiant implementation
    sigc::signal<void> signal_radiantStarted() const;
    sigc::signal<void> signal_radiantShutdown() const;
    ThreadManager& getThreadManager();
	void performLongRunningOperation(
		const std::function<void(ILongRunningOperation&)>& operationFunc,
		const std::string& title);

	// RegisterableModule implementation
	const std::string& getName() const;
	const StringSet& getDependencies() const;
	void initialiseModule(const ApplicationContext& ctx);
	void shutdownModule();

	// Stuff to be done after the modules have been loaded
	void postModuleInitialisation();

	void registerUICommands();

	// Target method bound to the "Exit" command
	static void exitCmd(const cmd::ArgumentList& args);
};
typedef std::shared_ptr<RadiantModule> RadiantModulePtr;

/**
 * Return the global Radiant module (for use internally, not by other
 * modules).
 */
RadiantModulePtr getGlobalRadiant();

} // namespace radiant
