# from https://stackoverflow.com/a/11288503/368004
import Queue


class IterableQueue(Queue.Queue):
    _sentinel = object()

    def __init__(self, toAdd=None):
        Queue.Queue.__init__(self)

        if toAdd is not None:
            self.put(toAdd)

    def __iter__(self):
        return iter(self.get, self._sentinel)

    def close(self):
        self.put(self._sentinel)
