#include "AsyncListenersManager.h"

template <class Request, class Event>
int AsyncListenersManager::startListening(std::unique_ptr<grpc::ClientAsyncReaderWriter<Request, Event>>(TMSRemote::Stub::* stub_member_function)(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq), Request request, std::function< bool(const Event&, const std::string&)> consumer, const std::string& name, bool initialDebug)
{
	int baseRequestId = getNextId();

	// Create async listener for events
	AsyncListener<Request, Event>* listener = new AsyncListener<Request, Event>(name, baseRequestId);
	listener->setDebug(initialDebug);

	// Bind stub member function to our instance of the stub
	using std::placeholders::_1;
	using std::placeholders::_2;
	std::function< std::unique_ptr< ::grpc::ClientAsyncReaderWriter<Request, Event>>(grpc::ClientContext*, grpc::CompletionQueue*) > streamSupplier = std::bind(stub_member_function, stub_, _1, _2);

	// Store listener in the map
	mapLock_.lock();
	idToListenerMap_[baseRequestId] = listener;
	mapLock_.unlock();

	// Start listening
	listener->start(streamSupplier, request, consumer);

	return baseRequestId;
}
