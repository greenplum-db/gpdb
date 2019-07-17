#!/usr/bin/env python

import threading
import time
from gppylib.db import dbconn

class TestDML(threading.Thread):

    @staticmethod
    def create(dbname, dmltype):
        if dmltype == 'insert':
            return TestInsert(dbname, dmltype)
        elif dmltype == 'update':
            return TestUpdate(dbname, dmltype)
        elif dmltype == 'delete':
            return TestDelete(dbname, dmltype)
        else:
            raise Exception("unknown dml type: {}" % (dmltype))

    def __init__(self, dbname, dmltype):
        self.dbname = dbname
        self.dmltype = dmltype
        self.tablename = 'gpexpand_test_{}'.format(dmltype)
        self.running = True
        self.retval = True
        self.retmsg = 'OK'
        super(TestDML, self).__init__()

        self.prepare()

    def run(self):
        conn = dbconn.connect(dbconn.DbURL(dbname=self.dbname), unsetSearchPath=False)

        self.loop(conn)
        self.verify(conn)

        conn.commit()

    def prepare(self):
        sql = '''
            DROP TABLE IF EXISTS {tablename};

            CREATE TABLE {tablename} (
                c1 INT,
                c2 INT
            ) DISTRIBUTED BY (c1);
        '''.format(tablename=self.tablename)

        conn = dbconn.connect(dbconn.DbURL(dbname=self.dbname), unsetSearchPath=False)
        dbconn.execSQL(conn, sql)

        self.prepare_extra(conn)

        conn.commit()

    def prepare_extra(self, conn):
        pass

    def loop(self, conn):
        # repeatedly execute the DML command
        self.counter = 0
        timestamp = time.time()
        starttime = timestamp
        self.maxtime = 0;
        while self.running or self.counter == 0:
            sql = self.loop_step()
            dbconn.execSQL(conn, sql)

            self.counter = self.counter + 1

            ts = time.time()
            self.maxtime = max(self.maxtime, ts - timestamp)
            timestamp = ts
        endtime = time.time()
        self.avgtime = (endtime - starttime) / self.counter

    def loop_step(self):
        return 'select 1'

    def reverify(self, conn):
        self.retval = True
        self.retmsg = 'OK'

        self.verify(conn)

        return self.retval, self.retmsg

    def verify(self):
        pass

    def stop(self):
        self.running = False
        self.join()

        return self.retval, self.retmsg

    def report_incorrect_result(self):
        self.retval = False
        self.retmsg = '{dml} result is incorrect'.format(dml=self.dmltype)

class TestInsert(TestDML):
    def loop_step(self):
        return '''
            insert into {tablename} values({counter}, {counter});
        '''.format(tablename=self.tablename, counter=self.counter)

    def verify(self, conn):
        sql = '''
            select c1 from {tablename} order by c1;
        '''.format(tablename=self.tablename, counter=self.counter)
        results = dbconn.execSQL(conn, sql).fetchall()

        for i in range(0, self.counter):
            if i != int(results[i][0]):
                self.report_incorrect_result()
                return

class TestUpdate(TestDML):
    datasize = 1000

    def prepare_extra(self, conn):
        sql = '''
            insert into {tablename} select i,i
            from generate_series(0,{datasize}-1) i;
        '''.format(tablename=self.tablename, datasize=self.datasize)
        dbconn.execSQL(conn, sql)

    def loop_step(self):
        return '''
            update {tablename} set c2=c1+{counter};
        '''.format(tablename=self.tablename, counter=self.counter)

    def verify(self, conn):
        sql = '''
            select c2 from {tablename} order by c1;
        '''.format(tablename=self.tablename, counter=self.counter)
        results = dbconn.execSQL(conn, sql).fetchall()

        for i in range(0, self.datasize):
            if i + self.counter - 1 != int(results[i][0]):
                self.report_incorrect_result()
                return

class TestDelete(TestDML):
    datasize = 100000

    def prepare_extra(self, conn):
        sql = '''
            insert into {tablename} select i,i
            from generate_series(0,{datasize}-1) i;
        '''.format(tablename=self.tablename, datasize=self.datasize)
        dbconn.execSQL(conn, sql)

    def loop_step(self):
        return '''
            delete from {tablename} where c1={counter};
        '''.format(tablename=self.tablename, counter=self.counter)

    def verify(self, conn):
        sql = '''
            select c1 from {tablename} order by c1;
        '''.format(tablename=self.tablename, counter=self.counter)
        results = dbconn.execSQL(conn, sql).fetchall()

        for i in range(self.counter, self.datasize):
            if i != int(results[i - self.counter][0]):
                self.report_incorrect_result()
                return

# for test only
if __name__ == '__main__':
    dbname = 'gpadmin'
    jobs = []

    for dml in ['insert', 'update', 'delete']:
        job = TestDML.create(dbname, dml)
        job.start()
        jobs.append((dml, job))

    time.sleep(10)

    for dml, job in jobs:
        code, message = job.stop()
        print '{dml}: {code}, message={message}, avgtime={avgtime}, maxtime={maxtime}'.format(
            dml=dml, code=code, message=message,
            avgtime=job.avgtime, maxtime=job.maxtime
        )
