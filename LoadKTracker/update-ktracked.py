#!/usr/bin/python

import sys
import MySQLdb as Mdb
import subprocess
import time
import datetime

sys.path.append('../modules')
import productions
import servers

uploader = "/seaquest/users/liuk/reco57_upload/sqlResWriter"
old_version = "_r1_0"
ksource = "/seaquest/users/liuk/reco57_upload/rootfiles"


def find_server(run_list):
    """ 
    Takes a list of runs
    Outputs a dictionary of a server name and its corresponding list of runs
    Adds to 'NoMatch' in case of no match
    """

    server_dict = servers.server_dict
    prod_dict = productions.get_productions(revision="R000")

    found_run_dict = {}

    for server in server_dict:
        found_run_dict[server] = []

    found_run_dict['NoMatch'] = []

    for run in run_list:
        production = "run_" + run + "_R000"
        found = False 

        for server in server_dict:
            if production in prod_dict[server]:
                found_run_dict[server].append(production)
                found = True

        if not found:
            found_run_dict['NoMatch'].append(production)

    return found_run_dict 


def update_decoderinfo(production, server, port):
    """
    After the successful upload of ktrackerInfo, update the summary.production
    table to indicate that the schema has ktracker data
    """
    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=port, db=production)
        cur = db.cursor()

        cur.execute("UPDATE summary.production SET ktracked=1 WHERE production='" + production + "'")

        db.close()

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)


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


def check_kinfo(server, port, production):
    """
    After the successful upload of ktrackerInfo, update the summary.production
    table to indicate that the schema has ktracker data
    """

    exists = 0

    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=port, db=production)
        cur = db.cursor()

        cur.execute("SELECT * FROM kInfo WHERE infoValue='ROOT decoder/r1.2.0'")
        exists = 1 if cur.rowcount > 0 else 0

        db.close()

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)

    return exists


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

    found_run_dict = find_server(run_list)
   
    for server, found_runs in found_run_dict.iteritems():
        if server != "NoMatch":
            for run in found_runs:
                if table_exists(server, server_dict[server]['port'], run, 'kInfo') and check_kinfo(server, server_dict[server]['port'], run):
                    print 'Updating entry for:', run, 'on', server
                    update_decoderinfo(run, server, server_dict[server]['port'])
                    pass
                else:
                    print 'Could not find kInfo in ' + run + ' on ' + server
    

if __name__ == "__main__":
    main(sys.argv[1])

