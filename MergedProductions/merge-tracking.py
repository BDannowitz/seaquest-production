# !/usr/bin/python

import sys
import MySQLdb as Mdb
import subprocess
import os
import argparse

sys.path.append('../modules')
import productions
import servers
from userinput import query_yes_no


def table_exists(server, schema, table):
    try:

        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         db=schema, host=server,
                         port=servers.server_dict[server]['port'])
        cur = db.cursor()
        cur.execute("SHOW TABLES LIKE '" + table + "'")
        exists = cur.rowcount

        if exists == 0:
            return False
        else:
            return True

    except Mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        sys.exit(1)


def merge_per_server(server, tbl_list, schema_suffix, j_flag, k_flag):
    roadset_dict = productions.roadset_dict

    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=servers.server_dict[server]['port'])
        cur = db.cursor()

        for road_set, run_range in roadset_dict.iteritems():

            schema = 'merged_roadset' + road_set + '_temp_' + schema_suffix
            query_string = 'DROP DATABASE IF EXISTS ' + schema
            cur.execute(query_string)

            query_string = 'CREATE DATABASE IF NOT EXISTS ' + schema
            cur.execute(query_string)
            query_string = 'USE ' + schema
            cur.execute(query_string)

            query_string = """SELECT DISTINCT production FROM summary.production
                              WHERE run BETWEEN %s AND %s
                                  AND decoded > 0""" % (run_range['lower'], run_range['upper'])
            if j_flag and k_flag:
                query_string = query_string + ' AND (jtracked = 1 OR ktracked = 1)'
            elif j_flag and not k_flag:
                query_string = query_string + ' AND jtracked = 1'
            elif not j_flag and k_flag:
                query_string = query_string + ' AND ktracked = 1'

            cur.execute(query_string)
            rows = cur.fetchall()

            first = 1

            print '>>> Merging the tables:', tbl_list, 'for road set', road_set, 'on server:', server
            for row in rows:
                print row[0]
                # For k-tracker merging, use these lines
                cur.execute("SELECT * FROM %s.kInfo WHERE infoValue='ROOT decoder/r1.2.0'" % row[0])
                exists = cur.rowcount
                if exists > 0:
                    for table in tbl_list:
                        if first:
                            query_string = "CREATE TABLE IF NOT EXISTS %s LIKE %s.%s" % (
                                table, row[0], table)
                            cur.execute(query_string)
                            cur.execute('ALTER TABLE %s DISABLE KEYS' % table)

                        if table == "Run":
                            query_string = "INSERT INTO %s SELECT * FROM %s.%s WHERE runID!=0" % (
                                table, row[0], table)
                        else:
                            query_string = "INSERT INTO %s SELECT * FROM %s.%s WHERE spillID!=0" % (
                                table, row[0], table)

                        print query_string
                        cur.execute(query_string)
                    first = 0

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)


def dump_merged(server, tbl_list, schema_suffix, dump_file_prefix):
    db_user = 'production'
    db_pass = '***REMOVED***'

    for road_set in productions.roadset_dict:

        schema = 'merged_roadset' + road_set + '_temp_' + schema_suffix
        dump_file_name = dump_file_prefix + road_set + '.sql'
        cmdL1 = ["mysqldump", "--host=" + server, "--port=" + str(servers.server_dict[server]['port']),
                 "--user=" + db_user, "--password=" + db_pass,
                 "--skip-add-drop-table", "--no-create-info",
                 schema]

        for table in tbl_list:
            cmdL1.append(table)
        print '>>> Dumping the tables:', tbl_list, 'to a dump file', dump_file_name

        # f = open(dump_file_name, 'a')
        # p1 = subprocess.Popen(cmdL1, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            with open(dump_file_name, 'a') as f:
                subprocess.check_call(cmdL1, stdout=f, stderr=subprocess.STDOUT)
        # dump_output = p1.communicate()[0]

        # f.write(dump_output)
        # f.close()
        except subprocess.CalledProcessError, e:
            print "mysqldump stdout output:\n", e.output


def make_dest_table(server, schema, tables):
    db_user = 'production'
    db_pass = '***REMOVED***'

    for table in tables:
        if table_exists(server, schema, table):
            try:
                db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                                 host=server, port=servers.server_dict[server]['port'], db=schema)
                cur = db.cursor()
                cur.execute('RENAME TABLE %s TO %s_old' % (table, table))
                cur.execute('CREATE TABLE %s LIKE %s_old' % (table, table))

            except Mdb.Error, e:
                try:
                    print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
                except IndexError:
                    print "MySQL Error: %s" % str(e)
        # Table doesn't already exist, load up the table_def file
        else:
            fd = open(table + '.sql', 'r')
            subprocess.Popen(['mysql', '-u', db_user, '-p' + db_pass, '-P', str(servers.server_dict[server]['port']),
                              '-h', server, schema], stdin=fd).wait()


def drop_temp_merged(server, schema_suffix):
    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=servers.server_dict[server]['port'])
        cur = db.cursor()

        for road_set, run_range in productions.roadset_dict.iteritems():
            schema = 'merged_roadset' + road_set + '_temp_' + schema_suffix
            print '>>> Dropping schema', schema, 'on', server
            query_string = 'DROP DATABASE IF EXISTS ' + schema
            cur.execute(query_string)

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)


def upload_dump(server, schema, dump_file):
    db_user = 'production'
    db_pass = '***REMOVED***'

    fd = open(dump_file, 'r')
    subprocess.Popen(['mysql', '-u', db_user, '-p' + db_pass, '-P', str(servers.server_dict[server]['port']),
                      '-h', server, schema], stdin=fd).wait()


def main():
    server_dict = servers.server_dict
    roadset_dict = productions.roadset_dict

    pid = os.getpid()

    # Get command line arguments
    parser = argparse.ArgumentParser(description='Merge table values into a merged production.')
    parser.add_argument('--tables', metavar='tbl_name', dest='tbl_list',
                        nargs='+', help='List of tables to merge')

    test = '--tables kTrack kDimuon'.split()
    # args = parser.parse_args(sys.argv)
    args = parser.parse_args(test)

    # Use pid for names of our temp schemas and mysqldump files
    dump_file_prefix = 'merge_dump_' + str(pid)
    schema_suffix = str(pid)

    for server in server_dict:
        # Aggregate all of the table data from a single server together
        merge_per_server(server, args.tbl_list, schema_suffix, j_flag=0, k_flag=1)

        if not query_yes_no("Continue?", default='no'):
            sys.exit(1)

        # Dump that merged table from each server to a single local dump file for each road set
        dump_merged(server, args.tbl_list, schema_suffix, dump_file_prefix)

        if not query_yes_no("Continue?", default='no'):
            sys.exit(1)

        # Clean up by removing the merged schemas we've created
        drop_temp_merged(server, schema_suffix)

    # Now we should have a single dump file for each roadset that contains
    # what we need to load to each server
    for server in server_dict:
        for roadset in roadset_dict:

            if not query_yes_no("Continue?", default='no'):
                sys.exit(1)

            dump_file = dump_file_prefix + roadset + '.sql'
            dest_schema = 'merged_roadset' + roadset + '_R004'

            if not query_yes_no("Continue?", default='no'):
                sys.exit(1)
            # Move the existing table over, create a new one...
            make_dest_table(server, dest_schema, args.tbl_list)

            if not query_yes_no("Continue?", default='no'):
                sys.exit(1)
            # And load the data to it
            upload_dump(server, dest_schema, args.tbl_list, dump_file)

    for roadset in roadset_dict:
        # Get rid of mysqldump files
        dump_file = dump_file_prefix + roadset + '.sql'
        os.remove(dump_file)

        # All done! Go take a break


if __name__ == '__main__':
    main()