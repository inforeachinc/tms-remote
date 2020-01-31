import threading
import traceback
import sys

class ConsoleReadingThread(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.daemon = True

    def threadDump(self):
        print >> sys.stderr, "\n*** STACKTRACE - START ***\n"
        code = []
        for threadId, stack in sys._current_frames().items():
            code.append("\n# ThreadID: %s" % threadId)
            for filename, lineno, name, line in traceback.extract_stack(stack):
                code.append('File: "%s", line %d, in %s' % (filename, lineno, name))
                if line:
                    code.append("  %s" % (line.strip()))

        for line in code:
            print >> sys.stderr, line
        print >> sys.stderr, "\n*** STACKTRACE - END ***\n"

    def run(self):
        while True:
            try:
                input = raw_input('')
                if (input == 'ds'):
                    self.threadDump()

            except Exception as e:
                print 'Exception when reading from the console = ', e
                break

