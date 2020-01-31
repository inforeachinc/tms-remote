"""
Contains sample stub processors of the events from the remote TMS
"""

from remote.TMSRemoteEvents_pb2 import *


#START SNIPPET: Market target event processor
def market_target_event_processor(event):
    event_case = event.WhichOneof("event")
    if event_case == 'added':
        added = event.added
        added.targetId
        added.fields
    elif event_case == 'updated':
        updated = event.updated
        updated.targetId
        updated.fields
    elif event_case == 'removed':
        removed = event.removed
        removed.targetId
    elif event_case == 'filteredOut':
        filtered_out = event.filteredOut
        filtered_out.targetId
    elif event_case == 'paused':
        paused = event.paused
        paused.targetId
    elif event_case == 'resumed':
        resumed = event.resumed
        resumed.targetId
    elif event_case == 'terminated':
        terminated = event.terminated
        terminated.targetId
    elif event_case == 'feedStatus':
        feed_status = event.feedStatus
        if feed_status == Disconnected:
            pass
        elif feed_status == Reconnected:
            pass
        elif feed_status == InitialStateReceived:
            pass
#END SNIPPET: Market target event processor


def instrument_position_event_processor(event):
    event_case = event.WhichOneof("event")
    if event_case == 'added':
        added = event.added
        instrument = added.instrument
        fields = added.fields
        long_qty = fields.numericFields['TradedLongQty']
    elif event_case == 'updated':
        updated = event.updated
        instrument = updated.instrument
        fields = updated.fields
        long_qty = fields.numericFields['TradedLongQty']
    elif event_case == 'feedStatus':
        feed_status = event.feedStatus
        if feed_status == Disconnected:
            pass
        elif feed_status == Reconnected:
            pass
        elif feed_status == InitialStateReceived:
            pass


def category_position_event_processor(event):
    event_case = event.WhichOneof("event")
    if event_case == 'added':
        added = event.added
        category_type = added.categoryType
        category_name = added.categoryName
        fields = added.fields
        long_qty = fields.numericFields['TradedLongQty']
    elif event_case == 'updated':
        updated = event.updated
        category_type = updated.categoryType
        category_name = updated.categoryName
        fields = updated.fields
        long_qty = fields.numericFields['TradedLongQty']
    elif event_case == 'feedStatus':
        feed_status = event.feedStatus
        if feed_status == Disconnected:
            pass
        elif feed_status == Reconnected:
            pass
        elif feed_status == InitialStateReceived:
            pass

#START SNIPPET: Market data event processor
def market_data_event_processor(event):
    event_case = event.WhichOneof("event")
    if event_case == 'update':
        update = event.update
        instrument = update.instrument
        fields = update.fields
        last_px = fields.numericFields['LastPx']
    elif event_case == 'feedStatus':
        feed_status = event.feedStatus
        if feed_status == Disconnected:
            pass
        elif feed_status == Reconnected:
            pass
#START SNIPPET: Market data event processor