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

import com.inforeach.eltrader.tms.api.grpc.*;
import io.grpc.ManagedChannel;
import io.grpc.Metadata;
import io.grpc.StatusRuntimeException;
import io.grpc.netty.GrpcSslContexts;
import io.grpc.netty.NettyChannelBuilder;
import io.grpc.stub.StreamObserver;
import org.apache.log4j.Logger;
import org.supercsv.io.CsvMapReader;
import org.supercsv.prefs.CsvPreference;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.FIXTag.FIXTag_OrdType_VALUE;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.FIXTag.FIXTag_Price_VALUE;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.OrdType.OrdType_Market_VALUE;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.WaveSizeType.WaveSizeType_PctTgtQty_VALUE;

/**
 * Sample client app for TMS gRPC remote client
 *
 * The example does the following steps:
 * <ol>
 * <li> Create a portfolio with targets from file
 * <li> Modify targets TrnDestination=Simulator1
 * <li> Subscribe to orders, targets flow and MarketData
 * <li> Send Wave e.g. 10% of tgt qty, limit orders with Px instructions BidPx:AskPx
 * <li> Modify price to MidPx for all orders open longer than N seconds since the last request
 * <li> Modify order type to Market for all orders open longer than M seconds after the prev step
 * <li> In MarketData listener compare LastPx and Price of open orders for current instrument. If (LastPx-Price)/LastPx > 0.01 then modify order price to MidPx
 * <li> Once an order is filled goto #4 for it
 * <li> Once a target is completed send alert to the user
 * <li> If Text field is updated with STOP value then cancel open orders for this target and stop trading it.
 * </ol>
 */
public class TMSClientApp
{
    private static final Logger LOGGER = Logger.getLogger(TMSClientApp.class);

    private static final String FILE_NAME = "grpcSample.csv";
    private static final Set<String> STRING_FIELDS = new HashSet<>(Arrays.asList("Instrument", "ClientName", "SetPxTo"));
    private static final String PORTFOLIO = "grpcSample - " + new SimpleDateFormat("yyyyddMM HH:mm:ss").format(new Date());
    private static final String USER = "demo";
    private static final String PASSWORD = "";
    private static final long MID_PX_TIMEOUT = 2000;
    private static final long MKT_ORD_TYPE_TIMEOUT = 3000;

    private static final Timer TIMER = new Timer("Timer");
    private static TMSRemoteGrpc.TMSRemoteBlockingStub CLIENT;

    static abstract class AbstractStreamObserver<EVENT> implements StreamObserver<EVENT>
    {
        private final String name_;

        public AbstractStreamObserver(String name)
        {
            name_ = name;
        }

        @Override
        public void onError(Throwable t)
        {
            LOGGER.error(this.getClass().getSimpleName() + " " + name_ + " error: " + t.getMessage(), t);
        }

        @Override
        public void onCompleted()
        {
            LOGGER.debug(this.getClass().getSimpleName() + " " + name_ + " completed");
        }

        @Override
        public void onNext(EVENT event)
        {
//            LOGGER.debug(this.getClass().getSimpleName() + " " + name_ + " received event: " + event);
            processEvent(event);
        }

        protected abstract void processEvent(EVENT event);
    }

    private static class Target
    {
        private final long targetId_;
        private final CountDownLatch latch_;
        private String openOrderId_;
        private double unreleased_;
        private boolean stopped_;

        public Target(long targetId, TMSRemoteCommon.Fields fields, CountDownLatch latch)
        {
            targetId_ = targetId;
            latch_ = latch;
            unreleased_ = fields.getNumericFieldsOrDefault("Unreleased", 0);
        }

        public void onUpdated(TMSRemoteCommon.Fields fields)
        {
            if (!stopped_)
                unreleased_ = fields.getNumericFieldsOrDefault("Unreleased", unreleased_);

            if ("STOP".equals(fields.getStringFieldsOrDefault("Text", null)))
            {
                stopped_ = true; // to do not update unreleased
                unreleased_ = 0;

                if (openOrderId_ != null)
                {
                    LOGGER.info("Stopping target " +targetId_);
                    CLIENT.cancelOrders(TMSTradingRequests.CancelOrdersRequest.newBuilder()
                        .addOrderId(openOrderId_)
                        .build());
                }
                else
                {
                    LOGGER.warn("Cannot stop target " +targetId_ + ", it has no open orders");
                }
            }
        }

        public void onOrderAdded(String orderId)
        {
            if (openOrderId_ != null)
                LOGGER.info("Warning: Target " + targetId_ + " already has open order");
            openOrderId_ = orderId;

            if (stopped_)
            {
                LOGGER.warn("New order " + orderId + " added to already stopped target " + targetId_ + ", canceling");
                CLIENT.cancelOrders(TMSTradingRequests.CancelOrdersRequest.newBuilder()
                    .addOrderId(openOrderId_)
                    .build());
            }
        }

        public void onOrderClosed(String orderId)
        {
            if (orderId.equals(openOrderId_))
            {
                openOrderId_ = null;
                if (unreleased_ > 0)
                {
                    CLIENT.sendOrders(TMSTradingRequests.SendOrdersRequest.newBuilder()
                        .addTargetId(targetId_)
                        .build());
                }
                else
                {
                    complete();
                }
            }
            else if (openOrderId_ != null)
            {
                LOGGER.info("Warning: Target " + targetId_ + " has another open order");
            }
            else
            {
                LOGGER.info("Warning: Target " + targetId_ + " has no open order");
            }
        }

        public void complete()
        {
            LOGGER.info("Target " + targetId_ + " is completed");
            CLIENT.postAlertMessage(TMSRemoteRequests.PostAlertMessageRequest.newBuilder()
                .addUser(USER)
                .setType("Info")
                .setDescription("Target " + targetId_ + " is completed")
                .setUrgent(false)
                .build());
            latch_.countDown();
        }
    }

    // order state transitions:
    // init - order added, price is defined by user
    // midpx - order was not filled during N seconds or market data price was changed significantly
    // market - order was not filled during M seconds after first "midpx" state
    // closed - order filled 100%
    private static class Order
    {
        private final String orderId_;
        private final Target target_;

        private boolean closed_;
        private double price_;
        private double midPx_;
        private TimerTask midPxTask_;
        private TimerTask marketTask_;

        public Order(String orderId, Target target, TMSRemoteCommon.Fields fields, TMSRemoteCommon.Fields marketDataFields)
        {
            orderId_ = orderId;
            target_ = target;
            closed_ = false;
            price_ = fields.getNumericFieldsOrDefault("OrdPx", 0);
            midPx_ = marketDataFields != null ? marketDataFields.getNumericFieldsOrDefault("MidPx", Double.NaN) : Double.NaN;
            midPxTask_ = schedule(this::setMidPx, MID_PX_TIMEOUT);
            LOGGER.info("New order " + orderId + " of target " + target.targetId_ + " added (Price=" + price_ + ")");
            target.onOrderAdded(orderId);
            // to check if it is closed already
            onUpdated(fields);
        }

        public void onUpdated(TMSRemoteCommon.Fields fields)
        {
            double leaves = fields.getNumericFieldsOrDefault("Leaves", -1);
            if (leaves >= 0)
            {
                boolean was_closed = closed_;
                closed_ = leaves == 0;
                // is closed
                if (!was_closed && closed_)
                {
                    // cancel scheduled events
                    if (midPxTask_ != null)
                        midPxTask_.cancel();
                    midPxTask_ = null;
                    if (marketTask_ != null)
                        marketTask_.cancel();
                    marketTask_ = null;
                    // notify target
                    LOGGER.info("Order " + orderId_ + " is closed");
                    target_.onOrderClosed(orderId_);
                }
            }
        }

        public void onMarketData(TMSRemoteCommon.Fields fields)
        {
            midPx_ = fields.getNumericFieldsOrDefault("MidPx", midPx_);
            if (!closed_ &&  price_ > 0)  // is not market
            {
                double lastPx = fields.getNumericFieldsOrDefault("LastPx", 0);
                if (Math.abs((lastPx - price_) / lastPx) > 0.01)
                {
                    LOGGER.info("Market price for order " + orderId_ + " was changed significantly (" + lastPx + ")");
                    setMidPx();
                }
            }
        }

        public void setMidPx()
        {
            if (midPxTask_ != null)
                midPxTask_.cancel();
            midPxTask_ = null;

            if (marketTask_ == null)
                marketTask_ = schedule(this::setMarket, MKT_ORD_TYPE_TIMEOUT);

             // modification itself
            LOGGER.info("Changing price of order " + orderId_ + " to MidPx(" + midPx_ + ")");
            if (midPx_ > 0)
            {
                CLIENT.modifyOrders(TMSTradingRequests.ModifyOrdersRequest.newBuilder()
                    .addOrderId(orderId_)
                    .addMessage(TMSRemoteCommon.FIXFields.newBuilder()
                        .putNumericFields(FIXTag_Price_VALUE, midPx_))
                    .build());
                price_ = midPx_;
            }
        }

        public void setMarket()
        {
            marketTask_ = null; // TODO: same for python

             // modification itself
            LOGGER.info("Changing type of order " + orderId_ + " to market");
            price_ = 0;
            CLIENT.modifyOrders(TMSTradingRequests.ModifyOrdersRequest.newBuilder()
                .addOrderId(orderId_)
                .addMessage(TMSRemoteCommon.FIXFields.newBuilder()
                    .putNumericFields(FIXTag_OrdType_VALUE, OrdType_Market_VALUE))
                .build());
        }

        public static TimerTask schedule(Runnable r, long delay)
        {
            TimerTask timerTask = new TimerTask()
            {
                @Override
                public void run()
                {
                    r.run();
                }
            };
            TIMER.schedule(timerTask, delay);
            return timerTask;
        }
    }

    public static void main(String[] args)
    {
        try
        {
            ManagedChannel channel = NettyChannelBuilder.forAddress("localhost", 8083)
                .sslContext(GrpcSslContexts.forClient().trustManager(new FileInputStream(new File(System.getProperty("user.dir"), "cert.pem"))).build())
//                .usePlaintext(true)
                .build();

            TMSRemoteGrpc.TMSRemoteStub asyncClient = TMSRemoteGrpc.newStub(channel);
            CLIENT = TMSRemoteGrpc.newBlockingStub(channel);


            CLIENT.login(TMSRemoteRequests.LoginRequest.newBuilder()
                .setUser(USER)
                .setPassword(PASSWORD)
                .build());

            TMSTradingRequests.CreateMarketPortfolioRequest request = TMSTradingRequests.CreateMarketPortfolioRequest.newBuilder()
                .setName(PORTFOLIO)
//                .setType(TMSRemoteRequests.CreateMarketPortfolioRequest.PortfolioType.Linked)
                .setType(TMSTradingRequests.PortfolioType.Pure)
//                .setFields(TMSRemoteCommon.Fields.newBuilder()
//                    .putNumericFields("LinkedToStaged", Value.YES))
                .build();

            try
            {
                CLIENT.createMarketPortfolio(request);
            }
            catch (StatusRuntimeException e)
            {
                String errorCode = e.getTrailers().get(Metadata.Key.of("ErrorCode", Metadata.ASCII_STRING_MARSHALLER));
                if (!"CannotCreatePortfolio".equals(errorCode))
                    throw e;
                LOGGER.warn("Portfolio already exists");
            }

            // add targets
            HashSet<String> instruments = new HashSet<>();

            Reader reader = new InputStreamReader(new FileInputStream(new File(System.getProperty("user.dir"), FILE_NAME)));
            CsvMapReader csvReader = new CsvMapReader(reader, new CsvPreference.Builder('\"', ',', "\r\n").build());
            String[] header = csvReader.getHeader(true);
            Map<String, String> row;

            TMSTradingRequests.AddMarketTargetsRequest.Builder addTargetRequestBuilder = TMSTradingRequests.AddMarketTargetsRequest.newBuilder();
            addTargetRequestBuilder.setPortfolio(PORTFOLIO);
            while ((row = csvReader.read(header)) != null)
            {
                TMSRemoteCommon.Fields.Builder fieldsBuilder = TMSRemoteCommon.Fields.newBuilder();

                for (Map.Entry<String,String> entry : row.entrySet())
                {
                    if (STRING_FIELDS.contains(entry.getKey()))
                        fieldsBuilder.putStringFields(entry.getKey(), entry.getValue());
                    else
                        fieldsBuilder.putNumericFields(entry.getKey(), Double.parseDouble(entry.getValue()));
                }
                addTargetRequestBuilder.addFields(fieldsBuilder);

                instruments.add(row.get("Instrument"));
            }

            final TMSTradingRequests.TargetIds targetsActionResponse = CLIENT.addMarketTargets(addTargetRequestBuilder.build());
            final List<Long> targetIdList = targetsActionResponse.getTargetIdList();

            CLIENT.modifyMarketTargets(TMSTradingRequests.ModifyTargetsRequest.newBuilder()
                .addAllTargetId(targetIdList)
                .addFields(TMSRemoteCommon.Fields.newBuilder()
                    .putStringFields("TrnDestination", "Simulator1")
//                    .putStringFields("Text", "NOFILL") // TODO testing only
                    .putNumericFields("WaveSizeType", WaveSizeType_PctTgtQty_VALUE)
                    .putNumericFields("WaveSize", 10))
                .build());


            // initialize structures
            CountDownLatch exitLatch = new CountDownLatch(targetIdList.size());
            Map<Long, Target> targetMap = Collections.synchronizedMap(new HashMap<>());
            Map<String, Order> orderMap = Collections.synchronizedMap(new HashMap<>());
            Map<String, List<Order>> ordersByInstrumentMap = Collections.synchronizedMap(new HashMap<>());
            Map<String, TMSRemoteCommon.Fields> marketDataMap = Collections.synchronizedMap(new HashMap<>());

            // subscribe to targets after target modification to get added event with updated fields
            StreamObserver<TMSTradingRequests.SubscribeForTargetsRequest> marketTargetSubscriptionRequest = asyncClient.subscribeForMarketTargets(new AbstractStreamObserver<TMSRemoteEvents.TargetEvent>("Targets listener")
            {
                @Override
                protected void processEvent(TMSRemoteEvents.TargetEvent targetEvent)
                {
                    switch (targetEvent.getEventCase())
                    {
                        case ADDED:
                        {
                            final TMSRemoteEvents.TargetEvent.TargetAddedEvent addedEvent = targetEvent.getAdded();
                            long targetId = addedEvent.getTargetId();
                            TMSRemoteCommon.Fields fields = addedEvent.getFields();
                            Target target = new Target(targetId, fields, exitLatch);
                            targetMap.put(targetId, target);
                            LOGGER.info("New target " + targetId + " added");
                            break;
                        }
                        case UPDATED:
                        {
                            final TMSRemoteEvents.TargetEvent.TargetUpdatedEvent updatedEvent = targetEvent.getUpdated();
                            long targetId = updatedEvent.getTargetId();
                            TMSRemoteCommon.Fields fields = updatedEvent.getFields();
                            targetMap.get(targetId).onUpdated(fields);
                            break;
                        }
                    }
                }
            });
            marketTargetSubscriptionRequest.onNext(TMSTradingRequests.SubscribeForTargetsRequest.newBuilder()
                .setFilter("Portfolio = '" + PORTFOLIO + "'")
                .addAllField(Arrays.asList("TgtID", "Unreleased", "Text"))
                .build());

            // subscribe to orders
            final StreamObserver<TMSTradingRequests.SubscribeForOrdersRequest> ordersSubscriptionRequest = asyncClient.subscribeForOrders(new AbstractStreamObserver<TMSRemoteEvents.OrderEvent>("Orders listener")
            {
                @Override
                protected void processEvent(TMSRemoteEvents.OrderEvent orderEvent)
                {
                    switch (orderEvent.getEventCase())
                    {
                        case ADDED:
                        {
                            final TMSRemoteEvents.OrderEvent.OrderAddedEvent orderAddedEvent = orderEvent.getAdded();
                            String orderId = orderAddedEvent.getOrderId();
                            final TMSRemoteCommon.Fields orderFields = orderAddedEvent.getFields();
                            long targetId = (long) orderFields.getNumericFieldsOrDefault("TgtID", -1);
                            String instrument = orderFields.getStringFieldsOrDefault("Instrument", null);

                            Target target = targetMap.get(targetId);
                            TMSRemoteCommon.Fields marketDataFields = marketDataMap.get(instrument);

                            Order order = new Order(orderId, target, orderFields, marketDataFields);
                            orderMap.put(orderId, order);

                            ordersByInstrumentMap.computeIfAbsent(instrument, i -> Collections.synchronizedList(new ArrayList<>())).add(order);
                            break;
                        }
                        case UPDATED:
                        {
                            final TMSRemoteEvents.OrderEvent.OrderUpdatedEvent orderUpdatedEvent = orderEvent.getUpdated();
                            String orderId = orderUpdatedEvent.getOrderId();
                            final TMSRemoteCommon.Fields orderFields = orderUpdatedEvent.getFields();
                            orderMap.get(orderId).onUpdated(orderFields);
                            break;
                        }
                    }
                }

            });
            ordersSubscriptionRequest.onNext(TMSTradingRequests.SubscribeForOrdersRequest.newBuilder()
                .setFilter("TgtID IN (" + targetIdList.stream().map(String::valueOf).collect(Collectors.joining(",")) + ")")
                .addAllField(Arrays.asList("TgtID", "Instrument", "Leaves", "OrdPx"))
                .build());

            // subscribe to market data

            StreamObserver<TMSRemoteRequests.SubscribeForMarketDataRequest> marketDataSubscriptionRequest = asyncClient.subscribeForMarketData(new AbstractStreamObserver<TMSRemoteEvents.MarketDataEvent>("Market data listener")
            {
                @Override
                protected void processEvent(TMSRemoteEvents.MarketDataEvent marketDataEvent)
                {
                    switch (marketDataEvent.getEventCase())
                    {
                        case UPDATE:
                        {
                            TMSRemoteEvents.MarketDataEvent.MarketDataUpdate update = marketDataEvent.getUpdate();
                            String instrument = update.getInstrument();
                            TMSRemoteCommon.Fields fields = update.getFields();
                            marketDataMap.put(instrument, fields);

                            List<Order> orders = ordersByInstrumentMap.get(instrument);
                            if (orders != null)
                                orders.forEach(o -> o.onMarketData(fields));
                            break;
                        }
                    }
                }
            });
            TMSRemoteRequests.SubscribeForMarketDataRequest.Builder marketDataSubscriptionRequestBuilder = TMSRemoteRequests.SubscribeForMarketDataRequest.newBuilder();
            for (String instrument : instruments)
            {
                marketDataSubscriptionRequestBuilder.addInstrument(instrument);
            }
            marketDataSubscriptionRequest.onNext(marketDataSubscriptionRequestBuilder
                .addAllField(Arrays.asList("LastPx", "MidPx"))
                .build());

            // send first wave
            CLIENT.sendOrders(TMSTradingRequests.SendOrdersRequest.newBuilder()
                .addAllTargetId(targetIdList)
                .build());

            exitLatch.await();

            marketTargetSubscriptionRequest.onCompleted();
            marketDataSubscriptionRequest.onCompleted();
            ordersSubscriptionRequest.onCompleted();
            channel.shutdown().awaitTermination(1, TimeUnit.SECONDS);

            System.exit(0);
        }
        catch (StatusRuntimeException ex)
        {
            LOGGER.error(ex.getMessage(), ex);
            final Metadata trailers = ex.getTrailers();
            String exceptionClass = trailers.get(Metadata.Key.of("ExceptionClass", Metadata.ASCII_STRING_MARSHALLER));
            String errorCode = trailers.get(Metadata.Key.of("ErrorCode", Metadata.ASCII_STRING_MARSHALLER));
            LOGGER.error("    Exception class: " + exceptionClass + " errorCode: " + errorCode);
            final String childExceptionCountString = trailers.get(Metadata.Key.of("ChildExceptionsCount", Metadata.ASCII_STRING_MARSHALLER));
            int childExceptionCount = childExceptionCountString != null ? Integer.parseInt(childExceptionCountString) : 0;
            for (int i = 0; i < Math.min(childExceptionCount, 10); i++) //only 10 child exceptions have details sent remotely
            {
                String childException = trailers.get(Metadata.Key.of("ChildExceptionMessage_" + i, Metadata.ASCII_STRING_MARSHALLER));
                LOGGER.error( "        ChildException " + i + " " + childException);
            }
        }
        catch (Exception e)
        {
            LOGGER.error(e.getMessage(), e);
        }
        finally
        {
            LOGGER.getLoggerRepository().shutdown();
        }
    }
}
