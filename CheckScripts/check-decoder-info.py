#!/usr/bin/python

import MySQLdb as Mdb
import MySQLdb.cursors
import servers
import productions

###
# Objective:
# - Gather list of all R004 schemas
#  - Look at their decoderInfo table to make sure all status_history's end with "0"
#  - Report list of productions and their servers that have an error
##


def get_bad_decoderInfo_runs():
    server_dict = servers.server_dict
    bad_run_dict = {}

    for server in server_dict:

        print "Getting R004 irregular decoderInfo entry counts from %s..." % server

        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=server_dict[server]['port'],
                         cursorclass=MySQLdb.cursors.DictCursor)
        cur = db.cursor()

        cur.execute("SELECT s.run, s.production FROM summary.production s "
                    "INNER JOIN merged_roadset57_R004.production m USING(run) "
                    "WHERE s.decoded=1 AND m.decoded=1")

        prod_sub_dict = cur.fetchall()

        for row in prod_sub_dict:

            run = int(row['run'])
            query_string = "USE %s" % row['production']
            cur.execute(query_string)

            # Make sure there's a Spill table
            cur.execute("SHOW TABLES LIKE 'decoderInfo'")
            infotable_exists = cur.rowcount

            if infotable_exists:

                # Count the number of unique spillID's in the Spill table to get the number of spills
                # query_string = """SELECT COUNT(*) AS `bad_count`
                #                   FROM decoderInfo
                #                   WHERE RIGHT(status_history,1)!="0" OR status_history IS NULL"""
                query_string = """SELECT COUNT(*) AS `bad_count`
                                  FROM decoderInfo
                                  WHERE RIGHT(status_history,4)!="-1 0" OR status_history IS NULL"""

                try:
                    cur.execute(query_string)
                    bad_count = cur.fetchone()

                    # Add an entry to the dictionary for this run, defaulting R004 to -1
                    if int(bad_count['bad_count']) > 0:
                        bad_run_dict[run] = {'bad_count': int(bad_count['bad_count']), 'host': server}

                except Mdb.Error, e:
                    try:
                        print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
                    except IndexError:
                        print "MySQL Error: %s" % str(e)

        db.close()

    print "run\tbad_count\thost"
    for entry in bad_run_dict:
        print str(entry) + "\t" + str(bad_run_dict[entry]['bad_count']) + "\t" + str(bad_run_dict[entry]['host'])


def check_latest_runs():

    server_dict = servers.server_dict
    bad_run_dict = {}

    for server in server_dict:

        bad_run_dict[server] = []

        try:
            db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                             host=server, port=server_dict[server]['port'],
                             cursorclass=MySQLdb.cursors.DictCursor)
            cur = db.cursor()

            cur.execute("""SELECT run, production
                           FROM summary.production s
                           WHERE productionStart > '2015-01-01 00:00:01'""")

            rows = cur.fetchall()
            print str(cur.rowcount) + "new runs to check on " + server

            for row in rows:
                query_string = "SELECT codaEventID FROM " + \
                    row['production'] + ".decoderInfo WHERE status_history IS NULL OR " + \
                    "(status_history IS NOT NULL AND status_history!='-1 0 ')"
                cur.execute(query_string)
                if cur.rowcount > 0:
                    bad_run_dict[server].append(row['production'])

        except Mdb.Error, e:
                try:
                    print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
                except IndexError:
                    print "MySQL Error: %s" % str(e)

    for server, bad_prods in bad_run_dict.iteritems():
        print server, ":", len(bad_prods)
        for bad_prod in bad_prods:
            print bad_prod


def main():
    check_latest_runs()


if __name__ == "__main__":
    main()