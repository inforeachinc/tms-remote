#pragma once

#include <tmsapigrpc/TMSRemote.grpc.pb.h>

/*
Usage example:

TMSRemote::Stub stub;
AsyncListenersManager::setup(TMSRemote::Stub stub);
AsyncListenersManager manager = AsyncListenersManager::getInstance();

int orderListenerId = manager.startListening(TMSRemote::Stub::PrepareAsyncsubscribeForOrders, ordersRequest, ordersConsumer);
int targetListenerId = manager.startListening(TMSRemote::Stub::PrepareAsyncsubscribeForMarketTargets, targetsRequest, targetsConsumer);

manager.stopListener(orderListenerId);
manager.stopAllListeners();
manager.terminate();
*/

class AsyncListenerBase;

class AsyncListenersManager
{
public:
	// Call setup(stub) before calling getInstance()
	static void setup(const TMSRemote::Stub &stub);
	static AsyncListenersManager &getInstance();

	template <class Request, class Event>
	int startListening(std::unique_ptr<grpc::ClientAsyncReaderWriter<Request, Event>> (TMSRemote::Stub::* stub_member_function)(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq), Request request, std::function< bool(const Event&, const std::string&)> consumer, const std::string &name, bool initialDebug = false);
	void setDebug(int listenerId, bool debug);
	void stopListener(int listenerId, const std::string &caller);
	void terminateListener(int listenerId, const std::string& caller);
	void stopAllListeners(const std::string &caller);
	void terminate(const std::string &caller);

public:
	// non-API methods to support Singleton pattern
	AsyncListenersManager(const AsyncListenersManager&) = delete;
	void operator=(const AsyncListenersManager&) = delete;

private:
	static AsyncListenersManager& createImpl(const TMSRemote::Stub *stub);
	AsyncListenersManager(const TMSRemote::Stub &stub);
	~AsyncListenersManager();
	AsyncListenerBase *getListener(int listenerId);
	int getNextId();
private:
	const TMSRemote::Stub &stub_;
	std::map<int, AsyncListenerBase *> idToListenerMap_;
	std::mutex mapLock_;
	std::atomic<int> counter_;
};
