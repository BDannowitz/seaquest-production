#!/usr/bin/python

import MySQLdb
import MySQLdb.cursors

MY_HOST = "e906-db1.fnal.gov"
MY_PORT = 3306
MY_USER = "production"
MY_PASS = "**REMOVED**"

###
# Objective:
# - Look at all of the R001 productions on DB1, and record how many spills each one has
# - Go to each of our fully decoded R004 production servers and record how many spills each of those has
#       - Report when the number of spills in a R001 production exceeds that of a decoded R004 production
###

# Connect to e906-db1, the home of our R001 productions
db = MySQLdb.connect(host=MY_HOST, port=MY_PORT, user=MY_USER, passwd=MY_PASS, cursorclass=MySQLdb.cursors.DictCursor)
cur = db.cursor()

r001_prods = {}

# Get list of R001 productions
try:
    cur.execute("SHOW DATABASES LIKE 'run\_______\_R001'")
    r001_prods = cur.fetchall()
except MySQLdb.Error, e:
    try:
        print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
    except IndexError:
        print "MySQL Error: %s" % str(e)

# Create a dictionary that will keep track of how many spills a given run has for the R001/R004 productions
prod_dict = {}

print "Getting R001 Spill counts from db1..."
for r001_prod in r001_prods:

    # Extract the run number
    run = int(r001_prod['Database (run\\_______\\_R001)'][4:10])

    # Set default schema
    query_string = "USE %s" % r001_prod['Database (run\\_______\\_R001)']
    cur.execute(query_string)

    # Make sure there's a Spill table
    cur.execute("SHOW TABLES LIKE 'Spill'")
    spill_exists = cur.rowcount

    spill_count = 0

    if spill_exists:

        # Count the number of unique spillID's in the Spill table to get the number of spills
        query_string = """SELECT COUNT(*) AS `spill_count`
                                  FROM ( SELECT DISTINCT spillID FROM %s.Spill ) t1""" % r001_prod[
            'Database (run\\_______\\_R001)']
        try:
            cur.execute(query_string)
            spill_count = cur.fetchone()

            # Add an entry to the dictionary for this run, defaulting R004 to -1
            prod_dict[run] = {'R001': int(spill_count['spill_count']), 'R004': -1, 'R004-host': 'NULL'}

        except MySQLdb.Error, e:
            try:
                print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
            except IndexError:
                print "MySQL Error: %s" % str(e)

# We've created our dictionary. Time to count the R004 spills.
db.close()

# Here are our production servers
servers = ("e906-db1.fnal.gov", "e906-db2.fnal.gov", "e906-db3.fnal.gov", "seaquel.physics.illinois.edu")

for server in servers:

    print "Getting R004 spill counts from %s..." % server

    if server == "seaquel.physics.illinois.edu":
        MY_PORT = 3283
    else:
        MY_PORT = 3306

    db = MySQLdb.connect(host=server, port=MY_PORT, user=MY_USER, passwd=MY_PASS,
                         cursorclass=MySQLdb.cursors.DictCursor)

    cur = db.cursor()

    cur.execute("SELECT run, production FROM summary.production WHERE decoded=1")

    prod_sub_dict = cur.fetchall()

    for row in prod_sub_dict:

        run = int(row['run'])
        query_string = "USE %s" % row['production']
        cur.execute(query_string)

        # Make sure there's a Spill table
        cur.execute("SHOW TABLES LIKE 'Spill'")
        spill_exists = cur.rowcount

        if spill_exists:

            # Count the number of unique spillID's in the Spill table to get the number of spills
            query_string = """SELECT COUNT(*) AS `spill_count`
                                          FROM ( SELECT DISTINCT spillID FROM %s.Spill ) t1""" % row['production']
            try:
                cur.execute(query_string)
                spill_count = cur.fetchone()

                # Add an entry to the dictionary for this run, defaulting R004 to -1
                if run in prod_dict:
                    prod_dict[run]['R004'] = int(spill_count['spill_count'])
                    prod_dict[run]['R004-host'] = server
                else:
                    prod_dict[run] = {'R001': -1, 'R004': int(spill_count['spill_count']), 'R004-host': server}

            except MySQLdb.Error, e:
                try:
                    print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
                except IndexError:
                    print "MySQL Error: %s" % str(e)

        else:
            prod_dict[run]['R004'] = 0
            prod_dict[run]['R004-host'] = server

    db.close()

print "run\tR001\tR004\tR004-host"
for entry in prod_dict:
    if 0 <= prod_dict[entry]['R004'] != prod_dict[entry]['R001']:
        print str(entry) + "\t" + str(prod_dict[entry]['R001']) + "\t" + str(prod_dict[entry]['R004']) + "\t" + \
              prod_dict[entry]['R004-host']
