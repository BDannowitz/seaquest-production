#!/usr/bin/python

import MySQLdb as Mdb
import servers

roadset_dict = {'49': {'lower': 8184, 'upper': 8896},
                '56': {'lower': 8897, 'upper': 8908},
                '57': {'lower': 8910, 'upper': 10420},
                '59': {'lower': 10421, 'upper': 10912},
                '61': {'lower': 10914, 'upper': 11028},
                '62': {'lower': 11272, 'upper': 12522}
}

revision_list = ['R001', 'R003', 'R004', 'R005']


def get_productions(revision='R004', server=None, roadset=None):

    prod_dict = {}
    server_dict = servers.server_dict

    for server_entry in server_dict:
        if (server is None) or (server_entry == server):

            prod_dict[server_entry] = []

            try:
                db = Mdb.connect(read_default_file='../.my.cnf',
                                 read_default_group='guest', host=server_entry,
                                 port=server_dict[server_entry]['port'])

                if not db:
                    print "No connection!"
                cur = db.cursor()

                for revision_entry in revision_list:
                    if (revision is None) or (revision_entry == revision):
                        query = "SHOW DATABASES LIKE 'run\_______\_" + revision_entry + "'"
                        cur.execute(query)
                        rows = cur.fetchall()

                        for row in rows:
                            run = int(row[0][4:10])
                            for roadset_entry in roadset_dict:
                                if ((roadset is None) and (row[0] not in prod_dict[server_entry])) or \
                                   ((roadset == roadset_entry) and \
                                   (roadset_dict[roadset_entry]['lower'] <= run <= roadset_dict[roadset_entry]['upper'])):
                                    prod_dict[server_entry].append(row[0])

                if db:
                    db.close()

            except Mdb.Error, e:
                try:
                    print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
                except IndexError:
                    print "MySQL Error: %s" % str(e)

    return prod_dict


def MyProductions():
    get_productions()

if __name__ == "__main__":
    MyProductions()
