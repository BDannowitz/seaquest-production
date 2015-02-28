#!/usr/bin/python

import sys
import math

def split_file(input_file, n_files):
    
    output_file_base = "list-"

    f = open(input_file, 'r')
    run_list = f.read().splitlines()
    f.close()

    length = len(run_list)

    lines_per_file = math.ceil(length / n_files)

    file_number = 0

    while file_number < n_files:
        print 'writing file number', file_number+1, 'of', n_files
        output_file = '%s%.02d.txt' % (output_file_base, file_number)
        f = open(output_file, 'w')
        if (file_number*lines_per_file + lines_per_file >= length) and (file_number*lines_per_file < length):
            for index in range(int(file_number*lines_per_file),int(length-1)):
                f.write(str(run_list[index]) + '\n')
        elif (file_number*lines_per_file + lines_per_file < length) and (file_number*lines_per_file < length):
            for index in range(int(file_number*lines_per_file),int(file_number*lines_per_file + lines_per_file)):
                f.write(str(run_list[index])+ '\n')
        f.close()
        file_number = file_number + 1



def main():
    #input_file = "/seaquest/users/dannowi1/seaquest-dannowitz/load-tracking/undone.txt"
    input_file = sys.argv[1]
    n_files = int(sys.argv[2])
    split_file(input_file, n_files)


if __name__ == "__main__":
    main()
