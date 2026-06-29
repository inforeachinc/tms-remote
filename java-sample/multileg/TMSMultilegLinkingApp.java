/*
 * Copyright (c) 1997-2026 InfoReach, Inc. All Rights Reserved.
 *
 * This software is the confidential and proprietary information of
 * InfoReach ("Confidential Information").  You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered into
 * with InfoReach.
 *
 * CopyrightVersion 2.0
 */

package multileg;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.*;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLException;

import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import com.inforeach.eltrader.tms.api.grpc.*;

import io.grpc.ManagedChannel;
import io.grpc.Metadata;
import io.grpc.StatusRuntimeException;
import io.grpc.netty.GrpcSslContexts;
import io.grpc.netty.NettyChannelBuilder;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.Side.Side_Buy_VALUE;
import static com.inforeach.eltrader.tms.api.grpc.TMSRemoteCommon.Side.Side_Sell_VALUE;

/**
 * Demonstrates linking regular market targets into multileg ones and then sending multileg orders of those targets.
 * The example does the following steps:
 * <ol>
 * <li> Creates 2 regular market targets
 * <li> Links them (also sets required multileg fields, making one of them 'primary' target)
 * <li> Sends multileg wave as AB message
 * </ol>
 */
public class TMSMultilegLinkingApp
{
    private static final Logger LOGGER = LogManager.getLogger(TMSMultilegLinkingApp.class);

    private static final String PORTFOLIO = "SINGLE TARGETS";
    private static final String USER = "demo";
    private static final String PASSWORD = "";

    public static void main(String[] args)
    {
        TMSRemoteGrpc.TMSRemoteBlockingStub tmsClient;
        ManagedChannel channel = null;
        try
        {
            channel = NettyChannelBuilder.forAddress("localhost", 8083)
                .sslContext(GrpcSslContexts.forClient().trustManager(new FileInputStream(new File(System.getProperty("user.dir"), "cert.pem"))).build())
                .build();
        }
        catch (SSLException | FileNotFoundException e)
        {
            LOGGER.error("Cannot open certificate file:" + e.getMessage());
            LOGGER.error(e);
            return;
        }


        tmsClient = TMSRemoteGrpc.newBlockingStub(channel);

        try
        {
            tmsClient.login(TMSRemoteRequests.LoginRequest.newBuilder()
                .setUser(USER)
                .setPassword(PASSWORD)
                .build());

            //Regular targets are created
            TMSTradingRequests.AddMarketTargetsRequest.Builder addTargetRequestBuilder = TMSTradingRequests.AddMarketTargetsRequest.newBuilder();
            addTargetRequestBuilder.setPortfolio(PORTFOLIO);
            TMSRemoteCommon.Fields.Builder fieldsBuilder = TMSRemoteCommon.Fields.newBuilder()
                .putStringFields("TrnDestination", "Simulator1")
                .putStringFields("Instrument", "IBMH4=HKF")
                .putNumericFields("Side", Side_Buy_VALUE)
                .putNumericFields("TgtQty", 1000);
            TMSRemoteCommon.Fields targetFields = fieldsBuilder.build();
            addTargetRequestBuilder.addFields(targetFields);
            fieldsBuilder = TMSRemoteCommon.Fields.newBuilder()
                .putStringFields("TrnDestination", "Simulator1")
                .putStringFields("Instrument", "IBMM4=HKF")
                .putNumericFields("Side", Side_Sell_VALUE)
                .putNumericFields("TgtQty", 1000);
            targetFields = fieldsBuilder.build();
            addTargetRequestBuilder.addFields(targetFields);

            final TMSTradingRequests.TargetIds targetIds = tmsClient.addMarketTargets(addTargetRequestBuilder.build());
            final List<Long> targetIdList = targetIds.getTargetIdList();

            // Linking targets
            String tgtCorrelatingInfo = "" + System.currentTimeMillis();
            TMSRemoteCommon.Fields fieldsUpdate1 = TMSRemoteCommon.Fields.newBuilder()
                .putNumericFields("IsPrimaryTgt", 'Y')
                .putNumericFields("WaveSize", 100)
                .putNumericFields("MultiLegQty", 1000)
                .putNumericFields("MultiLegRatio", 1)
                .putStringFields("MultiLegRefID", "1")
                .putNumericFields("MultiLegSide", Side_Buy_VALUE)
                .build();
            TMSRemoteCommon.Fields fieldsUpdate2 = TMSRemoteCommon.Fields.newBuilder()
                .putNumericFields("IsPrimaryTgt", 'N')
                .putNumericFields("MultiLegQty", 1000)
                .putNumericFields("MultiLegRatio", 1)
                .putStringFields("MultiLegRefID", "2")
                .putNumericFields("MultiLegSide", Side_Sell_VALUE)
                .build();
            TMSTradingRequests.LinkMultilegTargetsRequest linkMultilegTargetsRequest = TMSTradingRequests.LinkMultilegTargetsRequest.newBuilder().
                setTargetCorrelatingInfo(tgtCorrelatingInfo).
                addAllTargetId(targetIdList).addFields(0, fieldsUpdate1).addFields(1, fieldsUpdate2).
                build();
            tmsClient.linkMultilegMarketTargets(linkMultilegTargetsRequest);

            // Sending multileg AB message
            tmsClient.sendTargetMultilegOrder(TMSTradingRequests.SendTargetMultilegOrderRequest.newBuilder()
                .setPortfolio(PORTFOLIO)
                .setTargetCorrelatingInfo(tgtCorrelatingInfo)
                .build());

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
            LogManager.shutdown();
        }
    }
}
