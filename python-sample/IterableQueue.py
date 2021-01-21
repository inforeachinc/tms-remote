# from https://stackoverflow.com/a/11288503/368004
try:
    import Queue as queue
except ImportError:
    import queue as queue


class IterableQueue(queue.Queue):
    _sentinel = object()

    def __init__(self, to_add=None):
        queue.Queue.__init__(self)

        if to_add is not None:
            self.put(to_add)

    def __iter__(self):
        return iter(self.get, self._sentinel)

    def close(self):
        self.put(self._sentinel)
