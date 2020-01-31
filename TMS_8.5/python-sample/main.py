import grpc
import time
import datetime

from IterableQueue import IterableQueue
from EventThread import EventThread
from event_processors import *

from remote.TMSRemote_pb2_grpc import *
from remote.TMSRemoteCommon_pb2 import *
from remote.TMSRemoteRequests_pb2 import *
from remote.TMSTradingRequests_pb2 import *

SERVER = 'localhost'
PORTFOLIO = 'test ' + str(datetime.datetime.now())

# channel = grpc.insecure_channel(SERVER + ':8082')
ssl_credentials = grpc.ssl_channel_credentials(open('cert.pem', 'rb').read())
channel = grpc.secure_channel(SERVER + ':8082', ssl_credentials)
client = TMSRemoteStub(channel)


#START SNIPPET: Channel Connectivity Subscription
def log_connectivity_changes(connectivity):
    print("Channel changed status to %s." % connectivity)
channel.subscribe(log_connectivity_changes)
#END SNIPPET: Channel Connectivity Subscription

#START SNIPPET: Login
client.login(LoginRequest(user='demo', password=''))
#END SNIPPET: Login

#START SNIPPET: Security Master API
"""
Retrieve close price for instrument
"""
instrument_infos = client.getInstrumentInfos(InstrumentInfosRequest(instrument=['IBM']))
close_px = instrument_infos.instrumentInfo[0].numericFields['ClosePx']
#END SNIPPET: Security Master API

#START SNIPPET: Create Market Portfolio
"""
Create market portfolio
"""
client.createMarketPortfolio(CreateMarketPortfolioRequest(name=PORTFOLIO, type=CreateMarketPortfolioRequest.Pure))
#END SNIPPET: Create Market Portfolio

"""
Subscribe market_target_event_processor for the market target events
"""
#START SNIPPET: Subscribe for market targets
market_target_subscription_queue = IterableQueue(SubscribeForTargetsRequest(
    filter='TgtCreateTime >= ' + str(int(time.time() * 1000)),
    field=['TgtID', 'Instrument', 'TgtQty', 'FillQty']
))
events = client.subscribeForMarketTargets(iter(market_target_subscription_queue))
EventThread(events, market_target_event_processor).start()
#END SNIPPET: Subscribe for market targets
"""
Subscribe for the order events
"""
orders_subscription_queue = IterableQueue(SubscribeForOrdersRequest())
events = client.subscribeForOrders(iter(orders_subscription_queue))


class FeedStatus():
    status = Disconnected
    pass
feedStatus = FeedStatus()

def order_event_processor(event):
    event_case = event.WhichOneof("event")
    #Only process newly added orders, ignore the ones that are part of the initial state
    if event_case == 'added' and feedStatus.status == InitialStateReceived:
        added = event.added
        order_id = added.orderId
        fields = added.fields
        order_qty = fields.numericFields['OrdQty']
        if order_qty == 0:
            #START SNIPPET: Fill Orders
            client.fillOrders(FIXMessagesRequest(
                message=[{
                    'stringFields': {FIXTag_SingleOrderTransactionId: order_id},
                    'numericFields': {FIXTag_LastQty: 300, FIXTag_LastPx: 49.9}}
                ]
            ))
            #END SNIPPET: Fill Orders
        elif order_qty == 300:
            #START SNIPPET: Cancel Orders
            client.cancelOrders(CancelOrdersRequest(
                orderId=[order_id]
            ))
            #END SNIPPET: Cancel Orders
        else:
            #START SNIPPET: Modify Orders
            new_order_qty = order_qty - 100

            client.modifyOrders(ModifyOrdersRequest(
                orderId=[order_id],
                message=[{
                    'stringFields': {FIXTag_Text: 'FILL_100'},
                    'numericFields': {FIXTag_OrderQty: new_order_qty}}
                ]
            ))
            #END SNIPPET: Modify Orders
    elif event_case == 'feedStatus':
        feedStatus.status = event.feedStatus

EventThread(events, order_event_processor).start()

""" subscribe for "IBM" instruments global position """
instrument_position_subscription_queue = IterableQueue(SubscribeForInstrumentPositionsRequest(
    categoryType='Global',
    categoryName='Global',
    instrument='IBM'
))
events = client.subscribeForInstrumentPositions(iter(instrument_position_subscription_queue))
EventThread(events, instrument_position_event_processor).start()

""" subscribe for global position """
category_position_subscription_queue = IterableQueue(SubscribeForCategoryPositionsRequest(
    categoryType='Global',
    categoryName='Global'
))
events = client.subscribeForCategoryPositions(iter(category_position_subscription_queue))
EventThread(events, category_position_event_processor).start()

""" subscribe for market data """
#START SNIPPET: Subscribe for market data
market_data_subscription_queue = IterableQueue(SubscribeForMarketDataRequest(
    instrument=['IBM', 'MSFT'],
    field=['LastPx', 'ClosePx']
))
events = client.subscribeForMarketData(iter(market_data_subscription_queue))
EventThread(events, market_data_event_processor).start()
#END SNIPPET: Subscribe for market data

#START SNIPPET: Modify Market Portfolio
""" modify market portfolio """
client.modifyMarketPortfolio(ModifyPortfolioRequest(
    name=PORTFOLIO,
    fields={
        'stringFields': {'TrnDestinationAlias': 'Tally'},
    }
))
#END SNIPPET: Modify Market Portfolio

#START SNIPPET: Add Market Targets
""" add market target """
target_id = client.addMarketTargets(AddTargetsRequest(
    portfolio=PORTFOLIO,
    fields=[{
        'stringFields': {'Instrument': 'IBM'},
        'numericFields': {'Side': Side_Buy, 'TgtQty': 1000, 'TgtOrdType': OrdType_Market}}
    ]
)).targetId[0]
#END SNIPPET: Add Market Targets

#START SNIPPET: Modify Market Targets
""" modify market target """
client.modifyMarketTargets(ModifyTargetsRequest(
    targetId=[target_id],
    fields=[{
        'stringFields': {'TrnDestination': 'Simulator1'},
        'numericFields': {'WaveSizeType': WaveSizeType_PctTgtQty, 'WaveSize': 50}}
    ]
))
#END SNIPPET: Modify Market Targets

#START SNIPPET: Send Orders
""" send non-target order """
client.sendOrders(SendOrdersRequest(
    message=[{
        'stringFields': {FIXTag_Instrument:'ORCL', FIXTag_Symbol: 'MSFT', FIXTag_TrnDestination: 'Simulator1',  FIXTag_Text: 'NOFILL'},
        'numericFields': {FIXTag_OrderQty: 300, FIXTag_OrdType: OrdType_Limit, FIXTag_Price: 100,  FIXTag_Side: Side_Buy,
                          FIXTag_HandlInst: HandlInst_AutomatedExecutionOrderPublicBrokerInterventionOk}}
    ]
))

""" send target order """
client.sendOrders(SendOrdersRequest(
    targetId=[target_id],
))

""" send target order with additional field(s) in the message """
client.sendOrders(SendOrdersRequest(
    targetId=[target_id],
    message=[{
        'stringFields': {FIXTag_Text: 'NOFILL'}}
    ]
))
#END SNIPPET: Send Orders

#START SNIPPET: Pause Market Targets
""" pause target """
client.pauseMarketTargets(PauseMarketTargetsRequest(
    targetId=[target_id],
    cancelOpenOrders=False
))
#END SNIPPET: Pause Market Targets
time.sleep(2)

""" resume target """
client.resumeMarketTargets(ResumeMarketTargetsRequest(
    targetId=[target_id]
))

""" send order """
client.sendOrders(SendOrdersRequest(
    targetId=[target_id]
))

""" terminate target """
client.terminateMarketTargets(TerminateMarketTargetsRequest(
    targetId=[target_id],
    cancelOpenOrders=True
))


#START SNIPPET: Get Market Target
""" receive target from TMS service """
market_targets = client.getMarketTargets(TargetIds(
    targetId=[target_id]
))
print("Received target record with remote call:", market_targets.target[0].numericFields['TgtID'])
#END SNIPPET: Get Market Target


#START SNIPPET: Remove Market Targets
""" remove market target """
client.removeMarketTargets(TargetIds(
    targetId=[target_id]
))
#END SNIPPET: Remove Market Targets


#START SNIPPET: Remove Market Portfolio
""" remove market portfolio """
client.removeMarketPortfolio(RemovePortfolioRequest(
    name=PORTFOLIO
))
#END SNIPPET: Remove Market Portfolio

time.sleep(10)

""" unsubscribe from all """
#START SNIPPET: Unsubscribe from market targets
market_target_subscription_queue.close()
#END SNIPPET: Unsubscribe from market targets
orders_subscription_queue.close()
instrument_position_subscription_queue.close()
category_position_subscription_queue.close()

#START SNIPPET: Stop All Trading
""" stop all trading """
client.stopAllTrading(Void())
#END SNIPPET: Stop All Trading

#START SNIPPET: Reporting API
report_request = ReportRequest(domainManagerName='Portfolio report manager', reportName='Portfolio Orders by Instrument')
if not client.isReportAvailable(report_request).available:
    client.createReport(CreateReportRequest(domainManagerName='Portfolio report manager',
                                            reportSpecResource='/ElTrader/TMS/Common/Reports/Portfolio Orders by Instrument.reportspec'))
#END SNIPPET: Reporting API

#START SNIPPET: Alerts API
client.postAlertMessage(PostAlertMessageRequest(
    user=['demo'], type='Info', description='Client application finished', urgent=False
))
#END SNIPPET: Alerts API
