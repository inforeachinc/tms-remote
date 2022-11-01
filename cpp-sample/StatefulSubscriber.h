#pragma once

#include <string>
#include <mutex>

/**
Example of stateful async subscriber.
Each instance will susbcribe to market data for a name and calculate interval VWAP for it.

Usage:
	AsyncListenersManager::setup(...);
...
	StatefulSubscriber vwapCalculator_IBM("IBM");
	vwapCalculator_IBM.start();
...
	vwapCalculator_IBM.stop();
...
	double intervalVwap_IBM = vwapCalculator_IBM.getIntervalVwap(); // Interval VWAP from start to stop
	double intervalAccumSize_IBM = vwapCalculator_IBM.getIntervalAccumSize(); // Interval VWAP from start to stop
*/

class MarketDataEvent;

class StatefulSubscriber
{
public:
	StatefulSubscriber(const std::string& recordName);
	~StatefulSubscriber();

	// Start listening for market data events for record
	void start();
	// Stop listening for market data events and updating internal state
	void stop(const std::string& caller);
	// Not necessary to call explicitly - but allowed if we want to free the thread early
	void terminate(const std::string& caller);

	const std::string &getName();
	double getIntervalVwap();
	long getIntervalAccumSize();

private:
	bool processMarketDataEvent(const MarketDataEvent& event, const std::string& caller);
	void applyUpdate(double lastPx, long lastSize);

private:
	std::string recordName_;

	// Mutex to ensure thread safety
	std::mutex stateLock_;

	// Internal state
	double accumulatedVal_;
	long accumulatedQty_;
	std::atomic<int> listenerId_;
	std::atomic<double> lastAccumSize_;
	std::atomic<double> startTime_;

	static const std::string FieldName_LastPx;
	static const std::string FieldName_LastSize;
	static const std::string FieldName_AccumSize;
	static const std::string FieldName_TradeTime;
};
