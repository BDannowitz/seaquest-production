#!/usr/bin/python

import MySQLdb as mdb
import sys
import re

sys.path.append('../modules')
from servers import server_dict
from productions import schema_exists, table_exists

spill_cut = {}
spill_cut['57'] = {'targetPos': (1, 7),
                   'TSGo': (1e3, 8e3),
                   'Accepted': (1e3, 8e3),
                   'AfterInh': (1e3, 3e4),
                   'Acc/After': (0.2, 0.9),
                   'NM3ION': (2e12, 1e13),
                   'G2SEM': (2e12, 1e13),
                   'QIESum': (4e10, 1e12),
                   'Inhibit': (4e9, 1e11),
                   'Busy': (4e9, 1e11),
                   'DutyFactor': (15, 60),
                   'BadSpillRanges': ()}
spill_cut['59'] = spill_cut['57']
spill_cut['62'] = {'targetPos': (1, 7),
                   'TSGo': (1e2, 6e3),
                   'Accepted': (1e2, 6e3),
                   'AfterInh': (1e2, 1e4),
                   'Acc/After': (0.2, 1.05),
                   'NM3ION': (2e12, 1e13),
                   'G2SEM': (2e12, 1e13),
                   'QIESum': (4e10, 1e12),
                   'Inhibit': (4e9, 2e11),
                   'Busy': (4e9, 1e11),
                   'DutyFactor': (10, 60),
                   'BadSpillRanges': ()}

                    # (416709, 423255), (482574, 484924)

# 0     Beam: Duty Factor BETWEEN 15 AND 60
# 1     Beam: G2SEM BETWEEN 2e12 AND 1e13
# 2     Beam: QIEsum BETWEEN 4e10 AND 1e12
# 3     Beam:
# 4     Beam:
# 5     Beam:
# 6     Beam:
# 7     Beam:
# 8     Target: TargetPos (Spill) = TargetPos (Target)
# 9     Target: TargetPos BETWEEN 1 AND 7
# 10    Target:
# 11    Target:
# 12    Target:
# 13    Target:
# 14    Target:
# 15    Target:
# 16    DAQ / Trigger: Inhibit value
# 17    DAQ / Trigger: Busy value
# 18    DAQ / Trigger: AcceptedFPGA1 value
# 19    DAQ / Trigger: AfterInhFPGA1 value
# 20    DAQ / Trigger: Accepted/AfterInh value
# 21    DAQ / Trigger: TSGo value
# 22    DAQ / Trigger:
# 23    DAQ / Trigger:
# 24    Decoding: Duplicate values present
# 25    Decoding: Problematic (missing) values
# 26    Decoding: 'Bad spill' range
# 27    Decoding:
# 28    Decoding:
# 29    Decoding:
# 30    Decoding:
# 31    Decoding:

def set_spill_quality(server, schema, roadset):

    bad_spill_set = set()
    spill_set = set()
    reset_spill_dataquality_bit(server, schema)

    if roadset not in spill_cut:
        print 'Unknown roadset' + roadset + ". Using roadset 57 spill criteria."
        roadset = '57'

    try:
        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='guest',
                         db=schema,
                         host=server,
                         port=server_dict[server]['port'])
        cur = db.cursor()

        query = """
                SELECT DISTINCT s.spillID
                FROM Spill s INNER JOIN Target t USING(spillID)
                WHERE s.targetPos != t.value
                AND t.name='TARGPOS_CONTROL'
                """
        cur.execute(query)
        print str(cur.rowcount) + ' Spills where Spill.targetPos != Target.TARGPOS_CONTROL'
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 8)

        query = """
                SELECT DISTINCT s.spillID
                FROM Spill s
                WHERE s.targetPos NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['targetPos'][0],
                             spill_cut[roadset]['targetPos'][1]))
        print (str(cur.rowcount) + ' spills where spill.targetpos not between ' +
               str(spill_cut[roadset]['targetPos'][0]) + ' and ' + str(spill_cut[roadset]['targetPos'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 9)

        query = """
                SELECT DISTINCT spillID
                FROM Scaler
                WHERE scalerName = 'TSGo'
                AND spillType='EOS'
                AND value NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['TSGo'][0],
                             spill_cut[roadset]['TSGo'][1]))

        print (str(cur.rowcount) + ' spills where Scaler\'s TSGo not between ' +
               str(spill_cut[roadset]['TSGo'][0]) + ' and ' + str(spill_cut[roadset]['TSGo'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 21)

        query = """
                SELECT DISTINCT spillID
                FROM Scaler
                WHERE spillType = 'EOS' AND
                      scalerName = 'AcceptedMatrix1'
                AND value NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['Accepted'][0],
                             spill_cut[roadset]['Accepted'][1]))
        print (str(cur.rowcount) + ' spills where Scaler\'s AcceptedMatrix1 not between ' +
               str(spill_cut[roadset]['Accepted'][0]) + ' and ' +
               str(spill_cut[roadset]['Accepted'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 18)

        query = """
                SELECT DISTINCT spillID
                FROM Scaler
                WHERE spillType = 'EOS' AND
                      scalerName = 'AfterInhMatrix1'
                AND value NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['AfterInh'][0],
                             spill_cut[roadset]['AfterInh'][1]))
        print (str(cur.rowcount) + ' spills where Scaler\'s AfterInhMatrix1 not between ' +
               str(spill_cut[roadset]['AfterInh'][0]) + ' and ' + str(spill_cut[roadset]['AfterInh'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 19)

        query = """
                SELECT DISTINCT t1.spillID
                FROM (
                    SELECT spillID, value AS 'AcceptedMatrix1'
                    FROM Scaler
                    WHERE spillType = 'EOS' AND
                          scalerName = 'AcceptedMatrix1' ) t1
                INNER JOIN
                    (
                    SELECT spillID, value AS 'AfterInhMatrix1'
                    FROM Scaler
                    WHERE spillType = 'EOS' AND
                          scalerName = 'AfterInhMatrix1' ) t2
                USING(spillID)
                WHERE IF(AfterInhMatrix1>0,AcceptedMatrix1/AfterInhMatrix1,0) NOT BETWEEN %f AND %f
                """
        cur.execute(query % (spill_cut[roadset]['Acc/After'][0],
                             spill_cut[roadset]['Acc/After'][1]))
        print (str(cur.rowcount) + ' spills where Scaler\'s Accepted / AfterInhibit not between ' +
               str(spill_cut[roadset]['Acc/After'][0]) + ' and ' + str(spill_cut[roadset]['Acc/After'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 20)

        query = """
                SELECT DISTINCT spillID
                FROM Beam
                WHERE name = 'F:NM3ION' AND
                    value NOT BETWEEN %d AND %d
                """
        # cur.execute(query % (spill_cut[roadset]['NM3ION'][0],
        #                      spill_cut[roadset]['NM3ION'][1]))
        # print (str(cur.rowcount) + ' spills where Beam\'s NM3ION not between ' +
        #        str(spill_cut[roadset]['NM3ION'][0]) + ' and ' + str(spill_cut[roadset]['NM3ION'][1]))
        # rows = cur.fetchall()
        # for row in rows:
        #    bad_spill_set.add(row[0])

        query = """
                SELECT DISTINCT spillID
                FROM Beam
                WHERE name = 'S:G2SEM' AND
                        value NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['G2SEM'][0],
                             spill_cut[roadset]['G2SEM'][1]))
        print (str(cur.rowcount) + ' spills where Beam\'s G2SEM not between ' +
               str(spill_cut[roadset]['G2SEM'][0]) + ' and ' + str(spill_cut[roadset]['G2SEM'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 1)

        query = """
                SELECT DISTINCT spillID
                FROM BeamDAQ
                WHERE QIESum NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['QIESum'][0],
                             spill_cut[roadset]['QIESum'][1]))
        print (str(cur.rowcount) + ' spills where BeamDAQ\'s QIESum not between ' +
               str(spill_cut[roadset]['QIESum'][0]) + ' and ' + str(spill_cut[roadset]['QIESum'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 2)

        query = """
                SELECT DISTINCT spillID
                FROM BeamDAQ
                WHERE inhibit_block_sum NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['Inhibit'][0],
                             spill_cut[roadset]['Inhibit'][1]))
        print (str(cur.rowcount) + ' spills where BeamDAQ\'s Inhibit not between ' +
               str(spill_cut[roadset]['Inhibit'][0]) + ' and ' + str(spill_cut[roadset]['Inhibit'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 16)

        query = """
                SELECT DISTINCT spillID
                FROM BeamDAQ
                WHERE trigger_sum_no_inhibit NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['Busy'][0],
                             spill_cut[roadset]['Busy'][1]))
        print (str(cur.rowcount) + ' spills where BeamDAQ\'s Busy not between ' +
               str(spill_cut[roadset]['Busy'][0]) + ' and ' + str(spill_cut[roadset]['Busy'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 17)

        query = """
                SELECT DISTINCT spillID
                FROM BeamDAQ
                WHERE dutyfactor53MHz NOT BETWEEN %d AND %d
                """
        cur.execute(query % (spill_cut[roadset]['DutyFactor'][0],
                             spill_cut[roadset]['DutyFactor'][1]))
        print (str(cur.rowcount) + ' spills where BeamDAQ\'s Duty Factor not between ' +
               str(spill_cut[roadset]['DutyFactor'][0]) + ' and ' + str(spill_cut[roadset]['DutyFactor'][1]))
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 0)

        if len(spill_cut[roadset]['BadSpillRanges']) > 0:
            query = """
                    SELECT DISTINCT spillID
                    FROM Spill
                    WHERE
                    """

            for spill_range in spill_cut[roadset]['BadSpillRanges']:
                query += " spillID BETWEEN " + str(spill_range[0]) + " AND " + str(spill_range[1]) + " OR "
            query = query[:-4]
            cur.execute(query)
            print str(cur.rowcount), 'Spills in ranges', spill_cut[roadset]['BadSpillRanges']
            rows = cur.fetchall()
            spill_set.clear()
            for row in rows:
                bad_spill_set.add(row[0])
                spill_set.add(row[0])
            set_spill_dataquality_bit(server, schema, spill_set, 26)

        query = """
                        SELECT DISTINCT spillID FROM Target
                        WHERE name='TARGPOS_CONTROL'
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                    UNION
                        SELECT DISTINCT spillID FROM Spill
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                    UNION
                        SELECT DISTINCT spillID
                        FROM Scaler
                        WHERE spillType='EOS' AND scalerName='TSGo'
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                    UNION
                        SELECT DISTINCT spillID
                        FROM Scaler
                        WHERE spillType='EOS' AND scalerName='AfterInhMatrix1'
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                    UNION
                        SELECT DISTINCT spillID
                        FROM Scaler
                        WHERE spillType='EOS' AND scalerName='AcceptedMatrix1'
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                    UNION
                        SELECT DISTINCT spillID
                        FROM BeamDAQ
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                    UNION
                        SELECT DISTINCT spillID
                        FROM Beam
                        WHERE name='F:NM3ION'
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                    UNION
                        SELECT DISTINCT spillID
                        FROM Beam
                        WHERE name='S:G2SEM'
                        GROUP BY spillID
                        HAVING COUNT(*) > 1
                """
        cur.execute(query)
        print str(cur.rowcount) + ' Spills with duplicate values'
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 24)

        query = """
                    SELECT DISTINCT s.spillID
                    FROM Spill s LEFT JOIN
                        ( SELECT DISTINCT spillID FROM Target t
                          WHERE name='TARGPOS_CONTROL'
                        ) t
                    USING(spillID)
                    WHERE t.spillID IS NULL
                UNION
                    SELECT s.spillID
                    FROM Spill s LEFT JOIN
                        ( SELECT DISTINCT spillID
                          FROM Scaler
                          WHERE spillType='EOS' AND scalerName='TSGo'
                        ) sc USING(spillID)
                    WHERE sc.spillID IS NULL
                UNION
                    SELECT s.spillID
                    FROM Spill s LEFT JOIN
                        ( SELECT DISTINCT spillID
                          FROM Scaler
                          WHERE spillType='EOS' AND scalerName='AfterInhMatrix1'
                        ) sc USING(spillID)
                    WHERE sc.spillID IS NULL
                UNION
                    SELECT s.spillID
                    FROM Spill s LEFT JOIN
                        ( SELECT DISTINCT spillID
                          FROM Scaler
                          WHERE spillType='EOS' AND scalerName='AcceptedMatrix1'
                        ) sc USING(spillID)
                    WHERE sc.spillID IS NULL
                UNION
                    SELECT s.spillID
                    FROM Spill s LEFT JOIN
                        ( SELECT DISTINCT spillID
                          FROM BeamDAQ
                        ) b USING(spillID)
                    WHERE b.spillID IS NULL
                UNION
                    SELECT DISTINCT s.spillID
                    FROM Spill s LEFT JOIN
                        ( SELECT DISTINCT spillID
                          FROM Beam
                          WHERE name='S:G2SEM' ) b
                    USING (spillID)
                    WHERE b.spillID IS NULL
                UNION
                    SELECT DISTINCT t.spillID
                    FROM Spill s RIGHT JOIN
                        ( SELECT DISTINCT spillID FROM Target t
                          WHERE name='TARGPOS_CONTROL'
                        ) t
                    USING(spillID)
                    WHERE s.spillID IS NULL
                UNION
                    SELECT sc.spillID
                    FROM Spill s RIGHT JOIN
                        ( SELECT DISTINCT spillID
                          FROM Scaler
                          WHERE spillType='EOS'
                                AND (scalerName='TSGo' OR
                                     scalerName='AfterInhMatrix1' OR
                                     scalerName='AcceptedMatrix1')
                        ) sc USING(spillID)
                    WHERE s.spillID IS NULL
                UNION
                    SELECT b.spillID
                    FROM Spill s RIGHT JOIN
                        ( SELECT DISTINCT spillID
                          FROM BeamDAQ
                        ) b USING(spillID)
                    WHERE s.spillID IS NULL
                UNION
                    SELECT DISTINCT b.spillID
                    FROM Spill s RIGHT JOIN
                        ( SELECT DISTINCT spillID
                          FROM Beam
                          WHERE name='S:G2SEM' ) b
                    USING (spillID)
                    WHERE s.spillID IS NULL
                """
        cur.execute(query)
        print str(cur.rowcount) + ' Spills with missing value(s)'
        rows = cur.fetchall()
        spill_set.clear()
        for row in rows:
            bad_spill_set.add(row[0])
            spill_set.add(row[0])
        set_spill_dataquality_bit(server, schema, spill_set, 25)
        if db:
            db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return 1

    return bad_spill_set


def reset_spill_dataquality_bit(server, schema):

    query1 = 'UPDATE %s.Spill SET dataQuality=0'
    query2 = 'ALTER TABLE %s.Spill CHANGE dataQuality dataQuality BIGINT DEFAULT 0'

    try:
        db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                         db=schema,
                         host=server,
                         port=server_dict[server]['port'])
        cur = db.cursor()

        cur.execute(query1 % schema)
        cur.execute(query2 % schema)

        if db:
            db.close()

    except mdb.Error, e:

        print "Error %d: %s" % (e.args[0], e.args[1])
        return -1

    return 0


def set_spill_dataquality_bit(server, schema, spill_set, nth_bit):
    # Set the nth dataquality bit for a set of spills.

    query = """UPDATE %s.Spill
               SET dataQuality = dataQuality | %d
               WHERE spillID IN("""

    if len(spill_set) > 0:
        for spill in spill_set:
            query = query + str(spill) + ', '
        query = query[:-2] + ')'
    else:
        print 'Empty spill set.'
        return 0

    for x_server in server_dict:
        try:
            db = mdb.connect(read_default_file='../.my.cnf', read_default_group='production',
                             db=schema,
                             host=x_server,
                             port=server_dict[x_server]['port'])
            cur = db.cursor()

            cur.execute(query % (schema, 2**nth_bit))

            updated_count = cur.rowcount

            if db:
                db.close()

        except mdb.Error, e:

            print "Error %d: %s" % (e.args[0], e.args[1])
            return -1

    return updated_count


def main():
    print 'Hello World!'

if __name__ == '__main__':
    main()
