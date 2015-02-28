#!/usr/bin/python

import sys
import MySQLdb as Mdb
import subprocess
import time
import datetime

sys.path.append('../modules')
import productions
import servers

uploader = "/seaquest/users/liuk/reco59_upload/sqlResWriter"
old_version = "_r1_0"
ksource = "/seaquest/users/liuk/reco59_upload/rootfiles"

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


def ktrack_upload(track_file,vertex_file,server,prod):

    server_dict = servers.server_dict
    print uploader, track_file, vertex_file, prod, server, server_dict[server]['port']
    subprocess.call([uploader, track_file, vertex_file, prod, server, str(server_dict[server]['port'])])
    time.sleep(10)


def rename_ktrack_tables(production, server, port):
    """
    Moves over existing kTrack data.
    If something's already been moved over, do nothing
    """


    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=port)
        cur = db.cursor()

        cur.execute("USE " + production)

        for table_name in ['kTrack', 'kDimuon', 'kTrackHit', 'kInfo']:
            cur.execute("SHOW TABLES LIKE '" + table_name + old_version + "'")
            backup_exists = 1 if cur.rowcount > 0 else 0
            cur.execute("SHOW TABLES LIKE '" + table_name + "'")
            table_exists = 1 if cur.rowcount > 0 else 0

            if table_exists and not backup_exists:
                cur.execute("RENAME TABLE " + table_name + " TO " + table_name + old_version)
            exists = cur.rowcount

        db.close()

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)


def load_all_tracking(list_file, rundonefile):

    run_dict = {}
    server_dict = servers.server_dict
    run_list = []

    # Read file into list
    f = open(list_file, 'r')
    run_list = f.read().splitlines()
    f.close()

    found_run_dict = find_server(run_list)

    for server, found_runs in found_run_dict.iteritems():
        if server != "NoMatch":
            for run in found_runs:
                vertex_file = ksource + "/vertex_" + run[4:10] + "_r1.2.0.root"
                track_file = ksource + "/track_" + run[4:10] + "_r1.2.0.root"
                run_dict[run] = {'server' : server, 'production' : run, 'vfile' : vertex_file, 'tfile' : track_file}

    for run, details in run_dict.iteritems():
        question = "Upload run " + run + ", uploading files " + details['tfile'] + " and " + details['vfile'] + " to " + details['server'] + ":" + details['production'] + "?"
        answer = True
        if answer == True:        
            rename_ktrack_tables(run,details['server'],server_dict[details['server']]['port']) 
            ktrack_upload(details['tfile'],details['vfile'],details['server'],details['production'])
            with open(rundonefile, 'a') as fout:
                ts = time.time()
                st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
                fout.write(st + " - " + run[4:10] + "\n")
                update_decoderinfo(details['production'], details['server'], server_dict[details['server']]['port'])


def update_decoderinfo(production, server, port):
    """
    After the successful upload of ktrackerInfo, update the summary.production
    table to indicate that the schema has ktracker data
    """
    try:
        db = Mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         host=server, port=port)
        cur = db.cursor()

        cur.execute("UPDATE summary.production SET ktracked=1 WHERE production='" + production + "'")

        db.close()

    except Mdb.Error, e:
        try:
            print "MySQL Error [%d]: %s" % (e.args[0], e.args[1])
        except IndexError:
            print "MySQL Error: %s" % str(e)



def main(filename):
    """
    Take a filename containing a list of 6-digit run numbers
    Find existing productions on our MySQL servers
    Call Kun's tracking uploader sending the tracking information to the appropriate server and schema
    Output uploaded run numbers to 'done' file
    """
    run_list_file = filename 
    run_list_number = run_list_file[5:7]
    run_done_file = "done-" + run_list_number + ".txt"
    load_all_tracking(run_list_file, run_done_file) 
    

if __name__ == "__main__":
    main(sys.argv[1])

