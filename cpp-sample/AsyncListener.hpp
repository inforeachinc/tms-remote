#include <chrono>
#include <stdexcept>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <tmsapigrpc/TMSRemote.grpc.pb.h>

#include "Utils.h"
#include "AsyncListenerBase.h"

using namespace Utils;

template <class Request, class Event> class AsyncListener : public AsyncListenerBase
{
public:
	AsyncListener(const std::string name, int startTag) :
        name_(name),
		logPrefix_("["+name+"] "),
        startTag_(startTag)
    {
        isDebug_.store(false);
        listenerIsUp_.store(false);
        listenerThread_.store(NULL);
        requestTagStart_     = reinterpret_cast<void*>(startTag_ + 1);
        requestTagSubscribe_ = reinterpret_cast<void*>(startTag_ + 2);
        requestTagRead_      = reinterpret_cast<void*>(startTag_ + 3);
        requestTagDone_      = reinterpret_cast<void*>(startTag_ + 4);
    }

	virtual ~AsyncListener()
	{
        if (listenerIsUp_.load())
        {
            signalStop("[~AsyncListener()] ");
        }
        waitForStop("[~AsyncListener()] ");
	}

    virtual void setDebug(bool debug)
    {
        isDebug_.store(debug);
    }

    void start(std::function< std::unique_ptr< ::grpc::ClientAsyncReaderWriter< Request, Event>>(grpc::ClientContext*, grpc::CompletionQueue*) > streamSupplier, const Request &request, std::function< bool(const Event&, const std::string&)> consumer)
    {
        listenerIsUp_.store(true);
        std::thread* pThread = listenerThread_.exchange(new std::thread(&AsyncListener::run, this, streamSupplier, request, consumer));
        if (pThread)
        {
            throw std::runtime_error("start() may only be called once");
        }
    }

	virtual void signalStop(const std::string &caller)
	{
        if (isDebug()) std::cout << get_timestamp() << caller << name_ << "Flagging listener thread to stop..." << std::endl;
        listenerIsUp_.store(false);
        if (isDebug()) std::cout << get_timestamp() << caller << name_ << "Listener thread stop flag is set" << std::endl;
    }

    virtual void waitForStop(const std::string& caller)
	{
        std::thread* pThread = listenerThread_.exchange(NULL);
        if (pThread)
        {
            if (isDebug()) std::cout << get_timestamp() << caller << name_ << "Terminating listener thread..." << std::endl;
            pThread->join();
            delete pThread;
            std::cout << get_timestamp() << caller << name_ << " thread is stopped" << std::endl;
        }
	}


private:
    bool isDebug()
    {
        return isDebug_.load();
    }

    void run(std::function< std::unique_ptr< ::grpc::ClientAsyncReaderWriter< Request, Event>>(grpc::ClientContext*, grpc::CompletionQueue*) > streamSupplier, const Request &request, std::function< bool(const Event&, const std::string&)> consumer)
    {
        static const int timeOutSeconds = 1;
        static const std::chrono::duration<long long> timeout = std::chrono::seconds(timeOutSeconds);

        Event event;
        std::unique_ptr< ::grpc::ClientAsyncReaderWriter< Request, Event>> rpc(streamSupplier(&context_, &queue_));
        if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Before StartCall()..." << std::endl;
        rpc->StartCall(requestTagStart_);
        bool result = true;
        void* responseTag;
        bool ok = false;
        grpc::CompletionQueue::NextStatus nextStatus;
        std::chrono::time_point<std::chrono::system_clock> deadline;
        do
        {
            if (!threadShouldContinue())
            {
                stopStream(rpc, timeout, false); // Don't skip any events, we haven't received any new events yet
                break;
            }
            deadline = std::chrono::system_clock::now() + timeout;
            if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Before AsyncNext()..." << std::endl;
            nextStatus = queue_.AsyncNext(&responseTag, &ok, deadline);
            if (!ok)
            {
                std::cout << get_timestamp() << logPrefix_ << "Subscription call is dead!" << std::endl;
                Event disconnectEvent;
                disconnectEvent.set_feedstatus(FeedStatus::Disconnected);
                consumer(disconnectEvent, logPrefix_);
                break;
            }
            if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After AsyncNext(), responseTag==" << std::dec << (long long)responseTag << std::endl;
            if (!threadShouldContinue())
            {
                stopStream(rpc, timeout, (nextStatus == grpc::CompletionQueue::NextStatus::GOT_EVENT && requestTagRead_ == responseTag)); // Skip event only if we've received it
                break;
            }
            switch (nextStatus)
            {
            case grpc::CompletionQueue::NextStatus::TIMEOUT:
                if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After AsyncNext(), status=TIMEOUT" << std::endl;
                result = true;
                continue;

            case grpc::CompletionQueue::NextStatus::GOT_EVENT:
                if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After AsyncNext(), status=GOT_EVENT" << std::endl;
                assert(ok);
                result = true;
                if (requestTagStart_ == responseTag)
                {
                    // StartCall() is completed, time to write subscription request to our bidirectional rpc call
                    std::cout << get_timestamp() << logPrefix_ << "Stream is up" << std::endl;
                    if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Before Write()..." << std::endl;
                    rpc->Write(request, requestTagSubscribe_);
                    if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After Write()" << std::endl;
                }
                else if (requestTagSubscribe_ == responseTag)
                {
                    // Write() is completed, time to start calling Read() to get data server generates for our subscription
                    std::cout << get_timestamp() << logPrefix_ << "Subscription is accepted" << std::endl;
                    if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Before Read()..." << std::endl;
                    rpc->Read(&event, requestTagRead_);
                    if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After Read()" << std::endl;
                }
                else if (requestTagRead_ == responseTag)
                {
                    // We've received data for previous Read() call, process it
                    result = consumer(event, logPrefix_);
                    if (result)
                    {
                        // Keep callling Read(), otherwise there will be no new events for our subscription
                        if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Before Read()..." << std::endl;
                        rpc->Read(&event, requestTagRead_);
                        if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Received data event" << std::endl;
                    }
                    else
                    {
                        std::cout << get_timestamp() << logPrefix_ << "Got 'stop' flag from callback" << std::endl;
                        // If we're here, we've definitely received event and need to skip it.
                        // Or not... Sometimes, skipping event causes crash inside gRPC libraries.
                        stopStream(rpc, timeout, false);
                    }
                }
                else
                {
                    std::cout << get_timestamp() << logPrefix_ << "Received unknown Completion Queue tag " << std::dec << (long long)responseTag << std::endl;
                    result = false;
                }
                break;

            case grpc::CompletionQueue::NextStatus::SHUTDOWN:
                if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After AsyncNext(), status==SHUTDOWN" << std::endl;
                result = false;
                break;

            default:
                if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After AsyncNext(), status==WTF?" << std::endl;
                result = false;
                break;
            }
        } while (result);
        if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Shutting down queue..." << std::endl;
        queue_.Shutdown();
        std::cout << get_timestamp() << logPrefix_ << "Stream completed" << std::endl;
    }

    bool threadShouldContinue()
    {
        if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Checking if continue..." << std::endl;
        bool goOn = listenerIsUp_.load();
        if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Continue==" << std::boolalpha << goOn << std::endl;
        return goOn;
    }

    void stopStream(const std::unique_ptr<grpc::ClientAsyncReaderWriter<Request, Event>>& rpc, const std::chrono::seconds& timeout, bool needToSkipEvent)
    {
        std::cout << get_timestamp() << logPrefix_ << "Detected request to stop" << std::endl;
        if (needToSkipEvent)
        {
            /* This leads to crashes on sample shutdown
            if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Skipping event from completion queue" << std::endl;
            Event event;
            rpc->Read(&event, requestTagRead_);
            */
        }
        if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Calling WritesDone()" << std::endl;
        // Let server know we're done with our stream
        rpc->WritesDone(requestTagDone_);
        std::chrono::time_point<std::chrono::system_clock> deadline;
        grpc::CompletionQueue::NextStatus nextStatus;
        bool ok;
        void* responseTag;
        // Wait for server to close the stream on its side
        do
        {
            if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "Waiting for server to close stream..." << std::endl;
            deadline = std::chrono::system_clock::now() + timeout;
            nextStatus = queue_.AsyncNext(&responseTag, &ok, deadline);
            if (isDebug()) std::cout << get_timestamp() << logPrefix_ << "After AsyncNext(), responseTag==" << std::dec << (long long)responseTag << std::endl;
        } while (ok && nextStatus == grpc::CompletionQueue::NextStatus::TIMEOUT);
    }

private:
    const std::string name_;
    const std::string logPrefix_;
    const int startTag_;

	void *requestTagStart_;
	void *requestTagSubscribe_;
	void *requestTagRead_;
	void *requestTagDone_;

	grpc::ClientContext context_;
	grpc::CompletionQueue queue_;

    std::atomic<bool> isDebug_;
    std::atomic<bool> listenerIsUp_;
    std::atomic<std::thread *> listenerThread_;
};
