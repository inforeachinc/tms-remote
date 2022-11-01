#include "AsyncListener.hpp"
#include "AsyncListenersManager.h"
#include "AsyncListenersManager.hpp"

#include "StatefulSubscriber.h"


const std::string StatefulSubscriber::FieldName_LastPx("LastPx");
const std::string StatefulSubscriber::FieldName_LastSize("LastSize");
const std::string StatefulSubscriber::FieldName_AccumSize("AccumSize");
const std::string StatefulSubscriber::FieldName_TradeTime("TradeTime");

StatefulSubscriber::StatefulSubscriber(const std::string& recordName) :
	recordName_(recordName),
	accumulatedVal_(0),
	accumulatedQty_(0)
{
	listenerId_.store(0);
	lastAccumSize_.store(-1);
}

StatefulSubscriber::~StatefulSubscriber()
{
	static const std::string caller("[~StatefulSubscriber()]");
	stop(caller);
	terminate(caller);
}

// IMPORTANT: some thread should call AsyncListenersManager::setup() before calling StatefulSubscriber::start()
void StatefulSubscriber::start()
{
	// Use processMarketDataEvent() member function as callback method for target events
	// Since it's non-static method, we'll use lambda to capture this and bind method call to this
	std::function< bool(const MarketDataEvent&, const std::string&)> marketDataConsumer = [&](const MarketDataEvent& event, const std::string& caller) { return processMarketDataEvent(event, caller); };

	SubscribeForMarketDataRequest request;
	// Set record name
	request.add_instrument(getName());
	// Set list of fields we want to receive
	request.add_field(FieldName_LastPx);
	request.add_field(FieldName_LastSize);
	request.add_field(FieldName_AccumSize);
	request.add_field(FieldName_TradeTime);

    // Save subscription time - we'll use it later in processMarketDataEvent() to check if the first Market Data update is a trade
	double now = (double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	startTime_.store(now);

    // Start listener thread and save listener ID
	listenerId_.store(AsyncListenersManager::getInstance().startListening(&TMSRemote::Stub::PrepareAsyncsubscribeForMarketData, request, marketDataConsumer, "Market Data Listener for "+getName()));
}

void StatefulSubscriber::stop(const std::string& caller)
{
	int listenerId = listenerId_.load();
	if (listenerId > 0)
	{
		AsyncListenersManager::getInstance().stopListener(listenerId, caller);
	}
}

void StatefulSubscriber::terminate(const std::string& caller)
{
	int listenerId = listenerId_.exchange(0);
	if (listenerId > 0)
	{
		AsyncListenersManager::getInstance().terminateListener(listenerId, caller);
	}
}

const std::string& StatefulSubscriber::getName()
{
	return recordName_;
}

double StatefulSubscriber::getIntervalVwap()
{
	double accVal;
	long accQty;
	stateLock_.lock();
	accVal = accumulatedVal_;
	accQty = accumulatedQty_;
	stateLock_.unlock();
	return (accQty == 0) ? NAN : accVal/(double)accQty;
}

long StatefulSubscriber::getIntervalAccumSize()
{
	long accQty;
	stateLock_.lock();
	accQty = accumulatedQty_;
	stateLock_.unlock();
	return accQty;
}

bool StatefulSubscriber::processMarketDataEvent(const MarketDataEvent& event, const std::string& caller)
{
	switch (event.event_case())
	{
	case MarketDataEvent::EventCase::kUpdate:
		{
			auto update = event.update();
			auto fields = update.fields();
			auto numericfields = fields.numericfields();
			// We cannot apply each update because some of updates are not trades.
			// And if we apply updates that aren't trades, we'll apply some trades more than once.
			// To check if an update is a trade, let's check if accum size has increased.
			double currentAccumSize = numericfields[FieldName_AccumSize];
			double prevAccumSize = lastAccumSize_.exchange(currentAccumSize);
			if (currentAccumSize > prevAccumSize)
			{
				if (prevAccumSize < 0)
				{
					// It's first update.
					// To find out if the first udpate is a trade, let's compare its trade time with our subscription time.
					double tradeTime = numericfields[FieldName_TradeTime];
					double startTime = startTime_.load();
					if (tradeTime > startTime)
					{
						// It's a trade.
						applyUpdate(numericfields[FieldName_LastPx], (long)numericfields[FieldName_LastSize]);
					}
				}
				else
				{
					// It's a trade.
					applyUpdate(numericfields[FieldName_LastPx], (long)numericfields[FieldName_LastSize]);
				}
			}
		}
		break;
	default:
		break;
	}
	return true;
}

void StatefulSubscriber::applyUpdate(double lastPx, long lastSize)
{
	if ((lastPx != 0) && (lastSize != 0))
	{
		stateLock_.lock();
		accumulatedVal_ += lastPx*lastSize;
		accumulatedQty_ += lastSize;
		stateLock_.unlock();
	}
}
