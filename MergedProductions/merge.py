#!/usr/bin/python

"""Merge. One script to rule them all, one script to find them.
    One script to bring them all and in the schemas bind them.

Usage:
  merge.py (<production> [<production>...] | --all | --roadset=ROADSET | --range <minrunID> <maxrunID> )
           (--overwrite | --passive)
           (--suffix=<suffix>)
           [--server=<server>]
           [--disable-keys=<bool>]
           [(--jtracked=<bool> --ktracked=<bool>)]
  merge.py clear <production>... --suffix=<suffix>
  merge.py set-keys (on|off) --suffix=<suffix>
  merge.py -h | --help
  merge.py --version

Options:
  -h --help                      Show this screen.
  --version                      Show version.
  --all                          Merge all productions from all servers.
  <production>...                One (or more) production names
  --roadset=<roadset>            Merge only a specific roadset.
  --range <minrunID> <maxrunID>  Merge a range of runs
  --suffix=<suffix>              Merged production schema name suffix.
                                 Resulting schemas will be like:
                                 merged_roadsetROADSET_SUFFIX
                                 where ROADSET is determined by run
                                 number.
  clear                          Delete the listed productions from the merged
                                 productions defined by the <suffix> argument
  --overwrite                    Clear any previously existing data before
                                 merging a production.
  --passive                      Skip merging a production if data is
                                 found to exist previously.
  --server=<server>              Merge only productions from a specific
                                 server. Useful when doing parallel merging.
  set-keys                       Used only to turn indexes for <suffix> productions
                                 on and off.
  --disable-keys=<bool>          Disable/Enable the indexes of all merged
                                 production merging process. May be disruptive to
                                 ongoing analysis.
                                 [Default: 0]
  --jtracked=<bool>              Merge productions that have SQERP tracking
                                 results. If you define one of these, you must
                                 define both. Setting one to 0 tightens the
                                 selection. [Default: 1]
  --ktracked=<bool>              Merge productions that have kTracker
                                 tracking results. If you define one of these,
                                 you must define both. Setting one to 0 tightens
                                 the selection.[Default: 1]
"""

import sys
import MySQLdb as mdb
import subprocess
import os

sys.path.append("../modules")
from productions import roadset_dict
from servers import server_dict
from docopt import docopt

# Here is the list of tables to merge. If you add tables, please also add them to the
#   appropriate category lists below
tbl_list = ['Run', 'Spill', 'Event', 'BeamDAQ', 'Beam', 'Target', 'QIE', 'Scaler']
k_tables = ['kTrack', 'kDimuon']
j_tables = ['jTrack', 'jDimuon']
all_tbls = tbl_list + k_tables + j_tables

# Clearing these tables will use the runID.
clear_with_runID = ['Run', 'Spill', 'Event', 'Beam', 'Target', 'QIE',
                  'jTrack', 'jDimuon', 'kTrack', 'kDimuon']
# All other tables will be cleared with spillID
clear_with_spillID = ['BeamDAQ', 'Scaler']

# When dumping, we exclude spillID=0, so we need a list of base tables (not track) that have spillID field
has_spillID = ['Spill', 'Event', 'BeamDAQ', 'Beam', 'Target', 'QIE', 'Scaler']
no_spillID = ['Run']

# Only merge data that (1) has a kInfo table, and (2) that kInfo table has this value in it
kInfo_value = 'r1.4.0'


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


def check_jtracked(server, production):
    """
    Do whatever checks on jtrack tables that needs to be done to certify it for merging

    :param server: What server in the server_dict that the production exists on
    :param production: The production that we're checking
    :return: 1 if everything is okay (to keep jtracked = True)
             0 if something does not pass inspection
    """

    # With jtracked data, there's a chance that the runID in the tables is 0
    #   See if runID=0 for the jTrack and jDimuon
    #   If they are, set it to the appropriate runID
    #   Upon error, return 0

    if table_exists(server, production, 'jDimuon') and table_exists(server, production, 'jTrack'):

        try:

            db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                             host=server, port=server_dict[server]['port'], db=production)
            cur = db.cursor()
            cur.execute("""
                        SELECT DISTINCT runID FROM jDimuon
                        WHERE runID = 0
                        UNION ALL
                        SELECT DISTINCT runID FROM jTrack
                        WHERE runID = 0
                        """)
            zeros_exist = 1 if cur.rowcount > 0 else 0

            if zeros_exist:
                cur.execute("""
                            UPDATE jDimuon j
                            INNER JOIN Spill s USING(spillID)
                            SET j.runID = s.runID
                            """)
                cur.execute("""
                            UPDATE jTrack j
                            INNER JOIN Spill s USING(spillID)
                            SET j.runID = s.runID
                            """)

            return 1

        except mdb.Error, e:

            print "Error %d: %s" % (e.args[0], e.args[1])
            return 0

    else:
        # j* tables don't exist
        return 0


def check_ktracked(server, production):
    """
    Do whatever checks on ktrack tables that needs to be done to certify it for merging

    :param server: What server in the server_dict that the production exists on
    :param production: The production that we're checking
    :return: 1 if everything is okay (to keep ktracked = True)
             0 if something does not pass inspection
    """

    # For kTracker, the table format can change from version to version.
    #   Only merge if the version in kInfo matches the kInfo_value set at the top

    if table_exists(server, production, 'kInfo'):

        try:

            db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                             host=server, port=server_dict[server]['port'], db=production)
            cur = db.cursor()
            cur.execute("SELECT * FROM kInfo WHERE infoValue LIKE '%" + kInfo_value + "%'")
            exists = 1 if cur.rowcount > 0 else 0

            db.close()

            return exists

        except mdb.Error, e:

            print "Error %d: %s" % (e.args[0], e.args[1])
            return 0

    else:
        # kInfo does not exist
        return 0


def check_scaler(server, production):
    """
    Do whatever checks on Scaler tables that needs to be done to certify it for merging

    :param server: What server in the server_dict that the production exists on
    :param production: The production that we're checking
    :return: 1 if everything is okay (to keep ktracked = True)
             0 if something does not pass inspection
    """

    # For the Scaler table, some of the first productions out there had a runID field in the
    #   Scaler table. This will check to see if that field is in the table, and if it is there,
    #   it will drop the field.

    if table_exists(server, production, 'Scaler'):

        try:

            db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                             host=server, port=server_dict[server]['port'], db=production)
            cur = db.cursor()
            cur.execute("SHOW FIELDS IN Scaler LIKE 'runID'")
            exists = 1 if cur.rowcount > 0 else 0

            if exists:
                cur.execute("ALTER TABLE Scaler DROP `runID`")

            db.close()

            return 1 

        except mdb.Error, e:

            print "Error %d: %s" % (e.args[0], e.args[1])
            return 0

    else:
        # Scaler does not exist
        return 0


def dump_production(server, production_name, output_file, jtracked, ktracked):

    db_user = 'production'
    db_pass = '***REMOVED***'

    runID = production_name[4:10]
    jtracked = int(jtracked) if isinstance(jtracked, basestring) else jtracked
    ktracked = int(ktracked) if isinstance(ktracked, basestring) else ktracked

    # Assemble the dump statement
    where_string = "-w'spillID>0'"
    cmd = ["mysqldump",
           "--host=" + server,
           "--port=" + str(server_dict[server]['port']),
           "--user=" + db_user,
           "--password=" + db_pass,
           "--skip-add-drop-table",
           "--no-create-info",
           "--single-transaction",
           "--skip-disable-keys",
           "--compress",
           where_string,
           production_name]

    if jtracked:
        # Perform whatever checks that need to be done on jTrack and jDimuon
        jtracked = check_jtracked(server, production_name)
    if ktracked:
        # Perform whatever checks that need to be done on kTrack and kDimuon
        ktracked = check_ktracked(server, production_name)

    if ktracked | jtracked == 0:
        # If there is no good tracking, skip it
        return -1 

    if not check_scaler(server, production_name):
        print 'There was a problem in checking or altering the Scaler table in ' + production_name + ' on ' + server
        return 1

    for table in has_spillID:
        cmd.append(table)
    if ktracked:
        for table in k_tables:
            cmd.append(table)
    if jtracked:
        for table in j_tables:
            cmd.append(table)

    cmd = " ".join(cmd)

    try:
        with open(output_file, 'a') as f:
            subprocess.check_call(cmd, stdout=f, stderr=subprocess.STDOUT, shell=True)

    except subprocess.CalledProcessError, e:
        print "mysqldump stdout output:\n", e.output
        return 1

    # Assemble the dump statement
    cmd = ["mysqldump",
           "--host=" + server,
           "--port=" + str(server_dict[server]['port']),
           "--user=" + db_user,
           "--password=" + db_pass,
           "--skip-add-drop-table",
           "--no-create-info",
           "--single-transaction",
           "--skip-disable-keys",
           "--compress",
           production_name]

    for table in no_spillID:
        cmd.append(table)

    try:
        with open(output_file, 'a') as f:
            subprocess.check_call(cmd, stdout=f, stderr=subprocess.STDOUT)

    except subprocess.CalledProcessError, e:
        print "mysqldump stdout output:\n", e.output
        return 1

    # Don't forget to throw the summary.production entry into the mix!
    where_string = "-w'run=" + str(int(runID)) + "'"
    cmd = ["mysqldump",
           "--host=" + server,
           "--port=" + str(server_dict[server]['port']),
           "--user=" + db_user,
           "--password=" + db_pass,
           "--skip-add-drop-table",
           "--no-create-info",
           "--single-transaction",
           "--skip-disable-keys",
           "--compress",
           where_string,
           "summary",
           "production"]

    cmd = " ".join(cmd)

    try:
        with open(output_file, 'a') as f:
            subprocess.check_call(cmd, stdout=f, stderr=subprocess.STDOUT, shell=True)

    except subprocess.CalledProcessError, e:
        print "mysqldump stdout output:\n", e.output
        return 1

    return 0


def create_schema(server, schema):

    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=server_dict[server]['port'])
        cur = db.cursor()

        print 'Creating schema ' + schema + ' on ' + server
        cur.execute('CREATE DATABASE IF NOT EXISTS ' + schema)

        db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return 1


def make_dest_tables(server, schema):
    db_user = 'production'
    db_pass = '***REMOVED***'

    if not schema_exists(server, schema):
        if create_schema(server, schema):
            print 'Error creating new merged schema ' + schema + ' on ' + server

    for table in all_tbls:
        if not table_exists(server, schema, table):
            try:
                with open('table_defs/' + table + '.sql', 'r') as fd:
                    subprocess.Popen(['mysql',
                                      '-u', db_user,
                                      '-p' + db_pass,
                                      '-P', str(server_dict[server]['port']),
                                      '-h', server,
                                      '-C',
                                      schema], stdin=fd).wait()

            except subprocess.CalledProcessError, e:
                print "mysqldump stdout output:\n", e.output, e.message
                return 1

        if not table_exists(server, schema, 'production'):
            try:
                with open('table_defs/production.sql', 'r') as fd:
                    subprocess.Popen(['mysql',
                                      '-u', db_user,
                                      '-p' + db_pass,
                                      '-P', str(server_dict[server]['port']),
                                      '-h', server,
                                      '-C',
                                      schema], stdin=fd).wait()

            except subprocess.CalledProcessError, e:
                print "mysqldump stdout output:\n", e.output, e.message
                return 1

    return 0


def load_dump(merged_production, dump_file):

    db_user = 'production'
    db_pass = '***REMOVED***'

    for server in server_dict:
        try:

            with open(dump_file, 'r') as fd:
                print 'Loading ' + dump_file + ' into ' + merged_production + ' on ' + server
                subprocess.Popen(['mysql',
                                  '-u', db_user,
                                  '-p' + db_pass,
                                  '-P', str(server_dict[server]['port']),
                                  '-h', server,
                                  '-C',
                                  merged_production], stdin=fd).wait()

        except subprocess.CalledProcessError, e:
            print "mysql dump loading error:\n", e.output
            return 1

    return 0


def get_roadset(runID):

    my_roadset = None
    # Find which merged production that corresponds to this run
    for roadset in roadset_dict:
        if roadset_dict[roadset]['lower'] <= runID <= roadset_dict[roadset]['upper']:
            my_roadset = roadset

    if not my_roadset:
        print 'Run ' + str(runID) + ' falls outside of defined roadsets.'
        print 'If this is in error, modify the \'productions.py\' module to include this roadset'
        return None

    return my_roadset


def clear_run(runID, minSpillID, maxSpillID, production):
    """
    Clear out the run's worth of information from each merged production.
    Which merged production depends on the runID, the roadset run range, and the
        merged suffix defined above
    :param runID: The runID for the production to be cleared out
    :param minSpillID: The first (non-zero) spillID of the production
    :param maxSpillID: The last spillID of the production
    :param production: The schema name to clear the data from (on each server)
    :return: 0 for success
             1 for failure
    """

    for server in server_dict:
        print 'Clearing out run ' + str(runID) + ' data from ' + production + ' on ' + server
        if schema_exists(server, production):
            try:

                db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                                 host=server, port=server_dict[server]['port'], db=production)
                cur = db.cursor()

                for table in clear_with_runID:
                    if table_exists(server, production, table):
                        cur.execute('DELETE FROM ' + table + ' WHERE runID = ' + str(runID))
                for table in clear_with_spillID:
                    if table_exists(server, production, table):
                        cur.execute('DELETE FROM ' + table + ' WHERE spillID BETWEEN ' +
                                    str(minSpillID) + ' AND ' + str(maxSpillID))
                if table_exists(server, production, 'production'):
                    cur.execute('DELETE FROM production WHERE run = ' + str(runID))

                db.close()

            except mdb.Error, e:

                print "Error %d: %s" % (e.args[0], e.args[1])
                return 1

    return 0


def get_spill_range(server, production):
    """
    When clearing out data from merged productions (to prevent duplicates), we must
        know the spill range in addition to the runID. This function will take a production
        on a given server and output a tuple of a minimum (non-zero) spillID and a maximum
        (non-zero) spillID
    :param server: a server from the server_dict
    :param production: a production that exists on the above server
    :return:
    """

    minspill, maxspill = (-1, -1)

    if not table_exists(server, production, 'Spill'):
        print 'Spill table does not exist in schema ' + production + ' on ' + server
        return minspill, maxspill

    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=server_dict[server]['port'], db=production)
        cur = db.cursor()

        query_string = '''
                       SELECT MIN(spillID), MAX(spillID)
                       FROM Spill
                       WHERE spillID > 0
                       '''

        cur.execute(query_string)
        row = cur.fetchone()

        minspill, maxspill = (row[0], row[1])
        # print 'Min/Max SpillID\'s for ' + production + ': ' + str(minspill) + '/' + str(maxspill)

        db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return minspill, maxspill

    return minspill, maxspill


def get_tracked_prods(server, jtracked, ktracked):
    """
    Given a server and what type of tracking your looking for, get a list of production
        (schema names) that contain that tracking. Having both enabled will give you a
        logical OR of all valid productions
    :param server: MySQL server from our server_dict
    :param ktracked: Boolean indicating if you want to get jtracked productions
    :param jtracked: Boolean indicating if you want to get ktracked productions
    :return: List of tracked production names
    """

    jtracked = int(jtracked) if isinstance(jtracked, basestring) else jtracked
    ktracked = int(ktracked) if isinstance(ktracked, basestring) else ktracked


    prod_dict = {}

    try:

        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         host=server, port=server_dict[server]['port'])
        cur = db.cursor()

        query_string = '''
                       SELECT DISTINCT production, jtracked, ktracked
                       FROM summary.production
                       WHERE decoded > 0
                       '''
        if jtracked and ktracked:
            query_string += ' AND (jtracked = 1 OR ktracked = 1)'
        elif jtracked and not ktracked:
            query_string += ' AND jtracked = 1'
        elif not jtracked and ktracked:
            query_string += ' AND ktracked = 1'

        cur.execute(query_string)

        results = cur.fetchall()
        for row in results:
            prod_dict[row[0]] = {'server': server,
                                 'roadset': get_roadset(int(row[0][4:10])),
                                 'jtracked': int(row[1]),
                                 'ktracked': int(row[2])}

        db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return 1

    return prod_dict


def get_production_details(production, jtracked, ktracked):
    """
    Given a production name and what type of tracking your looking for, get a single-
     entry dictionary containing a dictionary of all the details
    :param production: Name of the production schema
    :param ktracked: Boolean indicating if you want to get jtracked productions
    :param jtracked: Boolean indicating if you want to get ktracked productions
    :return: Dictionary of a dictionary of production details
    """

    prod_dict = {}

    for server in server_dict:

        try:

            db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                             host=server, port=server_dict[server]['port'])
            cur = db.cursor()

            query_string = '''
                           SELECT DISTINCT production, jtracked, ktracked
                           FROM summary.production
                           WHERE decoded > 0
                           '''

            query_string += ' AND production = \'' + production + '\''

            if jtracked and ktracked:
                query_string += ' AND (jtracked = 1 OR ktracked = 1)'
            elif jtracked and not ktracked:
                query_string += ' AND jtracked = 1'
            elif not jtracked and ktracked:
                query_string += ' AND ktracked = 1'

            cur.execute(query_string)

            if cur.rowcount > 0:
                row = cur.fetchone()
                prod_dict[row[0]] = {'server': server,
                                     'roadset': get_roadset(int(row[0][4:10])),
                                     'jtracked': int(row[1]),
                                     'ktracked': int(row[2])}

            db.close()

        except mdb.Error, e:

            print "Error %d: %s" % (e.args[0], e.args[1])
            return 1

    return prod_dict


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


def merge_one(server, production, jtracked, ktracked, overwrite, merged_suffix):
    """
    1. Takes a production located on one server
    2. Finds its min/max spill, runID
    3. Looks to see if this data already exists in merged productions
    4. If data exists in merged:
        a. If passive, exit
        b. If overwrite, then clear out previously existing information for that run from all
           merged productions
    5. Dumps that productions pertinent tables
    6. Loads the mysqldump to each production on each server

    :param server: A MySQL server that exists in the server_dict dictionary
    :param production: A production schema 'run_XXXXXX_R00Y' that exists on that server
    :param jtracked: A flag if this production contains jtracked data
    :param ktracked: A flag if this production contains ktracked data
    :param overwrite: If true, will delete previously existing data before dumping and loading
    :param merged_suffix: The suffix of the merged productions that we'll be dealing with
    :return: 0 if successful
             1 on failure
    """

    pid = os.getpid()
    runID = int(production[4:10])

    merged_production = 'merged_roadset' + str(get_roadset(runID)) + '_' + merged_suffix

    minSpillID, maxSpillID = get_spill_range(server, production)
    if (minSpillID, maxSpillID) == (-1, -1):
        print 'Error getting min/max spillID\'s from ' + production + ' on ' + server
        return 1

    if run_exists(runID, merged_production) == 1:
        if overwrite:
            print 'Clearing out ' + merged_production + ' of Run ' + str(runID) + ' data'
            if clear_run(runID, minSpillID, maxSpillID, merged_production):
                print 'Error clearing out data for ' + str(runID) + ' from ' + merged_suffix + ' merged productions'
                return 1
        else:
            print 'Run ' + str(runID) + ' already exists in a merged production. Not going to go any further.'
            return 0

    dump_file = 'merge_dump_' + str(pid) + '.sql'
    if os.path.isfile(dump_file):
        os.remove(dump_file)

    print 'Dumping ' + production + ' into ' + dump_file
    dump_status = dump_production(server, production, dump_file, jtracked, ktracked)
    if dump_status == 1:
        print 'Error dumping ' + production + ' from ' + server + ' to ' + dump_file
        if os.path.isfile(dump_file):
            os.remove(dump_file)
        return 1
    elif dump_status == -1:
        # No tracking passed the checks. Skip this run.
        print 'Tracking did not pass checks. Skipping to next run.'
        return 0

    print 'Loading ' + production + ' in dump file ' + dump_file + ' to ' + merged_production
    if load_dump(merged_production, dump_file):
        print 'Error loading data for ' + str(runID) + ' into ' + merged_production
        if os.path.isfile(dump_file):
            os.remove(dump_file)
        return 1

    if os.path.isfile(dump_file):
        os.remove(dump_file)

    return 0


def main():

    arguments = docopt(__doc__, version='Merge 0.1')

    print arguments

    # Here, we just want to clear out any existing data for the listed productions
    if arguments['clear']:
        if arguments['<production>']:
            for prod in arguments['<production>']:
                # Get required information about where the production is so we can get its spillID range
                prod_dict = get_production_details(prod, 1, 1)
                runID = int(prod[4:10])
                merged_production = 'merged_roadset' + prod_dict[prod]['roadset'] + '_' + arguments['--suffix']
                minSpillID, maxSpillID = get_spill_range(prod_dict[prod]['server'], prod)
                if (minSpillID, maxSpillID) == (-1, -1):
                    print 'Error getting min/max spillID\'s from ' + prod + ' on ' + prod_dict[prod]['server']
                    return 1

                # Then check if there is data in the merged productions for this run
                if run_exists(runID, merged_production) == 1:
                    # If there is, clear it out.
                    print 'Clearing out ' + merged_production + ' of Run ' + str(runID) + ' data'
                    if clear_run(runID, minSpillID, maxSpillID, merged_production):
                        print 'Error clearing out data for ' + str(runID) + ' from ' + \
                              arguments['--suffix'] + ' merged productions'
                        return 1
        # That's all we want to do here!
        print 'Clearing successful. Exiting...'
        return 0

    # Define schema names, create them, and create their tables
    merged_productions = ['merged_roadset' + roadset + '_' + arguments['--suffix'] for roadset in roadset_dict]

    # Here, we just want to turn the indexes on or off
    if arguments['set-keys']:
        key_flag = 1 if arguments['on'] else 0
        for server in server_dict:
            for merged_production in merged_productions:
                print 'Setting keys in ' + merged_production + ' on ' + server
                if set_keys(server, merged_production, key_flag):
                    print 'Error setting keys in ' + merged_production + ' on ' + server
                    print 'You might want to check and see if you should manually re-enable all keys...'
                    return 1
        # This is all we came to do. Exiting...
        return 0
    
    print 'Making tables'
    for merged_production in merged_productions:
        for server in server_dict:
            if make_dest_tables(server, merged_production):
                print 'Error creating tables in ' + merged_production + ' on ' + server
                return 1

    # If --disable-keys is set to 1, then disable the keys of all the tables in each schema
    if arguments['--disable-keys'] == '1':
        print 'Disabling Keys. This may take a few minutes...'
        for server in server_dict:
            for merged_production in merged_productions:
                if set_keys(server, merged_production, 0):
                    print 'Error disabling keys in ' + merged_production + ' on ' + server
                    print 'You might want to check and see if you should manually re-enable all keys...'
                    return 1

    prods_to_merge = {}

    # If the --all option is chosen, get list of all decoded and
    #   tracked productions within our roadsets and merge them
    # If the --roadset option is chosen, do the same, but only merge
    #   ones of the specified roadset
    if arguments['--all'] or arguments['--roadset'] or arguments['--range']:

        # If --server is defined, only look at productions on that server
        if arguments['--server']:
            server_prod_dict = get_tracked_prods(arguments['--server'],
                                                 arguments['--jtracked'], arguments['--ktracked'])
            for prod in server_prod_dict:
                runID = int(prod[4:10])
                # If --roadset is defined, only look at productions in that roadset
                if arguments['--roadset'] and server_prod_dict[prod]['roadset'] == arguments['--roadset']:
                    prods_to_merge[prod] = server_prod_dict[prod]
                elif not arguments['--roadset']:
                    if arguments['--range'] and int(arguments['--range']) <= runID <= int(arguments['<maxrunID>']):
                        prods_to_merge[prod] = server_prod_dict[prod]
                    elif not arguments['--range']:
                        prods_to_merge[prod] = server_prod_dict[prod]

        else:
            # If --server is not defined, look at productions on each server
            for server in server_dict:
                server_prod_dict = get_tracked_prods(server, arguments['--jtracked'], arguments['--ktracked'])
                for prod in server_prod_dict:
                    runID = int(prod[4:10])
                    # If --roadset is defined, only look at productions in that roadset
                    if arguments['--roadset'] and server_prod_dict[prod]['roadset'] == arguments['--roadset']:
                        prods_to_merge[prod] = server_prod_dict[prod]
                    elif not arguments['--roadset']:
                        if arguments['--range'] and int(arguments['--range']) <= runID <= int(arguments['<maxrunID>']):
                            prods_to_merge[prod] = server_prod_dict[prod]
                        elif not arguments['--range']:
                            prods_to_merge[prod] = server_prod_dict[prod]

    # If a finite list of productions are chosen, locate their host servers,
    #   verify they're tracked, and merge them
    if arguments['<production>']:
        for prod in arguments['<production>']:
            print 'Merging production ' + prod
            prod_dict = get_production_details(prod, arguments['--jtracked'], arguments['--ktracked'])
            if len(prod_dict) == 0:
                print 'Production ' + prod + ' either cannot be found or does not match jtracked=' + \
                      arguments['--jtracked'] + ' or ktracked=' + arguments['--ktracked']
            else:
                # If --server is defined, only look at productions on that server
                if arguments['--server'] and prod_dict[prod]['server'] == arguments['--server']:
                    prods_to_merge[prod] = prod_dict[prod]
                elif not arguments['--server']:
                    prods_to_merge[prod] = prod_dict[prod]

    # All that work above was to get a list of productions and their details.
    #   Here, we take that list of productions and details and send them off to get merged.
    for prod in prods_to_merge:
        print prod, prods_to_merge[prod]['server'], prods_to_merge[prod]['roadset'], \
            int(prods_to_merge[prod]['jtracked']), int(prods_to_merge[prod]['ktracked']), arguments['--overwrite']
        merge_one(prods_to_merge[prod]['server'], prod, int(prods_to_merge[prod]['jtracked']),
                  int(prods_to_merge[prod]['ktracked']), arguments['--overwrite'], arguments['--suffix'])

    # If you turned them off, better turn them back on!
    if arguments['--disable-keys'] == '1':
        print 'Enabling Keys. This may take a few minutes...'
        for server in server_dict:
            for merged_production in merged_productions:
                set_keys(server, merged_production, 1)


if __name__ == '__main__':
    main()

