#include <string>
#include <iostream>
#include <fstream>
#include <ctime>
#include <chrono>
#include <thread>

#include <grpcpp/grpcpp.h>

#include <tmsapigrpc/TMSTradingRequests.pb.h>
#include <tmsapigrpc/TMSRemoteRequests.pb.h>
#include <tmsapigrpc/TMSRemote.grpc.pb.h>

#include "Utils.h"
#include "AsyncListener.hpp"
#include "AsyncListenersManager.h"
#include "AsyncListenersManager.hpp"
#include "StatefulSubscriber.h"
#include "StatelessSubscriber.h"

using namespace Utils;

// Comment/uncomment USE_MANAGED_LISTENERS to switch between AsyncListenersManager and manual async subscirptions
#define USE_MANAGED_LISTENERS

namespace
{
    std::string get_file_contents(const std::string& file_name)
    {
        std::ifstream inf(file_name);
        return std::string(std::istreambuf_iterator<char>(inf), std::istreambuf_iterator<char>());
    }

};


class TMSRemoteClient
{
public:
    TMSRemoteClient(std::shared_ptr<::grpc::Channel> channel) :
        client_(TMSRemote::NewStub(channel))
    {
        keepProcessingOrders_.store(true);
        isDebug_.store(false);
    }

//START SNIPPET: Create Market Portfolio
    bool create_market_portfolio(const std::string& portfolio_name)
    {
        ::grpc::ClientContext context;
        ::CreateMarketPortfolioRequest request;
        ::Void response;

        request.set_name(portfolio_name);
        request.set_type(::PortfolioType::Market);

        auto status = client_->createMarketPortfolio(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to create market portfolio: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Create Market Portfolio

//START SNIPPET: Modify Portfolio
    bool modify_market_portfolio(const std::string& portfolio_name)
    {
        grpc::ClientContext context;
        ModifyPortfolioRequest request;
        Void response;

        request.set_name(portfolio_name);
        Fields* fields = request.mutable_fields();
        auto stringfields = fields->mutable_stringfields();
        (*stringfields)["TrnDestinationAlias"] = "Tally";

        auto status = client_->modifyMarketPortfolio(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to modify market portfolio: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Modify Portfolio

//START SNIPPET: Remove Portfolio
    bool remove_market_portfolio(const std::string& portfolio_name)
    {
        grpc::ClientContext context;
        RemovePortfolioRequest request;
        Void response;

        request.set_name(portfolio_name);

        auto status = client_->removeMarketPortfolio(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to modify market portfolio: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Remove Portfolio

//START SNIPPET: Add Market Target
    long long add_market_target(const std::string& portfolio_name, const std::string& instrument, ::Side side, double qty)
    {
        ::grpc::ClientContext context;
        ::AddMarketTargetsRequest request;
        ::TargetIds response;

        request.set_portfolio(portfolio_name);
        ::Fields* fields = request.add_fields();
        auto numericfields = fields->mutable_numericfields();
        auto stringfields = fields->mutable_stringfields();
        (*stringfields)["Instrument"] = instrument;
        (*numericfields)["Side"] = side;
        (*numericfields)["TgtQty"] = qty;
        (*numericfields)["TgtOrdType"] = ::OrdType::OrdType_Market;

        auto status = client_->addMarketTargets(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to add market target: " << status.error_message() << std::endl;
        }

        return (response.targetid_size() > 0) ? response.targetid(0) : -1;
    }
//END SNIPPET: Add Market Target

//START SNIPPET: Remove Market Target
    bool remove_market_target(long long target_id)
    {
        ::grpc::ClientContext context;
        TargetIds request;
        Void response;

        request.add_targetid(target_id);

        auto status = client_->removeMarketTargets(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to remove market target: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Remove Market Target

//START SNIPPET: Pause or Resume Market Target
    bool pause_market_target(long long target_id, bool pause)
    {
        ::grpc::ClientContext context;
        Void response;

        grpc::Status status;
        if (pause)
        {
            PauseMarketTargetsRequest request;
            request.add_targetid(target_id);
            request.set_cancelopenorders(false);
            status = client_->pauseMarketTargets(&context, request, &response);
        }
        else
        {
            ResumeMarketTargetsRequest request;
            request.add_targetid(target_id);
            client_->resumeMarketTargets(&context, request, &response);
        }

        if (!status.ok())
        {
            std::cout << "unable to " << (pause ? "pause" : "resume") << " market target: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Pause or Resume Market Target

//START SNIPPET: Modify Market Targets
    bool modify_market_target(long long target_id, const std::string& destination, ::WaveSizeType wave_size_type, double wave_size)
    {
        ::grpc::ClientContext context;
        ::ModifyTargetsRequest request;
        ::Void response;

        request.add_targetid(target_id);
        ::Fields* fields = request.add_fields();
        auto numericfields = fields->mutable_numericfields();
        auto stringfields = fields->mutable_stringfields();
        (*stringfields)["TrnDestination"] = destination;
        (*numericfields)["WaveSizeType"] = wave_size_type;
        (*numericfields)["WaveSize"] = wave_size;

        auto status = client_->modifyMarketTargets(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to modify market target: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Modify Market Targets

//START SNIPPET: Get one Market Target
    bool get_market_target(long long target_id)
    {
        grpc::ClientContext context;
        ::TargetIds request;
        ::Targets response;

        request.add_targetid(target_id);

        auto status = client_->getMarketTargets(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to get market target: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Get one Market Target

    /**
    An example of how to NOT do subscriptions.
    This method uses synchronous Read() calls that will block indefinitely if there's no new data
    */
    void subscribe_for_orders()
    {
        const std::string caller("[WRONG Order Listener] ");
        ::grpc::ClientContext context;
        ::SubscribeForOrdersRequest request;
        ::OrderEvent event;

        auto stream = client_->subscribeForOrders(&context);
        stream->Write(request);

        std::cout << get_timestamp() << caller << "Calling first Read()..." << std::endl;
        while (stream->Read(&event))
        {
            if (isDebug()) std::cout << get_timestamp() << caller << "After Read(), checking if continue..." << std::endl;
            bool goOn = keepProcessingOrders_.load();
            std::cout << get_timestamp() << caller << "Read() completed, continue==" << std::boolalpha << goOn << std::endl;
            if (!goOn)
            {
                std::cout << get_timestamp() << caller << "Detected request to stop" << std::endl;
                if (isDebug()) std::cout << get_timestamp() << caller << "Calling WritesDone()" << std::endl;
                // Let server know we're done with our stream
                stream->WritesDone();
                // Wait for server to close the stream on its side
                do
                {
                    if (isDebug()) std::cout << get_timestamp() << caller << "Waiting for server to close stream..." << std::endl;
                } while (stream->Read(&event));
                break;
            }
            if (isDebug()) std::cout << get_timestamp() << caller << "Before process_order()" << std::endl;
            process_order(event, caller);
            if (isDebug()) std::cout << get_timestamp() << caller << "After process_order()" << std::endl;
            /*
            Unexpected behavior: Setting deadline does nothing.
            */
            std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
            context.set_deadline(deadline);
            std::cout << get_timestamp() << caller << "Calling next Read()..." << std::endl;
        }
        std::cout << get_timestamp() << caller << "Terminating stream..." << std::endl;
        auto status = stream->Finish();
        if (status.ok())
        {
            std::cout << get_timestamp() << caller << "Stream termination successful" << std::endl;
        }
        else
        {
            std::cout << get_timestamp() << caller << "Stream termination error: Received status " << status.error_message() << std::endl;
        }
        std::cout << get_timestamp() << caller << "Stream completed" << std::endl;
    }

    bool process_order(const ::OrderEvent& event, const std::string &caller)
    {
        if (!keepProcessingOrders_.load())
            return false;
        switch (event.event_case())
        {
        // Process newly added orders
        case ::OrderEvent::EventCase::kAdded:
            {
                if (isDebug()) std::cout << get_timestamp() << caller << "process_order(kAdded) " << std::endl;
                auto added = event.added();

                std::string order_id = added.orderid();
                auto fields = added.fields();
                auto numericfields = fields.numericfields();
                double orderQty = numericfields["OrdQty"];
                double cumQty = numericfields["FillQty"];

                std::cout << get_timestamp() << caller << "Received New Order notification: order ID=" << order_id << ", OrderQty=" << orderQty << ", CumQty=" << cumQty << std::endl;

                double leavesQty = orderQty - cumQty;
                if (leavesQty <= 300)
                {
                    if (leavesQty > 0)
                    {
                        cancel_order(order_id, caller);
                    }
                    else
                    {
                        std::cout << get_timestamp() << caller << "Will not cancel order with ID=" << order_id << ": it's completely filled." << std::endl;
                    }
                }
                else
                {
                    double newQty = orderQty - 100;
                    if (newQty >= cumQty)
                    {
                        modify_order(order_id, "FILL_100", newQty, caller);
                    }
                    else
                    {
                        std::cout << get_timestamp() << caller << "Will not modify order with ID=" << order_id << ": its CumQty is too close to OrderQty." << std::endl;
                    }
                }
            }
            break;

        case ::OrderEvent::EventCase::kFeedStatus:
            if (isDebug()) std::cout << get_timestamp() << caller << "process_order(kFeedStatus) " << std::endl;
            switch (event.feedstatus())
            {
            case ::FeedStatus::Disconnected:
                if (isDebug()) std::cout << get_timestamp() << caller << "Stream Disconnected" << std::endl;
                break;
            case ::FeedStatus::InitialStateReceived:
                if (isDebug()) std::cout << get_timestamp() << caller << "Stream InitialStateReceived" << std::endl;
                break;
            case ::FeedStatus::Reconnected:
                if (isDebug()) std::cout << get_timestamp() << caller << "Stream Reconnected" << std::endl;
                break;
            default:
                break;
            }
            break;
        case ::OrderEvent::EventCase::kUpdated:
            {
                auto updated = event.updated();
                std::string order_id = updated.orderid();
                std::cout << get_timestamp() << caller << "Order " << order_id << " is updated " << std::endl;
            }
            break;
        case ::OrderEvent::EventCase::kFilteredOut:
            if (isDebug()) std::cout << get_timestamp() << caller << "process_order(kFilteredOut) " << std::endl;
            break;
        case ::OrderEvent::EventCase::EVENT_NOT_SET:
            if (isDebug()) std::cout << get_timestamp() << caller << "process_order(EVENT_NOT_SET) " << std::endl;
            break;
        default:
            if (isDebug()) std::cout << get_timestamp() << caller << "process_order(WTF?) " << std::endl;
            break;
        }
        return keepProcessingOrders_.load();
    }

    bool cancel_order(const std::string& order_id, const std::string &caller)
    {
        ::grpc::ClientContext context;
        ::CancelOrdersRequest request;
        ::Void response;

        request.add_orderid(order_id);

        std::cout << get_timestamp() << caller << "Canceling order with ID=" << order_id << "...";
        auto status = client_->cancelOrders(&context, request, &response);

        if (status.ok())
        {
            std::cout << "OK" << std::endl;
        }
        else
        {
            std::cout << std::endl << get_timestamp() << caller << "Unable to cancel order with ID=" << order_id << ": " << status.error_message() << std::endl;
        }

        return status.ok();
    }

//START SNIPPET: Modify Order
    bool modify_order(const std::string& order_id, const std::string& text, double newQty, const std::string &caller)
    {
        ::grpc::ClientContext context;
        ::ModifyOrdersRequest request;
        ::Void response;

        request.add_orderid(order_id);
        ::FIXFields* message = request.add_message();
        auto stringfields = message->mutable_stringfields();
        auto numericfields = message->mutable_numericfields();
        (*stringfields)[::FIXTag::FIXTag_Text] = text;
        (*numericfields)[::FIXTag::FIXTag_OrderQty] = newQty;

        std::cout << get_timestamp() << caller << "Modifying order with ID=" << order_id << "...";
        auto status = client_->modifyOrders(&context, request, &response);

        if (status.ok())
        {
            std::cout << "OK" << std::endl;
        }
        else
        {
            std::cout << std::endl << get_timestamp() << caller << "Unable to modify order with ID=" << order_id << " to new qty=" << newQty << ": " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Modify Order

//START SNIPPET: Fill Order
    bool fill_order(const std::string& order_id, double fillQty, double fillPx)
    {
        ::grpc::ClientContext context;
        ::FIXMessagesRequest request;
        ::Void response;

        ::FIXFields* message = request.add_message();
        auto stringfields = message->mutable_stringfields();
        (*stringfields)[::FIXTag::FIXTag_SingleOrderTransactionId] = order_id;
        auto numericfields = message->mutable_numericfields();
        (*numericfields)[::FIXTag::FIXTag_LastQty] = fillQty;
        (*numericfields)[::FIXTag::FIXTag_LastPx] = fillPx;

        std::cout << get_timestamp() << "Filling order with ID=" << order_id << " with " << fillQty << "@" << fillPx << "..." << std::endl;
        auto status = client_->fillOrders(&context, request, &response);

        if (!status.ok())
        {
            std::cout << std::endl << get_timestamp() << "Unable to fill order with ID=" << order_id << ": " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Fill Order

//START SNIPPET: Stop All Trading
    void stop_all_trading()
    {
        grpc::ClientContext context;
        Void request;
        Void response;
        client_->stopAllTrading(&context, request, &response);
    }
//END SNIPPET: Stop All Trading

//START SNIPPET: Load Report
    bool load_report_if_absent(const std::string& managerName, const std::string& reportName, const std::string& reportLocation)
    {
        grpc::ClientContext context;
        ReportRequest availableRequest;
        availableRequest.set_domainmanagername(managerName);
        availableRequest.set_reportname(reportName);
        ReportAvailableResponse availableResponse;
        std::cout << get_timestamp() << "Checking for presense of report '" << reportName << "' on manager '" << managerName << "'..." << std::endl;
        grpc::Status status = client_->isReportAvailable(&context, availableRequest, &availableResponse);
        if (status.ok())
        {
            if (!availableResponse.available())
            {
                // For some reason, gRPC crashes if context is reused
                grpc::ClientContext anotherContext;
                CreateReportRequest createRequest;
                createRequest.set_domainmanagername(managerName);
                createRequest.set_reportspecresource(reportLocation);
                Void createResponse;
                std::cout << get_timestamp() << "Loading report from resource '" << reportLocation << "'..." << std::endl;
                // Reusing status might be OK, but I don't want to gamble
                grpc::Status anotherStatus = client_->createReport(&anotherContext, createRequest, &createResponse);
                if (!anotherStatus.ok())
                {
                    std::cout << std::endl << get_timestamp() << "Unable to load report: " << anotherStatus.error_message() << std::endl;
                }
                return anotherStatus.ok();
            }
        }
        else
        {
            std::cout << std::endl << get_timestamp() << "Unable to check report availability: " << status.error_message() << std::endl;
        }
        return false;
    }
//END SNIPPET: Load Report

//START SNIPPET: Use Security Master
    double get_close_price(const std::string& instrumentName)
    {
        double result = std::numeric_limits<double>::quiet_NaN();
        grpc::ClientContext context;
        InstrumentInfosRequest request;
        request.add_instrument(instrumentName);
        InstrumentInfosResponse response;
        grpc::Status status = client_->getInstrumentInfos(&context, request, &response);
        if (status.ok())
        {
            if (response.instrumentinfo_size() == 1)
            {
                google::protobuf::Map<std::string, double>::const_iterator iter = response.instrumentinfo()[0].numericfields().find("ClosePx");
                if (iter != response.instrumentinfo()[0].numericfields().end())
                {
                    result = iter->second;
                }
            }
        }
        else
        {
            std::cout << std::endl << get_timestamp() << "Unable to check report availability: " << status.error_message() << std::endl;
        }
        return result;
    }
//END SNIPPET: Use Security Master

    bool send_target_order(long long target_id)
    {
        ::grpc::ClientContext context;
        ::SendOrdersRequest request;
        ::Void response;

        request.add_targetid(target_id);

        auto status = client_->sendOrders(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to send target order: " << status.error_message() << std::endl;
        }

        return status.ok();
    }

//START SNIPPET: Send Order for Target
    bool send_target_order_with_params(long long target_id, const std::string& text)
    {
        ::grpc::ClientContext context;
        ::SendOrdersRequest request;
        ::Void response;

        request.add_targetid(target_id);
        ::FIXFields* message = request.add_message();
        auto stringfields = message->mutable_stringfields();
        (*stringfields)[::FIXTag::FIXTag_Text] = text;

        auto status = client_->sendOrders(&context, request, &response);

        if (!status.ok())
        {
            std::cout << "unable to send target order: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Send Order for Target

//START SNIPPET: Process Market Target events
    bool process_target(const TargetEvent &event, const std::string& caller)
    {
        switch (event.event_case())
        {
            case ::TargetEvent::EventCase::kAdded:
            {
                auto added = event.added();
                auto fields = added.fields();
                auto stringfields = fields.stringfields();
                auto numericfields = fields.numericfields();

                std::cout
                    << get_timestamp() << caller
                    << numericfields["TgtID"] << ", "
                    << stringfields["Portfolio"] << ", "
                    << stringfields["Instrument"] << ", "
                    << numericfields["TgtQty"] << ", "
                    << numericfields["FillQty"] << std::endl;
            }
            break;

            case ::TargetEvent::EventCase::kFeedStatus:
                if (isDebug()) std::cout << get_timestamp() << caller << "Received feed status " << event.feedstatus() << ", waiting for " << ::FeedStatus::InitialStateReceived << std::endl;
                if (event.feedstatus() == ::FeedStatus::InitialStateReceived)
                {
                    /*
                    We have received initial targets snapshot
                    We don't want to receive streaming target updates, so let's signal to cancel the stream
                    */
                    std::cout << get_timestamp() << caller << "Market Targets snapshot completed" << std::endl;
                    return false;
                }
                break;
        }
        return true;
    }
//END SNIPPET: Process Market Target events

    void setUpdateRate(int updatesPerSecond)
    {
        grpc::ClientContext context;
        SendCommandToCustomDataProvidersRequest request;
        request.set_datasourcename("MarketData");
        request.set_command("setUpdateRate");
        auto properties = request.mutable_commandproperties();
        auto stringfields = properties->mutable_stringfields();
        (*stringfields)[std::to_string(updatesPerSecond)] = "NULL";
        Void response;
        auto status = client_->sendCommandToCustomDataProviders(&context, request, &response);
        if (!status.ok())
        {
            std::cout << "unable to set update rate: " << status.error_message() << std::endl;
        }
    }

//START SNIPPET: Subscribe to Market Target events
    void list_market_targets_sync(const std::string &caller)
    {
        ::grpc::ClientContext context;
        ::SubscribeForTargetsRequest request;
        ::TargetEvent event;

        request.add_field("TgtID");
        request.add_field("Portfolio");
        request.add_field("Instrument");
        request.add_field("TgtQty");
        request.add_field("FillQty");

        auto stream = client_->subscribeForMarketTargets(&context);
        stream->Write(request);

        std::cout << std::endl << std::endl << get_timestamp() << caller << "Reqest sent" << std::endl;
        int count = 0;

//START SNIPPET: Detecting unsubscription from Market Target events by server
        while (stream->Read(&event))
//END SNIPPET: Detecting unsubscription from Market Target events by server
        {
            if (!process_target(event, caller))
            {
//START SNIPPET: Terminating subscription to Market Target events
                std::cout << get_timestamp() << caller << "Received total of " << count << " market targets in sync snapshot" << std::endl;
                if (isDebug()) std::cout << get_timestamp() << caller << "Calling WritesDone()" << std::endl;
                // Let server know we're done with our stream
                stream->WritesDone();
                // Wait for server to close the stream on its side
                do
                {
                    if (isDebug()) std::cout << get_timestamp() << caller << "Waiting for server to close stream..." << std::endl;
                } while (stream->Read(&event));
//END SNIPPET: Terminating subscription to Market Target events
            }
            count++;
        }
        std::cout << get_timestamp() << caller << "Terminating stream..." << std::endl;
        auto status = stream->Finish();
        if (status.ok())
        {
            std::cout << get_timestamp() << caller << "Stream termination successful" << std::endl;
        }
        else
        {
            std::cout << get_timestamp() << caller << "Stream termination error: Received status " << status.error_message() << std::endl;
        }
        std::cout << get_timestamp() << caller << "Stream completed" << std::endl;
    }
//END SNIPPET: Subscribe to Market Target events

//START SNIPPET: Login
    bool login(const std::string& user, const std::string& password)
    {
        ::grpc::ClientContext context;
        ::LoginRequest login_request;
        ::Void response;

        login_request.set_user(user);
        login_request.set_password(password);

        auto status = client_->login(&context, login_request, &response);

        if (!status.ok())
        {
            std::cout << "unable to login: " << status.error_message() << std::endl;
        }

        return status.ok();
    }
//END SNIPPET: Login

    void stopProcessingOrders(const std::string &caller)
    {
        if (isDebug()) std::cout << get_timestamp() << caller<< "Marking order processor as inactive..." << std::endl;
        keepProcessingOrders_.store(false);
        if (isDebug()) std::cout << get_timestamp() << caller << "Order processor stop flag is set" << std::endl;
    }

    void setDebug(bool debug)
    {
        isDebug_.store(debug);
    }

    bool isDebug()
    {
        return isDebug_.load();
    }

public:
     std::unique_ptr<TMSRemote::Stub> client_;
private:
    std::atomic<bool> keepProcessingOrders_;
    std::atomic<bool> isDebug_;
};


int main()
{
    static const bool debug = false;
    static const bool useSyncOrderListener = false; // Set to true to reproduce the problem with synchronous stream: blocking Read() call in subscribe_for_orders() method
    static const std::string caller = "[Main] ";

    auto ssl_options = grpc::SslCredentialsOptions();
    ssl_options.pem_root_certs = get_file_contents("cert.pem");
    auto channel = ::grpc::CreateChannel("localhost:8083", ::grpc::SslCredentials(ssl_options));
    TMSRemoteClient client(channel);
    client.setDebug(debug);

    const std::string PORTFOLIO = "Test " + std::to_string(std::time(nullptr));
    const std::string TMP_PORTFOLIO = PORTFOLIO+" - tmp";

    client.login("demo", "");

    using std::placeholders::_1;
    using std::placeholders::_2;

    // Create subscription request for orders
    // This is default request: No filtering, all fields will be present in each OrderEvent
    SubscribeForOrdersRequest ordersRequest;
    // Use client.process_order() member function as callback method for order events
    std::function< bool(const OrderEvent&, const std::string&)> ordersConsumer = [&client](const OrderEvent& event, const std::string& caller) { return client.process_order(event, caller); };

//START SNIPPET: Get Market Targets - prepare request
    // Create subscription request for market targets
    // No filtering, but we specify the fields that will be present in each TargetEvent
    SubscribeForTargetsRequest targetsRequest;
    targetsRequest.add_field("TgtID");
    targetsRequest.add_field("Portfolio");
    targetsRequest.add_field("Instrument");
    targetsRequest.add_field("TgtQty");
    targetsRequest.add_field("FillQty");
//END SNIPPET: Get Market Targets - prepare request
//START SNIPPET: Get Market Targets - prepare callback
    // Use client.process_target() member function as callback method for target events
    std::function< bool(const TargetEvent&, const std::string&)> targetsConsumer = [&client](const TargetEvent& event, const std::string& caller) { return client.process_target(event, caller); };
//END SNIPPET: Get Market Targets - prepare callback

#ifdef USE_MANAGED_LISTENERS
//START SNIPPET: Get Market Targets - setup listeners manager
    AsyncListenersManager::setup(*client.client_);
    AsyncListenersManager& manager = AsyncListenersManager::getInstance();
//END SNIPPET: Get Market Targets - setup listeners manager
    int orderListenerId = manager.startListening(&TMSRemote::Stub::PrepareAsyncsubscribeForOrders, ordersRequest, ordersConsumer, "Managed Orders Listener", debug);
//START SNIPPET: Get Market Targets - send subscription request
    int targetListenerId = manager.startListening(&TMSRemote::Stub::PrepareAsyncsubscribeForMarketTargets, targetsRequest, targetsConsumer, "Managed Targets Listener", debug);
//END SNIPPET: Get Market Targets - send subscription request

    // Check StatefulSubscriber class for an example of stateful listener
    StatefulSubscriber vwapCalculator_IBM("IBM");
    StatefulSubscriber vwapCalculator_MSFT("MSFT");
    vwapCalculator_IBM.start();
    vwapCalculator_MSFT.start();

    // Increase simulated update rate so our VWAP calculators have something to crunch on
    client.setUpdateRate(10);

    // Check StatelessSubscriber class for an example of stateless listener
    int statelessPortfolioListenerId1 = StatelessSubscriber::start("Stateless Portfolio Listener 1", debug);
    int statelessPortfolioListenerId2 = StatelessSubscriber::start("Stateless Portfolio Listener 2", debug);
#else
    // Create async listener for order events
    AsyncListener< SubscribeForOrdersRequest, OrderEvent> ordersListener("Async Orders Listener", 11000);
    ordersListener.setDebug(debug);
    // Declare local variables for RPC stream supplier and OrderEvent consumer - for convenience only
    std::function< std::unique_ptr< ::grpc::ClientAsyncReaderWriter< SubscribeForOrdersRequest, OrderEvent>>(grpc::ClientContext*, grpc::CompletionQueue*) > ordersStreamSupplier = std::bind(&TMSRemote::Stub::PrepareAsyncsubscribeForOrders, *client.client_, _1, _2);
    // We will use shutdown option 1 for targetsListener: stop async listener thread directly using ordersListener.signalStop()
    ordersListener.start(ordersStreamSupplier, ordersRequest, ordersConsumer);

    // Create async listener for market target events
    AsyncListener< SubscribeForTargetsRequest, TargetEvent> targetsListener("Async Targets Listener", 12000);
    targetsListener.setDebug(debug);
    // Declare local variables for RPC stream supplier and TargetEvent consumer - for convenience only
    std::function< std::unique_ptr< ::grpc::ClientAsyncReaderWriter< SubscribeForTargetsRequest, TargetEvent>>(grpc::ClientContext *, grpc::CompletionQueue *) > targetsStreamSupplier = std::bind(&TMSRemote::Stub::PrepareAsyncsubscribeForMarketTargets, *client.client_, _1, _2);
    // We will use shutdown option 2 for targetsListener: its consumer callback will return "false".
    // It's relatively safe in this case, because we close the stream as soon as initial snapshot is received.
    // But generally, this might result in blocked Read() call if AsyncListener.signalStop() is never called
    // See also comments for 'useSyncOrderListener' parameter.
    targetsListener.start(targetsStreamSupplier, targetsRequest, targetsConsumer);
#endif // USE_MANAGED_LISTENERS

    // Let async listener pick up current state
    std::this_thread::sleep_for(std::chrono::seconds(3));
    /*
    Have a target for 800 shares
    Set wave size to 50% of the target size
    Send first wave - one order of 400 shares will be sent, and it will start to be filled.
    Send second wave with "NOFILL" parameter - another order of 400 shares will be sent, but it will not be filled.
    Every time our order listener is notified of a new order, it tries to decrease its quantity by 100 or to cancel it, depending on order original quantity and filled quantity.
    */
    client.create_market_portfolio(PORTFOLIO);
    long long target_id = client.add_market_target(PORTFOLIO, "VOD LN", ::Side::Side_Buy, 800);
    client.modify_market_target(target_id, "Simulator1", ::WaveSizeType::WaveSizeType_PctTgtQty, 50);

    client.create_market_portfolio(TMP_PORTFOLIO);

#ifdef USE_MANAGED_LISTENERS
    // Stop one of the interval VWAP calculators before another
    vwapCalculator_IBM.stop(caller);
#endif // USE_MANAGED_LISTENERS

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << std::endl << get_timestamp() << caller << "Sending first order..." << std::endl;
    client.send_target_order(target_id);
    client.modify_market_portfolio(TMP_PORTFOLIO);
    client.remove_market_target(target_id); // This should fail because target has open orders
    client.pause_market_target(target_id + 38463, true); // This should fail because target ID is absent
    client.fill_order("absent_order_id", 100, 123.45); // This should fail because order ID does not exist
    client.load_report_if_absent("Portfolio report manager", "Portfolio Orders by Instrument", "/ElTrader/TMS/Common/Reports/Portfolio Orders by Instrument.reportspec");
    double ibm_close_px = client.get_close_price(vwapCalculator_IBM.getName());

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << std::endl << get_timestamp() << caller << "Sending second order..." << std::endl;
    client.send_target_order_with_params(target_id, "NOFILL");

    std::this_thread::sleep_for(std::chrono::seconds(1));
    client.remove_market_portfolio(TMP_PORTFOLIO);

    // Use synchronous listener to list market targets.
    // It's relatively safe in this case, because we close the stream as soon as initial snapshot is received.
    // But generally, synchronous listeners might result in blocked Read() call - see comments for 'useSyncOrderListener' parameter.
    // NOTE: if there's no other interaction with the system, this listener will find one extra target compared to async targets listener because we've created a new target after async target listener has finished listening.
    client.list_market_targets_sync(caller+"Sync Targets Listener: ");

    std::cout << std::endl << get_timestamp() << caller << "Terminating..." << std::endl;

#ifdef USE_MANAGED_LISTENERS
    // Stateful subscriber - stop
    vwapCalculator_IBM.stop(caller);
    vwapCalculator_MSFT.stop(caller);

    // Explicitly stopping stateless susbcriber is not necessary: it uses managed susbcriber and will be stopped when manager.stopAllListeners() is called.
    // Stateless subscriber - stop
    StatelessSubscriber::stop(statelessPortfolioListenerId1, caller);

//START SNIPPET: Get Market Targets - cancel subscription
    // If necessary, individual listeners can be stopped using stopListener() call
    manager.stopListener(targetListenerId, caller);
//END SNIPPET: Get Market Targets - cancel subscription
    // But there's a convenience call that stops all active listeners
    manager.stopAllListeners(caller);

    // Wait for all managed listener threads to terminate
    manager.terminate(caller);

    std::cout << get_timestamp() << caller << "Record " << vwapCalculator_IBM.getName() << ": IntervalVWAP=" << vwapCalculator_IBM.getIntervalVwap() << " compared to ClosePx=" << ibm_close_px << ", IntervalAccumSize=" << vwapCalculator_IBM.getIntervalAccumSize() << std::endl;
    std::cout << get_timestamp() << caller << "Record " << vwapCalculator_MSFT.getName() << ": IntervalVWAP=" << vwapCalculator_MSFT.getIntervalVwap() << " , IntervalAccumSize=" << vwapCalculator_MSFT.getIntervalAccumSize() << std::endl;
#else
    // Use shutdown option 1 for targetsListener: stop async listener thread directly using ordersListener.signalStop()
    ordersListener.signalStop(caller);
    // We used shutdown option 2 for targetsListener: its consumer callback will return "false", calling targetsListener.signalStop() is not mandatory, but is still safe and recommended
    targetsListener.signalStop(caller);

    // Wait for all individual listener threads to terminate
    ordersListener.waitForStop(caller);
    targetsListener.waitForStop(caller);
#endif // USE_MANAGED_LISTENERS

    if (useSyncOrderListener)
    {
        std::cout << std::endl << get_timestamp() << caller << "Creating sync Orders subscriber to reproduce blocking Read() call..." << std::endl;
        auto orderListenerThread = std::thread(&TMSRemoteClient::subscribe_for_orders, std::ref(client));
        const int waitTime = 5;
        std::cout << get_timestamp() << caller << "Sleeping for " << waitTime << " seconds to allow sync Orders subscriber to read all existing events..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(waitTime));
        client.stopProcessingOrders(caller);
        std::cout << std::endl << get_timestamp() << caller << "Signaled sync Orders subscriber to stop, waiting for it to terminate..." << std::endl;
        // join() call might take minutes or not return at all - because orderListenerThread is blocked in Read() call
        // The sample still can be terminated with BREAK signal (Ctrl+C), but this will cause server exception:
        // io.grpc.StatusRuntimeException: CANCELLED: cancelled before receiving half close
        orderListenerThread.join();
        std::cout << get_timestamp() << caller << "Sync Orders subscriber thread is terminated" << std::endl;
    }

    client.stop_all_trading();
    std::cout << std::endl << get_timestamp() << caller << "Completed" << std::endl;
    return 0;
}
