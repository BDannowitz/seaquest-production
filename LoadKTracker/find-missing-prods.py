#!/usr/bin/python

""" Find-Missing. 
    
    Usage:
        find-missing-prods.py --file=<file>
        find-missing-prods.py -h | --help
        find-missing-prods.py --version

    Options:
        --version                      Show version.
        --file=<file>                  List file
        --server=<server>              Server to look to for the check
        --prod=<prod>                  Merged production to check

"""

import sys
import MySQLdb as Mdb
import subprocess
import time
import datetime

sys.path.append('../modules')
import productions
from servers import server_dict
from docopt import docopt


def find_server(run_list):
    """ 
    Takes a list of runs
    Outputs a dictionary of a server name and its corresponding list of runs
    Adds to 'NoMatch' in case of no match
    """

    prod_dict = productions.get_productions('R004')

    found_run_dict = {}

    for server in server_dict:
        found_run_dict[server] = []

    found_run_dict['NoMatch'] = []

    for run in run_list:
        production = "run_" + run + "_R004"
        found = False 

        for server in server_dict:
            if production in prod_dict[server]:
                found_run_dict[server].append(production)
                found = True

        if not found:
            found_run_dict['NoMatch'].append(production)

    return found_run_dict 


def table_exists(server, port, production, table):
    """
    After the successful upload of ktrackerInfo, update the summary.production
    table to indicate that the schema has ktracker data
    """

    exists = 0

    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=port, db=production)
        cur = db.cursor()

        cur.execute("SHOW TABLES LIKE '" + table + "'")
        if cur.rowcount > 0:
            exists = 1 
        else:
            exists = 0

        db.close()

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)

    return exists


def get_prod_list(server, prod):

    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=server_dict[server]['port'], db=prod)
        cur = db.cursor()

        cur.execute('SELECT DISTINCT runID FROM kTrack')

        prod_list = ["%06d" % int(item[0]) for item in cur.fetchall()]

        db.close()
    
        return prod_list

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)

    
def get_summary_prods(server):

    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=server_dict[server]['port'])
        cur = db.cursor()

        cur.execute('SELECT DISTINCT run FROM summary.production WHERE decoded>0')

        prod_list = ["%06d" % int(item[0]) for item in cur.fetchall()]

        db.close()
    
        return prod_list

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)


def main():
    """
    Take a filename containing a list of 6-digit run numbers
    Find existing productions on our MySQL servers
    Check to see if that run's had its ktrack table uploaded to it
    If so, mark its ktracked field in summary.production to 1
    """
   
    arguments = docopt(__doc__, version="FindMissing, Version 0.1")
    print arguments
    
    run_list = []

    # Read file into list
    f = open(arguments['--file'], 'r')
    run_list = f.read().splitlines()
    run_list = sorted(run_list)
    f.close()

    prod_list = get_prod_list(arguments['--server'], arguments['--prod'])

    missing_run_list = []

    missing_count = 0
    # print 'The following are in the list file, but not in the kTrack data from ' + arguments['--prod']
    for run in run_list:
        if run not in prod_list:
            missing_run_list.append(run)
            missing_count += 1
    print str(missing_count) + ' total missing productions'

    missing_count = 0
    # print 'The following are in the kTrack data from ' + arguments['--prod'] + ', but not in the list file'
    for run in prod_list:
        if run not in run_list:
            # print run
            missing_count += 1
    print str(missing_count) + ' total missing productions'

    print missing_run_list

    missing_run_dict = find_server(missing_run_list)

    for server, server_run_list in missing_run_dict.iteritems():
        print server, server_run_list


def find_missing_prods():
    arguments = docopt(__doc__, version="FindMissing, Version 0.1")
    print arguments
    
    run_list = []

    # Read file into list
    f = open(arguments['--file'], 'r')
    run_list = f.read().splitlines()
    run_list = sorted(run_list)
    f.close()

    prod_list = []

    for server in server_dict:
        prod_list += get_summary_prods(server)

    missing_run_list = []

    missing_count = 0
    # print 'The following are in the list file, but not in the kTrack data from ' + arguments['--prod']
    for run in run_list:
        if run not in prod_list:
            missing_run_list.append(run)
            missing_count += 1
    print str(missing_count) + ' total missing productions'

    missing_run_dict = find_server(missing_run_list)
    for server in missing_run_dict:
        print server, ':', len(missing_run_dict[server]), 'untracked productions'
        missing_run_dict[server].sort()
        for prod in missing_run_dict[server]:
            # print str(int(prod[4:10])) + ',',
            print prod[4:10]
        print '\n'


if __name__ == "__main__":
    find_missing_prods()

