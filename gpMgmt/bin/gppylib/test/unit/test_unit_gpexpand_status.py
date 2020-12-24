#!/usr/bin/env python3
# pylint: disable=invalid-name, missing-docstring, too-few-public-methods
#
# Copyright (c) 2019-Present VMware, Inc. or its affiliates.
#

import os

from gppylib.test.unit.gp_unittest import GpTestCase, run_tests
from gppylib.commands import gp
from gppylib.db import dbconn

class Context(object):
    filename = os.path.join(gp.get_masterdatadir(), 'gpexpand.status')
    dbname = os.getenv('PGDATABASE', 'postgres')
    dburl = dbconn.DbURL(dbname=dbname)
    conn = dbconn.connect(dburl)
    day = 0

ctx = Context()

def get_gpexpand_status():
    st = gp.get_gpexpand_status()
    st.dbname = ctx.dbname
    return st.get_status()

def insert_status(status):
    ctx.day += 1
    dbconn.execSQL(ctx.conn, '''
        INSERT INTO gpexpand.status VALUES
            ( '{status}', date '2001-01-01' + interval '{day} day');
    '''.format(status=status, day=ctx.day))
    ctx.conn.commit()

def leave_phase1(func):
    def wrapper(*args, **kwargs):
        try:
            os.unlink(ctx.filename)
        except OSError:
            pass
        return func(*args, **kwargs)
    return wrapper

def leave_phase2(func):
    def wrapper(*args, **kwargs):
        dbconn.execSQL(ctx.conn, '''
            DROP SCHEMA IF EXISTS gpexpand CASCADE;
        ''')
        ctx.conn.commit()

        return func(*args, **kwargs)
    return wrapper

def drop_table(name):
    def decorator(func):
        def wrapper(*args, **kwargs):
            dbconn.execSQL(ctx.conn, '''
                DROP TABLE IF EXISTS {name};
            '''.format(name=name))
            ctx.conn.commit()

            return func(*args, **kwargs)
        return wrapper
    return decorator

def start_redistribution(func):
    def wrapper(*args, **kwargs):
        insert_status('EXPANSION STARTED')
        return func(*args, **kwargs)
    return wrapper

def stop_redistribution(func):
    def wrapper(*args, **kwargs):
        insert_status('EXPANSION STOPPED')
        return func(*args, **kwargs)
    return wrapper

def expanding_table(name):
    def decorator(func):
        def wrapper(*args, **kwargs):
            dbconn.execSQL(ctx.conn, '''
                UPDATE gpexpand.status_detail SET STATUS='IN PROGRESS'
                 WHERE fq_name='{name}';
            '''.format(name=name))
            ctx.conn.commit()

            return func(*args, **kwargs)
        return wrapper
    return decorator

def expanded_table(name):
    def decorator(func):
        def wrapper(*args, **kwargs):
            dbconn.execSQL(ctx.conn, '''
                UPDATE gpexpand.status_detail SET STATUS='COMPLETED'
                 WHERE fq_name='{name}';
            '''.format(name=name))
            ctx.conn.commit()

            return func(*args, **kwargs)
        return wrapper
    return decorator

class GpExpandUtils(GpTestCase):

    def setUp(self):
        ctx.day = 1
        dbconn.execSQL(ctx.conn, '''
            DROP SCHEMA IF EXISTS gpexpand CASCADE;
            CREATE SCHEMA gpexpand;
            CREATE TABLE gpexpand.status (status text, updated timestamp);
            CREATE TABLE gpexpand.status_detail (
                dbname text,
                fq_name text,
                schema_oid oid,
                table_oid oid,
                distribution_policy smallint[],
                distribution_policy_names text,
                distribution_policy_coloids text,
                distribution_policy_type text,
                root_partition_name text,
                storage_options text,
                rank int,
                status text,
                expansion_started timestamp,
                expansion_finished timestamp,
                source_bytes numeric
            );
            INSERT INTO gpexpand.status VALUES
                ( 'SETUP',      '2001-01-01' ),
                ( 'SETUP DONE', '2001-01-02' );
            INSERT INTO gpexpand.status_detail (dbname, fq_name, rank, status) VALUES
                ('fake_db', 'public.t1', 2, 'NOT STARTED'),
                ('fake_db', 'public.t2', 2, 'NOT STARTED');
        '''.format(dbname=ctx.dbname))
        ctx.conn.commit()

        with open(ctx.filename, 'w') as f:
            f.write('''UNINITIALIZED:None
EXPANSION_PREPARE_STARTED:<filename>
BUILD_SEGMENT_TEMPLATE_STARTED:<filename>
BUILD_SEGMENT_TEMPLATE_DONE:None
BUILD_SEGMENTS_STARTED:<filename>
BUILD_SEGMENTS_DONE:<number>
UPDATE_CATALOG_STARTED:<filename>
UPDATE_CATALOG_DONE:None
SETUP_EXPANSION_SCHEMA_STARTED:None
SETUP_EXPANSION_SCHEMA_DONE:None
PREPARE_EXPANSION_SCHEMA_STARTED:None
PREPARE_EXPANSION_SCHEMA_DONE:None
EXPANSION_PREPARE_DONE:None
''')

    @leave_phase1
    @leave_phase2
    def tearDown(self):
        pass

    @leave_phase1
    @leave_phase2
    def test_when_no_expansion(self):
        st = get_gpexpand_status()

        self.assertEqual(st.phase, 0)
        self.assertEqual(st.status, 'NO EXPANSION DETECTED')
        self.assertEqual(len(st.uncompleted), 0)
        self.assertEqual(len(st.inprogress), 0)
        self.assertEqual(len(st.completed), 0)

        st.get_progress()

        self.assertEqual(st.phase, 0)
        self.assertEqual(st.status, 'NO EXPANSION DETECTED')
        self.assertEqual(len(st.uncompleted), 0)
        self.assertEqual(len(st.inprogress), 0)
        self.assertEqual(len(st.completed), 0)

    def test_phase1_with_empty_status(self):
        with open(ctx.filename, 'w'):
            pass

        st = get_gpexpand_status()

        self.assertEqual(st.phase, 1)
        self.assertEqual(st.status, 'UNKNOWN PHASE1 STATUS')

    def test_phase1_with_normal_status(self):
        st = get_gpexpand_status()

        self.assertEqual(st.phase, 1)
        self.assertEqual(st.status, 'EXPANSION_PREPARE_DONE')

    @leave_phase1
    @drop_table('gpexpand.status_detail')
    def test_phase2_when_missing_status_detail(self):
        st = get_gpexpand_status()
        st.get_progress()

        self.assertEqual(st.phase, 2)
        self.assertEqual(st.status, 'SETUP DONE')
        self.assertEqual(len(st.uncompleted), 0)
        self.assertEqual(len(st.inprogress), 0)
        self.assertEqual(len(st.completed), 0)

    @leave_phase1
    def test_phase2_when_setup_done(self):
        st = get_gpexpand_status()
        st.get_progress()

        self.assertEqual(st.phase, 2)
        self.assertEqual(st.status, 'SETUP DONE')
        self.assertEqual(len(st.uncompleted), 2)
        self.assertEqual(len(st.inprogress), 0)
        self.assertEqual(len(st.completed), 0)

    @leave_phase1
    @start_redistribution
    @expanding_table('public.t1')
    def test_phase2_when_expanding_first_table(self):
        st = get_gpexpand_status()
        st.get_progress()

        self.assertEqual(st.phase, 2)
        self.assertEqual(st.status, 'EXPANSION STARTED')
        self.assertEqual(len(st.uncompleted), 1)
        self.assertEqual(len(st.inprogress), 1)
        self.assertEqual(len(st.completed), 0)

    @leave_phase1
    @start_redistribution
    @expanded_table('public.t1')
    def test_phase2_when_expanded_first_table(self):
        st = get_gpexpand_status()
        st.get_progress()

        self.assertEqual(st.phase, 2)
        self.assertEqual(st.status, 'EXPANSION STARTED')
        self.assertEqual(len(st.uncompleted), 1)
        self.assertEqual(len(st.inprogress), 0)
        self.assertEqual(len(st.completed), 1)

    @leave_phase1
    @start_redistribution
    @expanded_table('public.t1')
    @expanding_table('public.t2')
    def test_phase2_when_expanding_last_table(self):
        st = get_gpexpand_status()
        st.get_progress()

        self.assertEqual(st.phase, 2)
        self.assertEqual(st.status, 'EXPANSION STARTED')
        self.assertEqual(len(st.uncompleted), 0)
        self.assertEqual(len(st.inprogress), 1)
        self.assertEqual(len(st.completed), 1)

    @leave_phase1
    @start_redistribution
    @expanded_table('public.t1')
    @expanded_table('public.t2')
    @stop_redistribution
    def test_phase2_when_expanded_last_table(self):
        st = get_gpexpand_status()
        st.get_progress()

        self.assertEqual(st.phase, 2)
        self.assertEqual(st.status, 'EXPANSION STOPPED')
        self.assertEqual(len(st.uncompleted), 0)
        self.assertEqual(len(st.inprogress), 0)
        self.assertEqual(len(st.completed), 2)

if __name__ == '__main__':
    run_tests()
