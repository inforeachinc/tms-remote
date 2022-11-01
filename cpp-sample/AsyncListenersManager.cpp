#include "AsyncListenerBase.h"
#include "AsyncListenersManager.h"


// Call setup(stub) before calling getInstance()
void AsyncListenersManager::setup(const TMSRemote::Stub &stub)
{
	createImpl(&stub);
}

AsyncListenersManager &AsyncListenersManager::createImpl(const TMSRemote::Stub *stub)
{
	// Call setup(stub) before calling getInstance()
	static AsyncListenersManager instance(*stub);
	return instance;
}

// Call setup(stub) before calling getInstance()
AsyncListenersManager &AsyncListenersManager::getInstance()
{
	return createImpl(NULL);
}

AsyncListenersManager::AsyncListenersManager(const TMSRemote::Stub &stub) :
	stub_(stub)
{
}

AsyncListenersManager::~AsyncListenersManager()
{
	static const std::string caller("[~AsyncListenersManager]");
	stopAllListeners(caller);
	terminate(caller);
}

void AsyncListenersManager::setDebug(int listenerId, bool debug)
{
	AsyncListenerBase* listener = getListener(listenerId);
	if (listener)
	{
		listener->setDebug(debug);
	}
}

void AsyncListenersManager::stopListener(int listenerId, const std::string &caller)
{
	AsyncListenerBase *listener = getListener(listenerId);
	if (listener)
	{
		listener->signalStop(caller);
	}
}

void AsyncListenersManager::terminateListener(int listenerId, const std::string& caller)
{
	AsyncListenerBase* listener = getListener(listenerId);
	if (listener)
	{
		listener->waitForStop(caller);
	}
}

void AsyncListenersManager::stopAllListeners(const std::string &caller)
{
	mapLock_.lock();
	std::for_each(idToListenerMap_.begin(), idToListenerMap_.end(), [caller](std::pair<int, AsyncListenerBase*> element) {element.second->signalStop(caller); });
	mapLock_.unlock();
}

void AsyncListenersManager::terminate(const std::string &caller)
{
	mapLock_.lock();
	std::for_each(idToListenerMap_.begin(), idToListenerMap_.end(), [caller](std::pair<int, AsyncListenerBase*> element) {element.second->waitForStop(caller); });
	mapLock_.unlock();
}

int AsyncListenersManager::getNextId()
{
	static const int startId = 1000;
	static const int increment = 100;
	int count = counter_++;
	return startId + count*increment;
}

AsyncListenerBase *AsyncListenersManager::getListener(int listenerId)
{
	mapLock_.lock();
	AsyncListenerBase *result = idToListenerMap_[listenerId];
	mapLock_.unlock();
	return result;
}
