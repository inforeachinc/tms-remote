from threading import Thread
import logging


class SchedulerThread(Thread):
    def __init__(self, scheduler):
        Thread.__init__(self, name="Scheduler")
        self.scheduler = scheduler
        self.daemon = True

    def run(self):
        try:
            while self.isAlive():
                self.scheduler.run()
        except:
            logging.exception("Error")
