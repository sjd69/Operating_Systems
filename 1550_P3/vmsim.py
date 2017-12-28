# -*- coding: utf-8 -*-

# Written by Stephen Dowhy
# CS1550 project 3
# Virtual Memory Simualtor
# OPT IS NOT WORKING IN CURRENT VERSION

import os
import sys
from PageTable import Page, PageTable
from Memory import Memory


def simulate(number_of_frames, algorithm, file, tau):
    page_faults = 0

    page_table = PageTable()
    memory = Memory(number_of_frames, algorithm, tau)

    if not os.path.isfile(file):
        sys.exit(file + " not found.")
    else:
        # If we are using opt, pre-process file
        if algorithm == 'opt':
            current_line_number = 1
            for line in open(file, 'r'):
                split_line = line.strip().split(' ')
                address = split_line[0]
                # Get Page Number
                page_num = int(address[:5], 16)
                mode = split_line[1]
                # print("Address: " + address + ", MODE: " + mode)
                # print("SPLIT: " + str(split_line))
                try:
                    page = page_table[page_num]
                    page.opt_append(current_line_number)
                except KeyError:
                    new_page = Page(page_num, mode)
                    new_page.opt()
                    new_page.opt_append(current_line_number)

                current_line_number += 1
            file.seek(0)

        current_line_number = 1
        for line in open(file, 'r'):
            split_line = line.strip().split(' ')
            address = split_line[0]
            # Get Page Number
            page_num = int(address[:5], 16)
            mode = split_line[1]
            # print("Address: " + address + ", MODE: " + mode)
            # print("SPLIT: " + str(split_line))

            # Assume page exists
            try:
                page = page_table[page_num]

                if page.dirty == 'W':
                    page.dirty = 1

                # If page is valid, HIT
                if page.valid:
                    page.referenced = 1
                    # print("PTE IS VALID HIT")
                # Determine which kind of fault.
                else:
                    # If memory is full evict a page to free up a frame
                    if memory.full():
                        # print("RAM IS FULL. PAGE IS NOT IN RAM.")
                        page.frame_number = memory.evict(current_line_number)
                    # Page isn't in RAM but a frame is available
                    else:
                        # print("RAM IS NOT FULL. PAGE IS NOT IN RAM.")

                        page.frame_number = memory.frame_count

                    memory.add(page)
                    page.insertion_time = current_line_number
                    page_faults += 1
            # Page doesn't exist in PT yet
            except KeyError:

                new_page = Page(page_num, mode, current_line_number)

                if new_page.dirty == 'W':
                    new_page.dirty = 1

                # If memory is full, we would fault regardless of it we've seen it before
                if memory.full():
                    # print("RAM IS FULL AND PAGE NOT IN PT.")
                    new_page.frame_number = memory.evict(current_line_number)
                # COMPULSORY FAULT. RAM is not full, we just haven't encountered the page yet.
                else:
                    # print("RAM IS NOT FULL. PAGE IS NOT IN PT.")
                    new_page.frame_number = memory.frame_count

                page_faults += 1
                page_table.add(new_page)
                memory.add(new_page)

            current_line_number += 1
        output_results(algorithm, current_line_number, number_of_frames, memory, page_faults);


def output_results(algorithm, memory_accesses, number_of_frames, memory, page_faults):
    print("\nAlgorithm: " + algorithm)
    print("Number of Frames: " + str(number_of_frames))
    print("Memory Accesses: " + str(memory_accesses - 1))
    print("Page Faults: " + str(page_faults))
    print("Writes to Disk: " + str(memory.writes))


# MAIN FUNCTION
def main():
    # INPUT FORMAT
    # python vmsim.py –n <numframes> -a <opt|clock|fifo|work> [-t <tau>] <tracefile>
    if len(sys.argv) == 6:
        if sys.argv[1] == '-n' and sys.argv[3] == '-a' \
                and (sys.argv[4] == 'opt' or sys.argv[4] == 'clock' or sys.argv[4] == 'fifo') \
                and sys.argv[5] is not None:

            number_of_frames = int(sys.argv[2])
            algorithm = sys.argv[4]
            file = sys.argv[5]
            tau = 0
        else:
            # sys.exit("Invalid input."
            sys.exit("Usage: python vmsim.py –n <numframes> -a <opt|clock|fifo|work> [-t <tau>] <tracefile>")
    elif len(sys.argv) == 8:
        if sys.argv[1] == '-n' and sys.argv[3] == '-a' \
                and sys.argv[4] == 'work' and sys.argv[5] == '-t' and sys.argv[7] is not None:

            number_of_frames = int(sys.argv[2])
            algorithm = sys.argv[4]
            tau = int(sys.argv[6])
            file = sys.argv[7]
        else:
            # sys.exit("Invalid input."
            sys.exit("Usage: python vmsim.py –n <numframes> -a <opt|clock|fifo|work> [-t <tau>] <tracefile>")
    else:
        # sys.exit("Invalid input length."
        sys.exit("Usage: python vmsim.py –n <numframes> -a <opt|clock|fifo|work> [-t <tau>] <tracefile>")

    simulate(number_of_frames, algorithm, file, tau)


if __name__ == "__main__":
    main()
