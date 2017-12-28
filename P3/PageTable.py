from collections import deque


class PageTable:
    def __init__(self):
        self._table = {}

    def __repr__(self):
        return "Page Table: %s" % self._table

    def __getitem__(self, key):
        return self._table[key]

    def __setitem__(self, key, value):
        if key not in self._table:
            raise KeyError
        self._table[key] = value

    def add(self, page):
        page.valid = 1
        self._table[page.page_number] = page

    def update(self, page):
        self._table[page.page_number] = page


class Page:
    def __init__(self, page_number, mode, insertion_time):
        self._page_number = page_number
        self._frame_number = None
        self._mode = mode
        self._referenced = 1
        if mode == 'W':
            self._dirty = 1
        else:
            self._dirty = 0
        self._valid = 0
        self._insertion_time = insertion_time
        self._last_reference = insertion_time
        self._uses = None

    @property
    def page_number(self):
        return self._page_number

    @page_number.setter
    def page_number(self, value):
        self._page_number = value

    @property
    def frame_number(self):
        return self._frame_number

    @frame_number.setter
    def frame_number(self, value):
        self._frame_number = value

    @property
    def mode(self):
        return self._mode

    @mode.setter
    def mode(self, value):
        self._mode = value

    @property
    def referenced(self):
        return self._referenced

    @referenced.setter
    def referenced(self, value):
        self._referenced = value

    @property
    def dirty(self):
        return self._dirty

    @dirty.setter
    def dirty(self, value):
        self._dirty = value

    @property
    def valid(self):
        return self._valid

    @valid.setter
    def valid(self, value):
        self._valid = value

    @property
    def insertion_time(self):
        return self._insertion_time

    @insertion_time.setter
    def insertion_time(self, value):
        self._insertion_time = value

    @property
    def last_reference(self):
        return self._last_reference

    @last_reference.setter
    def last_reference(self, value):
        self._last_reference = value

    # Enables opt algorithm deque
    def opt(self):
        self._uses = deque()

    def opt_append(self, value):
        self._uses.appendleft(value)

    def opt_pop(self):
        return self._uses.popleft()

    def opt_len(self):
        return len(self._uses)

    def opt_peek(self):
        return self._uses[0]
