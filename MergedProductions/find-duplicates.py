#!/usr/bin/python

"""Find Duplicates.

Usage:
  find-duplicates.py  
  find-duplicates.py -h | --help
  find-duplicates.py --version

Options:
  -h --help                  Show this screen.
  --version                  Show version.
"""

import sys
import MySQLdb as mdb

sys.path.append("../modules")
from servers import server_dict
from docopt import docopt


def get_prods(server):

    query = 'SELECT run FROM summary.production WHERE ktracked=1 OR jtracked=1'

    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=server_dict[server]['port'])
        cur = db.cursor()

        cur.execute(query)

        run_list = [item[0] for item in cur.fetchall()]

        db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return -1

    return run_list


def main():

    arguments = docopt(__doc__, version='FindDupes! 0.1')

    master_run_list = []
    duplicate_run_list = []

    for server in server_dict:
        run_list = []
        run_list = get_prods(server)
        for run in run_list:
            if run not in master_run_list:
                master_run_list.append(run)
            else:
                duplicate_run_list.append(run)

    print "Duplicate runs:"
    duplicate_run_list.sort()
    print duplicate_run_list
    run_server_set = [] 
    for server in server_dict:
        server_run_list = get_prods(server)
        for run in duplicate_run_list:
            if run in server_run_list:
                run_server_set += ((str(run), server))

    run_server_set.sort()
    for entry in run_server_set:
        print entry 


if __name__ == '__main__':
    main()

