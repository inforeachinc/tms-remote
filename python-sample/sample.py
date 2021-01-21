import os
import grpc
import time
import sched
import csv
import logging

from IterableQueue import IterableQueue
from EventThread import EventThread
from CountDownLatch import CountDownLatch
from SchedulerThread import SchedulerThread

from remote.TMSRemote_pb2_grpc import *
from remote.TMSRemoteCommon_pb2 import *
from remote.TMSRemoteRequests_pb2 import *
from remote.TMSTradingRequests_pb2 import *

# Sample client app for TMS gRPC remote client The example does the following steps:
#     Create a portfolio with targets from file
#     Modify targets TrnDestination=Simulator1
#     Subscribe to orders, targets flow and MarketData
#     Send Wave e.g. 10% of tgt qty, limit orders with Px instructions BidPx:AskPx
#     Modify price to MidPx for all orders open longer than N seconds since the last request
#     Modify order type to Market for all orders open longer than M seconds after the prev step
#     In MarketData listener compare LastPx and Price of open orders for current instrument. If (LastPx-Price)/LastPx > 0.01 then modify order price to MidPx
#     Once an order is filled goto #4 for it
#     Once a target is completed send alert to the user
#     If Text field is updated with STOP value then cancel open orders for this target and stop trading it.

SERVER = 'localhost'
USER = 'demo'
PASSWORD = ''
PORTFOLIO = 'grpcSample - ' + time.strftime('%Y%d%m %H:%M:%S')
FILE_NAME = 'grpcSample.csv'
STRING_FIELDS = set(['Instrument', 'ClientName', 'SetPxTo'])
MID_PX_TIMEOUT = 2  # seconds
MARKET_TIMEOUT = 3  # seconds

abspath = os.path.abspath(__file__)
directory = os.path.dirname(abspath)

logging_format = '%(asctime)s %(levelname)-6s %(message)s'
logging.basicConfig(level=logging.INFO, format=logging_format)
fh = logging.FileHandler(directory + '/sample.log')
fh.setFormatter(logging.Formatter(logging_format))
logging.getLogger('').addHandler(fh)

scheduler = sched.scheduler(time.time, time.sleep)
SchedulerThread(scheduler).start()

# channel = grpc.insecure_channel(SERVER + ':8082')
ssl_credentials = grpc.ssl_channel_credentials(open(directory + '/cert.pem', 'rb').read())
channel = grpc.secure_channel(SERVER + ':8082', ssl_credentials)
client = TMSRemoteStub(channel)

client.login(LoginRequest(user=USER, password=PASSWORD))


class Target:
    def __init__(self, target_id, fields, latch):
        self.target_id = target_id
        self.latch = latch
        self.open_order_id = None
        self.unreleased = fields.numericFields['Unreleased']
        self.stopped = False

    def on_updated(self, fields):
        if not self.stopped:
            self.unreleased = fields.numericFields.get('Unreleased', self.unreleased)

        if fields.stringFields.get('Text', None) == 'STOP':
            self.stopped = True  # to do not update unreleased
            self.unreleased = 0

            if self.open_order_id is not None:
                logging.info('Stopping target ' + str(self.target_id))
                client.cancelOrders(CancelOrdersRequest(
                    orderId=[self.open_order_id]
                ))
            else:
                logging.warning('Cannot stop target ' + str(self.target_id) + ', it has no open orders')

    def on_order_added(self, order_id):
        if self.open_order_id is not None:
            logging.info('Warning: Target ' + str(self.target_id) + ' already has open order')
        self.open_order_id = order_id

        if self.stopped:
            logging.warning('New order ' + str(order_id) + ' added to already stopped target ' + str(self.target_id) + ', canceling')
            client.cancelOrders(CancelOrdersRequest(
                orderId=[self.open_order_id]
            ))

    def on_order_closed(self, order_id):
        if self.open_order_id == order_id:
            self.open_order_id = None
            if self.unreleased > 0:
                client.sendOrders(SendOrdersRequest(
                    targetId=[self.target_id]
                ))
            else:
                self.complete()
        elif self.open_order_id is not None:
            logging.info('Warning: Target ' + str(self.target_id) + ' has another open order')
        else:
            logging.info('Warning: Target ' + str(self.target_id) + ' has no open order')

    def complete(self):
        logging.info('Target ' + str(self.target_id) + ' is completed')
        client.postAlertMessage(PostAlertMessageRequest(
            user=[USER], type='Info', description='Target ' + str(self.target_id) + ' is completed', urgent=False
        ))
        self.latch.count_down()


# order state transitions:
# init - order added, price is defined by user
# midpx - order was not filled during N seconds or market data price was changed significantly
# market - order was not filled during M seconds after first "midpx" state
# closed - order filled 100%
class Order:
    def __init__(self, order_id, target, fields, md):
        self.order_id = order_id
        self.target = target
        self.closed = False
        self.price = fields.numericFields['OrdPx']
        self.mid_px = md.numericFields['MidPx'] if md is not None else None
        self.midpx_event = scheduler.enter(MID_PX_TIMEOUT, 1, self.set_midpx, [True])
        self.market_event = None
        logging.info('New order ' + str(order_id) + ' of target ' + str(target.target_id) + ' added (Price=' + str(self.price) + ')')
        target.on_order_added(order_id)
        # to check if it is closed already
        self.on_updated(fields)

    def on_updated(self, fields):
        numeric_fields = fields.numericFields
        if 'Leaves' in numeric_fields:
            was_closed = self.closed
            self.closed = numeric_fields['Leaves'] == 0
            # is closed
            if not was_closed and self.closed:
                # cancel scheduled events
                if self.midpx_event is not None:
                    scheduler.cancel(self.midpx_event)
                    self.midpx_event = None
                if self.market_event is not None:
                    scheduler.cancel(self.market_event)
                    self.market_event = None
                # notify target
                logging.info('Order ' + str(self.order_id) + ' is closed')
                self.target.on_order_closed(self.order_id)

    def on_market_data(self, fields):
        self.mid_px = fields.numericFields['MidPx']
        if not self.closed and self.price > 0:  # is not market
            last_px = fields.numericFields['LastPx']
            if abs((last_px - self.price)/last_px) > 0.01:
                logging.info('Market price for order ' + str(self.order_id) + ' was changed significantly (' + str(last_px) + ')')
                self.set_midpx(False)

    def set_midpx(self, from_timer):
        if self.midpx_event is not None:
            if not from_timer:
                scheduler.cancel(self.midpx_event)
            self.midpx_event = None
        if self.market_event is None:
            self.market_event = scheduler.enter(MARKET_TIMEOUT, 1, self.set_market, [])
        # modification itself
        logging.info('Changing price of order ' + str(self.order_id) + ' to MidPx(' + str(self.mid_px) + ')')
        if self.mid_px is not None:
            client.modifyOrders(ModifyOrdersRequest(
                orderId=[self.order_id],
                message=[{
                    'numericFields': {FIXTag_Price: self.mid_px}}
                ]
            ))
            self.price = self.mid_px

    def set_market(self):
        self.market_event = None
        # modification itself
        self.price = 0
        logging.info('Changing type of order ' + str(self.order_id) + ' to market')
        client.modifyOrders(ModifyOrdersRequest(
            orderId=[self.order_id],
            message=[{
                'numericFields': {FIXTag_OrdType: OrdType_Market}}
            ]
        ))

# create market portfolio
try:
    client.createMarketPortfolio(CreateMarketPortfolioRequest(name=PORTFOLIO, type=CreateMarketPortfolioRequest.Pure))
except grpc.RpcError as e:
    error_code = dict(e.initial_metadata()).get('errorcode', None)
    if error_code != 'CannotCreatePortfolio':
        raise e
    logging.info('Portfolio already exists')

# load targets from CSV file
instruments = set()
targets = []
with open(directory + '/' + FILE_NAME, 'r') as csvfile:
    reader = csv.DictReader(csvfile, delimiter=',', quotechar='"')
    for row in reader:
        target = {'stringFields': {k: v for k, v in row.items() if k in STRING_FIELDS},
                  'numericFields': {k: float(v) for k, v in row.items() if k not in STRING_FIELDS}}
        targets.append(target)
        instruments.add(row['Instrument'])

# create market targets
try:
    target_ids = client.addMarketTargets(AddTargetsRequest(
        portfolio=PORTFOLIO,
        fields=targets
    )).targetId
except Exception as ex:
    logging.exception('Exception when calling addMarketTargets')
    if hasattr(ex, 'trailing_metadata'):
        metadata = dict(ex.trailing_metadata())
        print("    Exception class: ", metadata.get("exceptionclass", None), " errorCode: ", metadata.get("errorcode", None))
        childExceptionCount = int(metadata.get("childexceptionscount", "0"))
        for ec in range(min(childExceptionCount, 10)):  # only 10 child exceptions have details sent remotely
            print("        ChildException ", ec, " ", metadata.get("childexceptionmessage_" + str(ec)))
    sys.exit()

# subscribe for market targets
targets = {}
latch = CountDownLatch(len(target_ids))


def market_target_event_processor(event):
    event_case = event.WhichOneof('event')
    if event_case == 'added':
        added = event.added
        target_id = added.targetId
        logging.info('New target ' + str(target_id )+ ' added')
        targets[target_id] = Target(target_id, added.fields, latch)
    elif event_case == 'updated':
        updated = event.updated
        targets[updated.targetId].on_updated(updated.fields)


market_target_subscription_queue = IterableQueue(SubscribeForTargetsRequest(
    filter="Portfolio = '" + PORTFOLIO + "'",
    field=['TgtID', 'Unreleased', 'Text']
))
events = client.subscribeForMarketTargets(iter(market_target_subscription_queue))
EventThread(events, market_target_event_processor).start()

# modify market targets
client.modifyMarketTargets(ModifyTargetsRequest(
    targetId=target_ids,
    fields=[{
        'stringFields': {'TrnDestination': 'Simulator1'},
        # 'stringFields': {'TrnDestination': 'Simulator1', 'Text': 'NOFILL'},
        'numericFields': {'WaveSizeType': WaveSizeType_PctTgtQty, 'WaveSize': 10}}
    ]
))

# subscribe for orders
orders = {}
orders_by_instrument = {}
market_data = {}


def order_event_processor(event):
    event_case = event.WhichOneof('event')
    if event_case == 'added':
        added = event.added
        order_id = added.orderId
        fields = added.fields
        target_id = int(fields.numericFields['TgtID'])
        target = targets[target_id]
        md = market_data.get(fields.stringFields['Instrument'], None)
        order = Order(order_id, target, fields, md)
        orders[order_id] = order
        instrument = fields.stringFields['Instrument']
        by_instrument = orders_by_instrument.get(instrument, None)
        if by_instrument is None:
            by_instrument = []
            orders_by_instrument[instrument] = []
        by_instrument.append(order)
    elif event_case == 'updated':
        updated = event.updated
        orders[updated.orderId].on_updated(updated.fields)


orders_subscription_queue = IterableQueue(SubscribeForOrdersRequest(
    filter="Portfolio = '" + PORTFOLIO + "'",
    field=['TgtID', 'Instrument', 'Leaves', 'OrdPx']
))
events = client.subscribeForOrders(iter(orders_subscription_queue))
EventThread(events, order_event_processor).start()

# subscribe for market data


def market_data_event_processor(event):
    event_case = event.WhichOneof('event')
    if event_case == 'update':
        update = event.update
        market_data[update.instrument] = update.fields
        by_instrument = orders_by_instrument.get(update.instrument, None)
        if by_instrument is not None:
            for order in by_instrument:
                order.on_market_data(update.fields)


market_data_subscription_queue = IterableQueue(SubscribeForMarketDataRequest(
    instrument=instruments,
    field=['LastPx', 'MidPx']
))
events = client.subscribeForMarketData(iter(market_data_subscription_queue))
EventThread(events, market_data_event_processor).start()

# send first wave
client.sendOrders(SendOrdersRequest(
    targetId=target_ids
))

latch.awaitLatch()

market_target_subscription_queue.close()
orders_subscription_queue.close()
market_data_subscription_queue.close()
