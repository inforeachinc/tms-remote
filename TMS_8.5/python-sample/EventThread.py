import threading


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
                print 'Event thread for ', str(self.func), ' is stopping because of exception = ', e
                break
            else:
                #START SNIPPET: Exception Details Printing
                try:
                    self.func(event)
                except Exception as ex:
                    print 'Exception when calling ', str(self.func), ' Exception = ', ex
                    metadata = dict(ex.trailing_metadata())
                    print "    Exception class: ", metadata.get("exceptionclass", None), " errorCode: ", metadata.get("errorcode", None)
                    childExceptionCount = int(metadata.get("childexceptionscount", "0"))
                    for ec in range(min(childExceptionCount, 10)):  # only 10 child exceptions have details sent remotely
                        print "        ChildException ", ec, " ", metadata.get("childexceptionmessage_" + str(ec))
                #END SNIPPET: Exception Details Printing


