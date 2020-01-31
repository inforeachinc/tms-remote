/*
 * Copyright (c) 1997-2018 InfoReach, Inc. All Rights Reserved.
 *
 * This software is the confidential and proprietary information of
 * InfoReach ("Confidential Information").  You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered into
 * with InfoReach.
 *
 * CopyrightVersion 2.0
 */

package snippet;

import org.apache.log4j.Logger;

import com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon;
import com.inforeach.eltrader.tms.api.grpc.TMSRemoteEvents;

import io.grpc.stub.StreamObserver;

class TMSClientAppStreamObservers
{
    private static final Logger LOGGER = Logger.getLogger(TMSClientApp.class);

    static abstract class AbstractStreamObserver<EVENT> implements StreamObserver<EVENT>
    {

        @Override
        public void onError(Throwable t)
        {
            LOGGER.error(this.getClass().getSimpleName() + " error: " + t.getMessage(), t);
        }

        @Override
        public void onCompleted()
        {
            LOGGER.debug(this.getClass().getSimpleName() + " completed");
        }

        @Override
        public void onNext(EVENT event)
        {
            LOGGER.debug(this.getClass().getSimpleName() + " received event: " + event);
            processEvent(event);
        }

        protected abstract void processEvent(EVENT event);
    }

    //START SNIPPET: Market targets event observer
    static class MarketTargetStreamObserver extends  AbstractStreamObserver<TMSRemoteEvents.TargetEvent>
    {
        @Override
        protected void processEvent(TMSRemoteEvents.TargetEvent targetEvent)
        {
            //Example of target event processing:
            switch (targetEvent.getEventCase())
            {
                case ADDED:
                    final TMSRemoteEvents.TargetEvent.TargetAddedEvent addedEvent = targetEvent.getAdded();
                    addedEvent.getFields();
                    addedEvent.getTargetId();
                    break;
                case UPDATED:
                    final TMSRemoteEvents.TargetEvent.TargetUpdatedEvent updatedEvent= targetEvent.getUpdated();
                    updatedEvent.getFields();
                    updatedEvent.getTargetId();
                    break;
                case REMOVED:
                    final TMSRemoteEvents.TargetEvent.TargetRemovedEvent removedEvent = targetEvent.getRemoved();
                    removedEvent.getTargetId();
                    break;
                case FILTEREDOUT:
                    final TMSRemoteEvents.TargetEvent.TargetFilteredOutEvent  filteredOutEvent = targetEvent.getFilteredOut();
                    filteredOutEvent.getTargetId();
                    break;
                case PAUSED:
                    final TMSRemoteEvents.TargetEvent.TargetPausedEvent pausedEvent = targetEvent.getPaused();
                    pausedEvent.getTargetId();
                    break;
                case RESUMED:
                    final TMSRemoteEvents.TargetEvent.TargetResumedEvent resumedEvent = targetEvent.getResumed();
                    resumedEvent.getTargetId();
                    break;
                case FEEDSTATUS:
                    switch (targetEvent.getFeedStatus())
                    {
                        case Disconnected:
                            break;
                        case Reconnected:
                            break;
                        case InitialStateReceived:
                            break;
                    }
                    break;
            }
        }
    }
    //END SNIPPET: Market targets event observer

    static class CategoryPositionEventStreamObserver extends AbstractStreamObserver<TMSRemoteEvents.CategoryPositionEvent>
    {
        @Override
        protected void processEvent(TMSRemoteEvents.CategoryPositionEvent categoryPositionEvent)
        {
            switch (categoryPositionEvent.getEventCase())
            {
                case ADDED:
                {
                    final TMSRemoteEvents.CategoryPositionEvent.CategoryPositionAddedEvent added = categoryPositionEvent.getAdded();
                    final String categoryType = added.getCategoryType();
                    final String categoryName = added.getCategoryName();
                    final TMSRemoteCommon.Fields fields = added.getFields();
                    final double longQty = fields.getNumericFieldsMap().get("TradedLongQty");
                    break;
                }
                case UPDATED:
                {
                    final TMSRemoteEvents.CategoryPositionEvent.CategoryPositionUpdatedEvent updated = categoryPositionEvent.getUpdated();
                    final String categoryType = updated.getCategoryType();
                    final String categoryName = updated.getCategoryName();
                    final TMSRemoteCommon.Fields fields = updated.getFields();
                    final double longQty = fields.getNumericFieldsMap().get("TradedLongQty");
                    break;
                }
                case FEEDSTATUS:
                    switch (categoryPositionEvent.getFeedStatus())
                    {
                        case Disconnected:
                            break;
                        case Reconnected:
                            break;
                        case InitialStateReceived:
                            break;
                    }
                    break;
            }
        }
    }

    static class InstrumentPositionEventStreamObserver extends AbstractStreamObserver<TMSRemoteEvents.InstrumentPositionEvent>
    {
        @Override
        protected void processEvent(TMSRemoteEvents.InstrumentPositionEvent instrumentPositionEvent)
        {
            switch (instrumentPositionEvent.getEventCase())
            {
                case ADDED:
                {
                    final TMSRemoteCommon.Fields fields = instrumentPositionEvent.getAdded().getFields();
                    final String instrument = instrumentPositionEvent.getAdded().getInstrument();
                    final double longQty = fields.getNumericFieldsMap().get("TradedLongQty");
                    break;
                }
                case UPDATED:
                {
                    final TMSRemoteCommon.Fields fields = instrumentPositionEvent.getUpdated().getFields();
                    final String instrument = instrumentPositionEvent.getUpdated().getInstrument();
                    final double longQty = fields.getNumericFieldsMap().get("TradedLongQty");
                    break;
                }
                case FEEDSTATUS:
                    switch (instrumentPositionEvent.getFeedStatus())
                    {
                        case Disconnected:
                            break;
                        case Reconnected:
                            break;
                        case InitialStateReceived:
                            break;
                    }
                    break;
            }
        }
    }

    //START SNIPPET: Market data event observer
    static class MarketDataStreamObserver implements StreamObserver<TMSRemoteEvents.MarketDataEvent>
    {
        @Override
        public void onNext(TMSRemoteEvents.MarketDataEvent marketDataEvent)
        {
            switch (marketDataEvent.getEventCase())
            {
                case UPDATE:
                {
                    TMSRemoteEvents.MarketDataEvent.MarketDataUpdate update = marketDataEvent.getUpdate();
                    String instrument = update.getInstrument();
                    TMSRemoteCommon.Fields fields = update.getFields();
                    double lastPx = fields.getNumericFieldsMap().get("LastPx");
                    break;
                }
                case FEEDSTATUS:
                    switch (marketDataEvent.getFeedStatus())
                    {
                        case Disconnected:
                            break;
                        case Reconnected:
                            break;
                    }
                    break;
            }
        }

        @Override
        public void onError(Throwable t)
        {
            t.printStackTrace();
        }

        @Override
        public void onCompleted()
        {
        }
    }
    //END SNIPPET: Market data event observer

    //START SNIPPET: FIXMessage event observer
    static class FIXMessagesStreamObserver implements StreamObserver<TMSRemoteEvents.FIXEvent>
    {
        @Override
        public void onNext(TMSRemoteEvents.FIXEvent fixEvent)
        {
            if (fixEvent.getEventCase() == TMSRemoteEvents.FIXEvent.EventCase.MESSAGE)
            {
                final TMSRemoteCommon.FIXFields fields = fixEvent.getMessage().getFields();
                LOGGER.debug(this.getClass().getSimpleName() + " received FIX message = " + fields);
            }
        }

        @Override
        public void onError(Throwable t)
        {
            t.printStackTrace();
        }

        @Override
        public void onCompleted()
        {
        }
    }
    //START SNIPPET: FIXMessage event observer
}
