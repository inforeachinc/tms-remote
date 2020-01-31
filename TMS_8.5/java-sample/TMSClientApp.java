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

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

import org.apache.log4j.Logger;

import com.inforeach.eltrader.tms.api.grpc.*;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.FIXTag.*;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.OrdType.*;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.Side.*;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.HandlInst.*;

import io.grpc.ManagedChannel;
import io.grpc.Metadata;
import io.grpc.StatusRuntimeException;
import io.grpc.netty.GrpcSslContexts;
import io.grpc.netty.NettyChannelBuilder;
import io.grpc.stub.StreamObserver;

/**
 * Sample client app for TMS gRPC remote client
 */
public class TMSClientApp
{
    private static final Logger LOGGER = Logger.getLogger(TMSClientApp.class);

    private static final String PORTFOLIO = "test";
    private static final String USER = "demo";
    private static final String PASSWORD = "";

    public static void main(String[] args)
    {
        try
        {
            ManagedChannel channel = NettyChannelBuilder.forAddress("localhost", 8082)
                .sslContext(GrpcSslContexts.forClient().trustManager(TMSClientApp.class.getResourceAsStream("cert.pem")).build())
//                .usePlaintext(true)
                .build();

            TMSRemoteGrpc.TMSRemoteStub client = TMSRemoteGrpc.newStub(channel);
            TMSRemoteGrpc.TMSRemoteBlockingStub blockingClient = TMSRemoteGrpc.newBlockingStub(channel);

            //START SNIPPET: Login
            blockingClient.login(TMSRemoteRequests.LoginRequest.newBuilder()
                .setUser(USER)
                .setPassword(PASSWORD)
                .build());
            //END SNIPPET: Login

            //START SNIPPET: Security Master API
            TMSRemoteRequests.InstrumentInfosResponse instrumentInfosResponse = blockingClient.getInstrumentInfos(TMSRemoteRequests.InstrumentInfosRequest.newBuilder()
                .addInstrument("IBM")
                .build());
            if (instrumentInfosResponse.getInstrumentInfoCount() == 1)
            {
                double closePx =  instrumentInfosResponse.getInstrumentInfo(0).getNumericFieldsOrDefault("ClosePx", Double.NaN);
                LOGGER.debug("ClosePx: " + closePx);
            }
            //END SNIPPET: Security Master API

            //START SNIPPET: Create Market Portfolio
            TMSTradingRequests.CreateMarketPortfolioRequest request = TMSTradingRequests.CreateMarketPortfolioRequest.newBuilder()
                .setName(PORTFOLIO)
                .setType(TMSTradingRequests.CreateMarketPortfolioRequest.PortfolioType.Pure)
                .build();

            try
            {
                blockingClient.createMarketPortfolio(request);
            }
            catch (StatusRuntimeException e)
            {
                String errorCode = e.getTrailers().get(Metadata.Key.of("ErrorCode", Metadata.ASCII_STRING_MARSHALLER));
                if (!"CannotCreatePortfolio".equals(errorCode))
                    throw e;
                LOGGER.warn("Portfolio already exists");
            }
            //END SNIPPET: Create Market Portfolio

            //-----------------   Subscribe for events ---------------------------
            //START SNIPPET: Subscribe for market targets
            StreamObserver<TMSTradingRequests.SubscribeForTargetsRequest> marketTargetSubscriptionRequest = client.subscribeForMarketTargets(new TMSClientAppStreamObservers.MarketTargetStreamObserver());
            marketTargetSubscriptionRequest.onNext(TMSTradingRequests.SubscribeForTargetsRequest.newBuilder()
                .setFilter("TgtCreateTime >= " + System.currentTimeMillis())
                .addAllField(Arrays.asList("TgtID", "Instrument", "TgtQty", "FillQty"))
                .build());
            //END SNIPPET: Subscribe for market targets

            final StreamObserver<TMSTradingRequests.SubscribeForOrdersRequest> ordersSubscriptionRequest = client.subscribeForOrders(new TMSClientAppStreamObservers.AbstractStreamObserver<TMSRemoteEvents.OrderEvent>()
            {
                @Override
                protected void processEvent(TMSRemoteEvents.OrderEvent orderEvent)
                {
                    //When order added modify it by changing qty right away
                    if (orderEvent.getEventCase() == TMSRemoteEvents.OrderEvent.EventCase.ADDED)
                    {
                        final TMSRemoteEvents.OrderEvent.OrderAddedEvent orderAddedEvent = orderEvent.getAdded();
                        final String orderId = orderAddedEvent.getOrderId();
                        final TMSRemoteCommon.Fields orderFields = orderAddedEvent.getFields();
                        final Double orderQty = orderFields.getNumericFieldsMap().get("OrdQty");
                        if (orderQty == 0)
                        {
                            //START SNIPPET: Fill Orders
                            TMSRemoteCommon.FIXFields.Builder messageBuilder = TMSRemoteCommon.FIXFields.newBuilder()
                                .putStringFields(FIXTag_SingleOrderTransactionId_VALUE, orderId)
                                .putNumericFields(FIXTag_LastQty_VALUE, 300)
                                .putNumericFields(FIXTag_LastPx_VALUE, 49.9);
                            blockingClient.fillOrders(TMSRemoteRequests.FIXMessagesRequest.newBuilder()
                                .addMessage(messageBuilder)
                                .build());
                            //END SNIPPET: Fill Orders
                        }
                        else if (orderQty == 300)
                        {
                            //START SNIPPET: Cancel Orders
                            blockingClient.cancelOrders(TMSTradingRequests.CancelOrdersRequest.newBuilder()
                                .addOrderId(orderId)
                                .build()
                            );
                            //END SNIPPET: Cancel Orders
                        }
                        else
                        {
                            //START SNIPPET: Modify Orders
                            final double newOrderQty = orderQty.doubleValue() - 100;

                            blockingClient.modifyOrders(TMSTradingRequests.ModifyOrdersRequest.newBuilder()
                                .addOrderId(orderId)
                                .addMessage(TMSRemoteCommon.FIXFields.newBuilder()
                                    .putNumericFields(FIXTag_OrderQty_VALUE, newOrderQty)
                                    .putStringFields(FIXTag_Text_VALUE, "FILL_100") //instruction to start filling
                                    .build())
                                .build()
                            );
                            //END SNIPPET: Modify Orders
                        }
                    }
                }
            });
            ordersSubscriptionRequest.onNext(TMSTradingRequests.SubscribeForOrdersRequest.newBuilder()
                .build());

            StreamObserver<TMSRemoteRequests.SubscribeForInstrumentPositionsRequest> instrumentPositionSubscriptionRequest = client.subscribeForInstrumentPositions(new TMSClientAppStreamObservers.InstrumentPositionEventStreamObserver());
            //subscribe for "IBM" instruments global position
            instrumentPositionSubscriptionRequest.onNext(TMSRemoteRequests.SubscribeForInstrumentPositionsRequest.newBuilder()
                .setCategoryType("Global")
                .setCategoryName("Global")
                .setInstrument("IBM")
                .build());

            StreamObserver<TMSRemoteRequests.SubscribeForCategoryPositionsRequest> categoryPositionSubscriptionRequest = client.subscribeForCategoryPositions(new TMSClientAppStreamObservers.CategoryPositionEventStreamObserver());
            //subscribe for global position
            categoryPositionSubscriptionRequest.onNext(TMSRemoteRequests.SubscribeForCategoryPositionsRequest.newBuilder()
                .setCategoryType("Global")
                .setCategoryName("Global")
                .build());

            //START SNIPPET: Subscribe for market data
            StreamObserver<TMSRemoteRequests.SubscribeForMarketDataRequest> marketDataSubscriptionRequest = client.subscribeForMarketData(new TMSClientAppStreamObservers.MarketDataStreamObserver());
            marketDataSubscriptionRequest.onNext(TMSRemoteRequests.SubscribeForMarketDataRequest.newBuilder()
                .addInstrument("IBM")
                .addInstrument("GOOG")
                .addAllField(Arrays.asList("LastPx", "ClosePx"))
                .build());
            //END SNIPPET: Subscribe for market data

            //-------------------------------------------------------------------

           //START SNIPPET: Modify Market Portfolio
            TMSTradingRequests.ModifyPortfolioRequest modifyMarketPortfolioRequest = TMSTradingRequests.ModifyPortfolioRequest.newBuilder()
                .setName(PORTFOLIO)
                .setFields(TMSRemoteCommon.Fields.newBuilder()
                    .putStringFields("TrnDestinationAlias", "Tally")
                    .build())
                .build();
            blockingClient.modifyMarketPortfolio(modifyMarketPortfolioRequest);
            //END SNIPPET: Modify Market Portfolio

            //START SNIPPET: Add Market Targets
            final TMSTradingRequests.TargetIds targetsActionResponse = blockingClient.addMarketTargets(TMSTradingRequests.AddTargetsRequest.newBuilder()
                .setPortfolio(PORTFOLIO)
                .addFields(TMSRemoteCommon.Fields.newBuilder()
                    .putStringFields("Instrument", "IBM")
                    .putStringFields("TgtText", "NOFILL")
                    .putNumericFields("Side", Side_Buy_VALUE)
                    .putNumericFields("TgtQty", 1000)
                    .putNumericFields("TgtOrdType", OrdType_Market_VALUE))
                .build());
            final List<Long> targetIdList = targetsActionResponse.getTargetIdList();
            long targetId = targetIdList.get(0);
            //END SNIPPET: Add Market Targets

            //START SNIPPET: Modify Market Targets
            blockingClient.modifyMarketTargets(TMSTradingRequests.ModifyTargetsRequest.newBuilder()
                .addTargetId(targetId)
                .addFields(TMSRemoteCommon.Fields.newBuilder()
                    .putStringFields("TrnDestination", "Simulator1")
                    .putNumericFields("WaveSizeType", 0)
                    .putNumericFields("WaveSize", 50))
                .build());
            //END SNIPPET: Modify Market Targets


            blockingClient.sendOrders(TMSTradingRequests.SendOrdersRequest.newBuilder()
                .addTargetId(targetId)
                .build());


            //START SNIPPET: Pause Market Targets
            blockingClient.pauseMarketTargets(TMSTradingRequests.PauseMarketTargetsRequest.newBuilder()
                .addTargetId(targetId)
                .setCancelOpenOrders(false)
                .build());
            //END SNIPPET: Pause Market Targets

            Thread.sleep(2_000);

            blockingClient.resumeMarketTargets(TMSTradingRequests.ResumeMarketTargetsRequest.newBuilder()
                .addTargetId(targetId)
                .build());

            //START SNIPPET: Send Orders
            //Send non-target orders
            blockingClient.sendOrders(TMSTradingRequests.SendOrdersRequest.newBuilder()
                .addMessage(TMSRemoteCommon.FIXFields.newBuilder()
                    .putStringFields(FIXTag_Instrument_VALUE, "MSFT")
                    .putStringFields(FIXTag_Symbol_VALUE, "MSFT")
                    .putStringFields(FIXTag_TrnDestination_VALUE, "Simulator1")
                    .putStringFields(FIXTag_Text_VALUE, "NOFILL")
                    .putNumericFields(FIXTag_OrderQty_VALUE, 300)
                    .putNumericFields(FIXTag_OrdType_VALUE, OrdType_Limit_VALUE)
                    .putNumericFields(FIXTag_Price_VALUE, 100)
                    .putNumericFields(FIXTag_Side_VALUE, Side_Buy_VALUE)
                    .putNumericFields(FIXTag_HandlInst_VALUE, HandlInst_AutomatedExecutionOrderPublicBrokerInterventionOk_VALUE)
                    .build())
                .build());

            //Send target orders
            blockingClient.sendOrders(TMSTradingRequests.SendOrdersRequest.newBuilder()
                .addTargetId(targetId)
                .build());

            //Send target order with additional field(s) in the message
            blockingClient.sendOrders(TMSTradingRequests.SendOrdersRequest.newBuilder()
                .addTargetId(targetId)
                .addMessage(TMSRemoteCommon.FIXFields.newBuilder()
                    .putStringFields(FIXTag_Text_VALUE, "NOFILL")
                    .build())
                .build());
            //END SNIPPET: Send Orders

            blockingClient.terminateMarketTargets(TMSTradingRequests.TerminateMarketTargetsRequest.newBuilder()
                .addTargetId(targetId)
                .setCancelOpenOrders(true)
                .build());

            //START SNIPPET: Get Market Target
            final TMSTradingRequests.Targets marketTargets = blockingClient.getMarketTargets(TMSTradingRequests.TargetIds.newBuilder()
                .addTargetId(targetId)
                .build());

            LOGGER.debug("Received target record with remote call: " + marketTargets);
            //END SNIPPET: Get Market Target

            try
            {
                //START SNIPPET: Remove Market Targets
                blockingClient.removeMarketTargets(TMSTradingRequests.TargetIds.newBuilder()
                    .addTargetId(targetId)
                    .build());
                //END SNIPPET: Remove Market Targets
            }
            catch (Exception e)
            {
                LOGGER.error("Cannot remove market target", e);
            }

            try
            {
                //START SNIPPET: Remove Market Portfolio
                TMSTradingRequests.RemovePortfolioRequest removeMarketPortfolioRequest = TMSTradingRequests.RemovePortfolioRequest.newBuilder()
                     .setName(PORTFOLIO)
                     .build();
                blockingClient.removeMarketPortfolio(removeMarketPortfolioRequest);
                //END SNIPPET: Remove Market Portfolio
            }
            catch (Exception e)
            {
                LOGGER.error("Cannot remove market portfolio", e);
            }

            Thread.sleep(10_000);

            // unsubscribe from updates
            //START SNIPPET: Unsubscribe from market targets
            marketTargetSubscriptionRequest.onCompleted();
            //END SNIPPET: Unsubscribe from market targets
            instrumentPositionSubscriptionRequest.onCompleted();
            categoryPositionSubscriptionRequest.onCompleted();
            ordersSubscriptionRequest.onCompleted();

            //START SNIPPET: Stop All Trading
            blockingClient.stopAllTrading(TMSRemoteCommon.Void.getDefaultInstance());
            //END SNIPPET: Stop All Trading

            //START SNIPPET: Reporting API
            TMSRemoteRequests.ReportRequest reportRequest = TMSRemoteRequests.ReportRequest.newBuilder()
                .setDomainManagerName("Portfolio report manager")
                .setReportName("Portfolio Orders by Instrument")
                .build();
            if (!blockingClient.isReportAvailable(reportRequest).getAvailable())
            {
                blockingClient.createReport(TMSRemoteRequests.CreateReportRequest.newBuilder()
                    .setDomainManagerName("Portfolio report manager")
                    .setReportSpecResource("/ElTrader/TMS/Common/Reports/Portfolio Orders by Instrument.reportspec")
                    .build());
            }
            //END SNIPPET: Reporting API

            //START SNIPPET: Alerts API
            blockingClient.postAlertMessage(TMSRemoteRequests.PostAlertMessageRequest.newBuilder()
                .addUser(USER)
                .setType("Info")
                .setDescription("Client application finished")
                .setUrgent(false)
                .build());
            //END SNIPPET: Alerts API

            channel.shutdown().awaitTermination(1, TimeUnit.SECONDS);
        }
        //START SNIPPET: Exception Details Printing
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
        //END SNIPPET: Exception Details Printing
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
