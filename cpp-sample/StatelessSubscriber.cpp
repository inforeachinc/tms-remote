#include "AsyncListener.hpp"
#include "AsyncListenersManager.h"
#include "AsyncListenersManager.hpp"
#include "Utils.h"

#include "StatelessSubscriber.h"

using namespace Utils;

int StatelessSubscriber::start(const std::string& name, bool debug)
{
	SubscribeForPortfoliosRequest request;

	// Start listener thread and save listener ID
	// Use processPortfolioEvent() member function as callback method for target events. Since it's static method, we'll directly use pointer to it
	return AsyncListenersManager::getInstance().startListening<SubscribeForPortfoliosRequest, PortfolioEvent>(&TMSRemote::Stub::PrepareAsyncsubscribeForMarketPortfolios, request, &processPortfolioEvent, name, debug);
}

void StatelessSubscriber::stop(int listenerId, const std::string& caller)
{
	AsyncListenersManager::getInstance().stopListener(listenerId, caller);
}

void StatelessSubscriber::terminate(int listenerId, const std::string& caller)
{
	AsyncListenersManager::getInstance().terminateListener(listenerId, caller);
}

bool StatelessSubscriber::processPortfolioEvent(const PortfolioEvent& event, const std::string& caller)
{
	switch (event.event_case())
	{
	case PortfolioEvent::EventCase::kAdded:
		{
			auto update = event.added();
			std::string pfName = update.portfolioname();
			std::cout << get_timestamp() << caller << "Portfolio event: added " << pfName << std::endl;
		}
		break;
	case PortfolioEvent::EventCase::kRemoved:
		{
			auto update = event.removed();
			std::string pfName = update.portfolioname();
			std::cout << get_timestamp() << caller << "Portfolio event: removed " << pfName << std::endl;
		}
		break;
	case PortfolioEvent::EventCase::kUpdated:
		{
			auto update = event.updated();
			std::string pfName = update.portfolioname();
			std::cout << get_timestamp() << caller << "Portfolio event: updated " << pfName << std::endl;
		}
		break;
	default:
		break;
	}
	return true;
}
