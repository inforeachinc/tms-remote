from __future__ import print_function
import threading
import logging


class EventThread(threading.Thread):
    def __init__(self, iter, func):
        threading.Thread.__init__(self)
        self.daemon = True

        self.iter = iter
        self.func = func

    def run(self):
        while True:
            try:
                event = next(self.iter)
            except StopIteration:
                break
            except Exception as e:
                logging.exception('Event thread for ' + str(self.func) + ' is stopping because of exception')
                break
            else:
                try:
                    self.func(event)
                except Exception as ex:
                    logging.exception('Exception when calling ' + str(self.func))

