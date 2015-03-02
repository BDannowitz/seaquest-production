#!/usr/bin/python

import sys
import MySQLdb as Mdb
import subprocess
import time
import datetime

sys.path.append('../modules')
import productions
import servers


def find_server(run_list):
    """ 
    Takes a list of runs
    Outputs a dictionary of a server name and its corresponding list of runs
    Adds to 'NoMatch' in case of no match
    """

    server_dict = servers.server_dict
    prod_dict = productions.get_productions()

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
        exists = 1 if cur.rowcount > 0 else 0

        db.close()

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)

    return exists


def get_prod_list():

    server = 'e906-db3.fnal.gov'
    port = 3306
    production = 'merged_roadset57_R004_V003'

    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=port, db=production)
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

    


def main(filename):
    """
    Take a filename containing a list of 6-digit run numbers
    Find existing productions on our MySQL servers
    Check to see if that run's had its ktrack table uploaded to it
    If so, mark its ktracked field in summary.production to 1
    """
    
    server_dict = servers.server_dict
    
    run_list = []

    # Read file into list
    f = open(filename, 'r')
    run_list = f.read().splitlines()
    f.close()

    prod_list = get_prod_list()

    for run in run_list:
        if run not in prod_list:
            print run


if __name__ == "__main__":
    klist = "/seaquest/users/liuk/reco57_upload/list.txt"
    main(klist)

