from operator import attrgetter


class Memory:
    def __init__(self, number_of_frames, algorithm, tau):
        self._frame_table = [None for _ in range(number_of_frames)]
        self._num_frames = number_of_frames
        self._frame_count = 0
        self._algorithm = algorithm
        self._writes = 0
        self._clock = 0
        self._tau = tau

    def add(self, page):
        self._frame_table[page.frame_number] = page
        page.valid = 1
        page.referenced = 1
        self._frame_count += 1

    @property
    def frame_count(self):
        return self._frame_count

    @frame_count.setter
    def frame_count(self, value):
        self._frame_count = value

    @property
    def writes(self):
        return self._writes

    def full(self):
        return int(self._frame_count) >= int(self._num_frames)

    def evict(self, current_line):
        global evicted
        # if self._algorithm == 'opt':
        #     furthest = current_line
        #     for frame in self._frame_table:
        #         if frame.opt_len == 0:
        #             page.frame_number = frame
        #             break
        #         elif frame.opt_len > 0:
        #             while frame.opt_peek <= current_line:
        #                 frame.opt_pop
        #
        #             if frame.opt_len != 0:
        #                 next_use = frame.opt_peek
        #                 if frame.opt_peek > furthest:
        #                     furthest = frame.opt_peek
        if self._algorithm == 'clock':
            replaced = False
            while not replaced:
                entry = self._frame_table[self._clock]
                if entry.referenced == 0:
                    evicted = entry
                    replaced = True
                else:
                    entry.referenced = 0
                    self._clock += 1

                    if self._clock >= self._num_frames:
                        self._clock = 0
        elif self._algorithm == 'fifo':
            evicted = min(self._frame_table, key=attrgetter('_insertion_time'))
        elif self._algorithm == 'work':
            evicted = None
            oldest = None
            replaced = False
            clock_start = self._clock
            self._clock += 1
            if self._clock >= self._num_frames:
                self._clock = 0

            while not replaced:
                entry = self._frame_table[self._clock]

                if self._clock == clock_start:
                    if oldest is None:
                        evicted = min(self._frame_table, key=attrgetter('_insertion_time'))
                    else:
                        evicted = oldest
                    break
                if entry.dirty == 0 and entry.referenced == 0:
                    evicted = entry
                    replaced = True
                elif entry.dirty == 1 and entry.referenced == 0:
                    if entry.last_reference < (current_line - self._tau):
                        self._writes += 1
                        if oldest is None:
                            oldest = entry
                        elif oldest.last_reference > entry.last_reference:
                            oldest = entry
                        entry.dirty = 0
                elif entry.referenced == 1:
                    entry.referenced = 0

                self._clock += 1

                if self._clock >= self._num_frames:
                    self._clock = 0

        if evicted.dirty == 1:
            self._writes += 1
        evicted.valid = 0
        return evicted.frame_number
