#!/usr/bin/python

import MySQLdb as Mdb
import productions
import servers
import userinput


def get_empties_dict(rev):

    server_dict = servers.server_dict
    prod_dict = productions.get_productions(revision=rev)
    hit_query = "SHOW TABLES LIKE 'Hit'"

    empty_dict = {}

    for server, prod_list in prod_dict.iteritems():

        empty_dict[server] = {}

        try:
            db = Mdb.connect(read_default_file='../.my.cnf',
                             read_default_group='guest', host=server,
                             port=server_dict[server]['port'])
            cur = db.cursor()
            for prod in prod_list:
                cur.execute("USE " + prod)
                cur.execute(hit_query)
                hit_exists = cur.rowcount

                if not hit_exists:
                    run = int(prod[4:10])
                    empty_dict[server][prod] = run

        except Mdb.Error, e:
            try:
                print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
            except IndexError:
                print "MySQL Error: %s" % str(e)

    return empty_dict


def get_shells_dict(rev):

    server_dict = servers.server_dict
    prod_dict = productions.get_productions(revision=rev)
    query = "SHOW TABLES"

    shells_dict = {}

    for server, prod_list in prod_dict.iteritems():

        shells_dict[server] = {}

        try:
            db = Mdb.connect(read_default_file='../.my.cnf',
                             read_default_group='guest', host=server,
                             port=server_dict[server]['port'])
            cur = db.cursor()
            for prod in prod_list:
                cur.execute("USE " + prod)
                cur.execute(query)
                table_count = cur.rowcount

                if table_count < 7:
                    run = int(prod[4:10])
                    shells_dict[server][prod] = table_count

        except Mdb.Error, e:
            try:
                print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
            except IndexError:
                print "MySQL Error: %s" % str(e)

    return shells_dict


def verify_summary_production():

    server_dict = servers.server_dict
    empty_dict = get_empties_dict('R004')

    for server, run_dict in empty_dict.iteritems():
        print server, ":", len(run_dict), "runs without Hit tables"

        try:
            db = Mdb.connect(read_default_file='../.my.cnf',
                             read_default_group='guest', host=server,
                             port=server_dict[server]['port'])
            cur = db.cursor()

            for prod, run in run_dict.iteritems():
                    cur.execute("SELECT * FROM summary.production WHERE run=" + str(run) )

                    count = cur.rowcount

                    if count == 0:
                        # print prod, "does not have an entry in summary.production on", server
                        print str(run) + ","
                    elif count > 1:
                        print prod, "has", count, "entries in summary.production on", server
                    # print run
            if db:
                db.close()

        except Mdb.Error, e:
            try:
                print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
            except IndexError:
                print "MySQL Error: %s" % str(e)


def drop_shells():

    shells_dict = get_shells_dict('R004')
    server_dict = servers.server_dict

    for server, run_dict in shells_dict.iteritems():
        try:
            db = Mdb.connect(read_default_file='../.my.cnf',
                             read_default_group='production', host=server,
                             port=server_dict[server]['port'])
            cur = db.cursor()

            for prod, table_count in run_dict.iteritems():
                query = "DROP DATABASE " + prod
                question = prod + " has only " + str(table_count) + " tables. Execute Drop?: '" + query + "'"
                answer = userinput.query_yes_no(question, default="no")
                if answer == True:
                    cur.execute(query)

        except Mdb.Error, e:
            try:
                print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
            except IndexError:
                print "MySQL Error: %s" % str(e)


def main():
    empties_dict = get_empties_dict('R004')
    for server, prod_dict in empties_dict.iteritems():
        print server
        for prod, run in prod_dict.iteritems():
            print run

if __name__ == "__main__":
    main()
