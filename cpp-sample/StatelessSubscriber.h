#pragma once

#include <string>
#include <mutex>

/**
Example of stateless async subscriber.
No instances needed since there's no state.

Usage:
	AsyncListenersManager::setup(...);
...
	int listenerId = StatelessSubscriber.start();
...
	StatelessSubscriber::stop(listenerId, "thread name");
*/

class PortfolioEvent;

class StatelessSubscriber
{
public:
	// Start listening for events
	// Returns listener ID
	static int start(const std::string& name, bool debug = false);
	// Stop listening for events
	static void stop(int listenerId, const std::string& caller);
	// Not necessary to call explicitly - but allowed if we want to free the thread early
	static void terminate(int listenerId, const std::string& caller);

private:
	static bool processPortfolioEvent(const PortfolioEvent& event, const std::string& caller);
};
#pragma once
