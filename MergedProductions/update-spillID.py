#!/usr/bin/python

"""Merge. One script to rule them all, one script to find them.
    One script to bring them all and in the schemas bind them.

Usage:
  update-spillID.py --server=<server> --production=<production> 
  update-spillID.py -h | --help
  update-spillID.py --version

Options:
  -h --help                  Show this screen.
  --version                  Show version.
  --server=<server>          Update spillID for k* table on <server> 
  --production=<production>  Update the spillID for k* table values on <production>
"""

import sys
import MySQLdb as mdb

sys.path.append("../modules")
from servers import server_dict
from docopt import docopt


def schema_exists(server, schema):
    """
    Takes a server and schema
    Returns:
        1 if schema exists (case-sensitive)
        0 if schema does not exist
       -1 if query or connection error occurs
    """

    exists = -1

    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=server_dict[server]['port'])
        cur = db.cursor()
        cur.execute("SHOW DATABASES LIKE '" + schema + "'")
        exists = 1 if cur.rowcount > 0 else 0

        return exists

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return -1


def table_exists(server, schema, table):
    """
    Takes a server, schema, and table name
    Returns:
        1 if table exists (case-sensitive)
        0 if table does not exist
       -1 if query or connection error occurs
    """

    exists = -1

    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         db=schema, host=server,
                         port=server_dict[server]['port'])
        cur = db.cursor()
        cur.execute("SHOW TABLES LIKE '" + table + "'")
        exists = 1 if cur.rowcount > 0 else 0

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return -1

    return exists


def set_keys(server, production, key_flag):

    if schema_exists(server, production):

        try:
            db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                             host=server, port=server_dict[server]['port'], db=production)
            cur = db.cursor()

            cur.execute('SHOW TABLES')
            tables = cur.fetchall()
            for table in tables:
                if key_flag:
                    key_string = 'ALTER TABLE ' + table[0] + ' ENABLE KEYS'
                else:
                    key_string = 'ALTER TABLE ' + table[0] + ' DISABLE KEYS'
                cur.execute(key_string)

            db.close()

            return 0

        except mdb.Error, e:

            print "Error %d: %s" % (e.args[0], e.args[1])
            return 1

    return 0


def run_exists(runID, production):
    """
    Looks at the 'production' table in the specified production on each server to see if the run already exists there

    :param runID:
    :param production:
    :return: 0 if it is not found in the production on any of the servers
             1 if it is found on a server
    """

    exists = 0
    query = 'SELECT production FROM production WHERE run=' + str(runID)

    for server in server_dict:

        try:

            db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                             host=server, port=server_dict[server]['port'], db=production)
            cur = db.cursor()

            cur.execute(query)

            exists = (exists | 1) if cur.rowcount > 0 else exists

            db.close()

        except mdb.Error, e:

            print "Error %d: %s" % (e.args[0], e.args[1])
            return -1

    return exists


def get_ktrack_prods(server, production):

    query = 'SELECT DISTINCT runID FROM kTrack'

    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=server_dict[server]['port'], db=production)
        cur = db.cursor()

        cur.execute(query)

        run_list = [item[0] for item in cur.fetchall()]

        db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return -1

    return run_list


def update_ktrack(server, production, run):

    ktrack_query = """
                   UPDATE kTrack k 
                        LEFT JOIN Event e USING(runID, eventID)
                   SET k.spillID = e.spillID
                   WHERE k.runID = %d AND e.runID = %d
                   """
    kdimuon_query = """
                    UPDATE kDimuon k 
                        LEFT JOIN Event e USING(runID, eventID)
                    SET k.spillID = e.spillID
                    WHERE k.runID = %d AND e.runID = %d
                    """
    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=server_dict[server]['port'], db=production)
        
        cur = db.cursor()

        cur.execute(ktrack_query % (run, run))
        cur.execute(kdimuon_query % (run, run))

        db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return -1

    return 0 


def main():

    arguments = docopt(__doc__, version='UpdateKTrack 0.1')
    print arguments

    if arguments['--server'] in server_dict:
        run_list = get_ktrack_prods(arguments['--server'], arguments['--production'])
        print run_list
        for run in run_list:
            print 'Updating k* tables for run ' + str(run) + ' on ' + arguments['--server']
            update_ktrack(arguments['--server'], arguments['--production'], run)


if __name__ == '__main__':
    main()

