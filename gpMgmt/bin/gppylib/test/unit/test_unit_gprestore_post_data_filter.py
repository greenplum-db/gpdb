#!/usr/bin/env python
# coding: utf-8 

import os, sys
import unittest2 as unittest
from gppylib import gplog
from mock import patch
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gprestore_post_data_filter import get_table_schema_set, extract_schema, check_table, \
                                       get_type, process_schema


logger = gplog.get_unittest_logger()

class GpRestoreFilterTestCase(unittest.TestCase):
 
    def test_process_schema_rule(self):

        test_case_buf = """
SET search_path = bial, pg_catalog;

--
-- Name: ab_reporting_beta_match_ins; Type: RULE; Schema: bial; Owner: table_owner
--

CREATE RULE ab_reporting_beta_match_ins AS ON INSERT TO ab_reporting_beta_match DO INSTEAD INSERT INTO weblog_mart_tbls.ab_reporting_beta_match VALUES new.search_session_id;


--
-- Name: RULE ab_reporting_beta_match_ins ON ab_reporting_beta_match; Type: COMMENT; Schema: bial; Owner: table_owner
--

COMMENT ON RULE ab_reporting_beta_match_ins ON ab_reporting_beta_match IS 'version:20110607_1553 generated on 2012-11-29 11:55:23.938893-06';


--
-- Name: ab_reporting_beta_match_upd; Type: RULE; Schema: bial; Owner: table_owner
--

CREATE RULE ab_reporting_beta_match_upd AS ON UPDATE TO ab_reporting_beta_match_1 DO INSTEAD UPDATE weblog_mart_tbls.ab_reporting_beta_match = SET checkin_date = new.checkin_date;
"""
        expected_out = """SET search_path = bial, pg_catalog;
CREATE RULE ab_reporting_beta_match_ins AS ON INSERT TO ab_reporting_beta_match DO INSTEAD INSERT INTO weblog_mart_tbls.ab_reporting_beta_match VALUES new.search_session_id;


"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['bial'])
        dump_tables = set([('bial', 'ab_reporting_beta_match')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_schema(dump_schemas, dump_tables, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)


    def test_process_schema_index(self):

        test_case_buf = """
SET search_path = weblog_mart_tbls, pg_catalog;

--
-- Name: ak1_property_select_error_loadtime; Type: INDEX; Schema: weblog_mart_tbls; Owner: table_owner; Tablespace: 
--

CREATE INDEX ak1_property_select_error_loadtime ON hrs_property_select_errors USING btree (pse_load_timestamp);


--
-- Name: dim_betagroup_idx; Type: INDEX; Schema: weblog_mart_tbls; Owner: table_owner; Tablespace: 
--

CREATE UNIQUE INDEX dim_betagroup_idx ON weblog_mart_tbls.dim_betagroup USING btree (betagroupid);


--
-- Name: dim_cruise_idx; Type: INDEX; Schema: weblog_mart; Owner: table_owner; Tablespace: 
--

CREATE UNIQUE INDEX dim_cruise_idx ON dim_cruise USING btree (cruiseid);
"""
        expected_out = """SET search_path = weblog_mart_tbls, pg_catalog;
CREATE INDEX ak1_property_select_error_loadtime ON hrs_property_select_errors USING btree (pse_load_timestamp);


CREATE UNIQUE INDEX dim_betagroup_idx ON weblog_mart_tbls.dim_betagroup USING btree (betagroupid);


"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['weblog_mart_tbls'])
        dump_tables = set([('weblog_mart_tbls', 'hrs_property_select_errors'), ('weblog_mart_tbls','dim_betagroup')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_schema(dump_schemas, dump_tables, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_process_schema_trigger(self):

        test_case_buf = """
SET search_path = adt, pg_catalog;

--
-- Name: adt_contact_record_encrypt; Type: TRIGGER; Schema: adt; Owner: table_owner
--

CREATE TRIGGER adt_contact_record_encrypt
    BEFORE INSERT OR UPDATE ON adt_contact_record
    FOR EACH ROW
    EXECUTE PROCEDURE adt_contact_record_encrypt();


SET search_path = air, pg_catalog;

--
-- Name: air_itinerary_encrypt; Type: TRIGGER; Schema: air; Owner: table_owner
--

CREATE TRIGGER air_itinerary_encrypt
    BEFORE INSERT OR UPDATE ON air_itinerary
    FOR EACH ROW
    EXECUTE PROCEDURE air_itinerary_encrypt();
"""
        expected_out = """SET search_path = air, pg_catalog;
CREATE TRIGGER air_itinerary_encrypt
    BEFORE INSERT OR UPDATE ON air_itinerary
    FOR EACH ROW
    EXECUTE PROCEDURE air_itinerary_encrypt();
"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['air'])
        dump_tables = set([('air', 'air_itinerary')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_schema(dump_schemas, dump_tables, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_process_schema_constraint(self):

        test_case_buf = """
SET search_path = affil, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: affil_linkshare_sku_pkey; Type: CONSTRAINT; Schema: affil; Owner: table_owner; Tablespace: 
--

ALTER TABLE ONLY affil_linkshare_sku
    ADD CONSTRAINT affil_linkshare_sku_pkey PRIMARY KEY (affil_linkshare_sku_id);


--
-- Name: affil_page_landing_hist_pkey; Type: CONSTRAINT; Schema: affil; Owner: table_owner; Tablespace: 
--

ALTER TABLE affil_page_landing_hist
    ADD CONSTRAINT affil_page_landing_hist_pkey PRIMARY KEY (affil_page_landing_hist_id);
"""
        expected_out = """SET search_path = affil, pg_catalog;
SET default_tablespace = '';

SET default_with_oids = false;

ALTER TABLE ONLY affil_linkshare_sku
    ADD CONSTRAINT affil_linkshare_sku_pkey PRIMARY KEY (affil_linkshare_sku_id);


ALTER TABLE affil_page_landing_hist
    ADD CONSTRAINT affil_page_landing_hist_pkey PRIMARY KEY (affil_page_landing_hist_id);
"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['affil'])
        dump_tables = set([('affil', 'affil_linkshare_sku'), ('affil','affil_page_landing_hist')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_schema(dump_schemas, dump_tables, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_process_schema_no_search_path(self):

        test_case_buf = """
--
-- Name: ab_reporting_beta_match_ins; Type: RULE; Schema: bial; Owner: table_owner
--

CREATE RULE ab_reporting_beta_match_ins AS ON INSERT TO ab_reporting_beta_match DO INSTEAD INSERT INTO weblog_mart_tbls.ab_reporting_beta_match VALUES new.search_session_id;
"""
        expected_out = ""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['bial'])
        dump_tables = set([('bial', 'ab_reporting_beta_match')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_schema(dump_schemas, dump_tables, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_process_schema_rule_mismatched_table(self):

        test_case_buf = """
SET search_path = bial, pg_catalog

--
-- Name: ab_reporting_beta_match_ins; Type: RULE; Schema: bial; Owner: table_owner
--

CREATE RULE ab_reporting_beta_match_ins AS ON INSERT TO weblog_mart_tbls.ab_reporting_beta_match DO INSTEAD INSERT INTO ab_reporting_beta_match VALUES new.search_session_id;
"""
        expected_out = ""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['bial'])
        dump_tables = set([('bial', 'ab_reporting_beta_match')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_schema(dump_schemas, dump_tables, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_check_table_valid(self):
        line = 'ALTER TABLE ONLY public.ao_index_table\n'
        search_str = ' ONLY '
        schema = 'public'
        dump_tables = [('public', 'ao_index_table')]
        
        self.assertTrue(check_table(schema, line, search_str, dump_tables))

    def test_check_table_bad_search_str(self):
        line = 'ALTER TABLE ONLY ao_index_table\n'
        search_str = ' TO '
        schema = 'public'
        dump_tables = [('public', 'ao_index_table')]
        
        self.assertFalse(check_table(schema, line, search_str, dump_tables))

    def test_check_table_mismatched_schema(self):
        line = 'ALTER TABLE ONLY ao_index_table\n'
        search_str = ' ONLY '
        schema = 'pepper'
        dump_tables = [('public', 'ao_index_table')]
        
        self.assertFalse(check_table(schema, line, search_str, dump_tables))

    def test_check_table_mismatched_table(self):
        line = 'ALTER TABLE ONLY public.ao_index_table\n'
        search_str = ' ONLY '
        schema = 'public'
        dump_tables = [('public', 'ao_part_table')]
        
        self.assertFalse(check_table(schema, line, search_str, dump_tables))

    def test_check_table_trailing_text(self):
        line = 'CREATE UNIQUE INDEX ON public.ao_index_table CLUSTER\n'
        search_str = ' ON '
        schema = 'public'
        dump_tables = [('public', 'ao_index_table')]
        
        self.assertTrue(check_table(schema, line, search_str, dump_tables))

    def test_check_table_fully_qualified_table(self):
        line = 'ALTER TABLE ONLY public.ao_index_table\n'
        search_str = ' ONLY '
        schema = 'public'
        dump_tables = [('public', 'ao_index_table')]
        
        self.assertTrue(check_table(schema, line, search_str, dump_tables))

    def test_process_schema_index_pkey(self):

        test_case_buf = """--
-- Greenplum Database database dump
--
SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = user_schema, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: a_pkey; Type: CONSTRAINT; Schema: user_schema; Owner: user_role_b; Tablespace: 
--

ALTER TABLE ONLY user_table
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = public, pg_catalog;

--
-- Name: 测试_btree_index; Type: INDEX; Schema: public; Owner: dcddev; Tablespace: 
--

CREATE INDEX "测试_btree_index" ON "测试" USING btree (id);


SET search_path = user_schema, pg_catalog;

--
-- Name: user_table_idx; Type: INDEX; Schema: user_schema; Owner: user_role_b; Tablespace: 
--

CREATE INDEX user_table_idx ON user_table USING btree (b);


--
-- Greenplum Database database dump complete
--
"""
        expected_out="""SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = user_schema, pg_catalog;
SET default_tablespace = '';

SET default_with_oids = false;

ALTER TABLE ONLY user_table
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = user_schema, pg_catalog;
CREATE INDEX user_table_idx ON user_table USING btree (b);


"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)
        
        dump_schemas = set(['user_schema'])
        dump_tables = set([('user_schema', 'user_table')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin: 
                process_schema(dump_schemas, dump_tables, fdin, fdout)
        
        with open(out_name, 'r') as fd:
            results = fd.read()
        
        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)


    def test_process_schema_index_pkey_special_characters(self):

        test_case_buf = """--
-- Greenplum Database database dump
--
SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = user_schema, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: a_pkey; Type: CONSTRAINT; Schema: user_schema; Owner: user_role_b; Tablespace: 
--

ALTER TABLE ONLY user_table
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = public, pg_catalog;

--
-- Name: 测试_btree_index; Type: INDEX; Schema: public; Owner: dcddev; Tablespace: 
--

CREATE INDEX "测试_btree_index" ON "测试" USING btree (id);


SET search_path = user_schema, pg_catalog;

--
-- Name: user_table_idx; Type: INDEX; Schema: user_schema; Owner: user_role_b; Tablespace: 
--

CREATE INDEX user_table_idx ON user_table USING btree (b);


--
-- Greenplum Database database dump complete
--
"""

        expected_out="""SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_tablespace = '';

SET default_with_oids = false;

SET search_path = public, pg_catalog;
CREATE INDEX "测试_btree_index" ON "测试" USING btree (id);


"""
        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)
        
        dump_schemas = set(['public'])
        dump_tables = set([('public', '测试')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin: 
                process_schema(dump_schemas, dump_tables, fdin, fdout)
        
        with open(out_name, 'r') as fd:
            results = fd.read()
        
        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_special_char_schema_filter_index(self):

        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;


SET search_path = "测试_schema", pg_catalog;

--
-- Name: a_pkey; Type: CONSTRAINT; Schema: 测试_schema; Owner: user_role_b; Tablespace: 
--

ALTER TABLE ONLY "测试"
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = "测试_schema", pg_catalog;

--
-- Name: user_table_idx; Type: INDEX; Schema: 测试_schema; Owner: user_role_b; Tablespace: 
--

CREATE INDEX user_table_idx ON "测试" USING btree (b);


--
-- Greenplum Database database dump complete
--
"""

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_tablespace = '';

SET default_with_oids = false;


SET search_path = "测试_schema", pg_catalog;
ALTER TABLE ONLY "测试"
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = "测试_schema", pg_catalog;
CREATE INDEX user_table_idx ON "测试" USING btree (b);


"""
        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)
        
        dump_schemas = set(['测试_schema'])
        dump_tables = set([('测试_schema', '测试')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin: 
                process_schema(dump_schemas, dump_tables, fdin, fdout)
        
        with open(out_name, 'r') as fd:
            results = fd.read()
        
        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_euro_char_schema_filter_index(self):

        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;


SET search_path = "Áá_schema", pg_catalog;

--
-- Name: a_pkey; Type: CONSTRAINT; Schema: Áá_schema; Owner: user_role_b; Tablespace: 
--

ALTER TABLE ONLY "Áá"
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = "Áá_schema", pg_catalog;

--
-- Name: user_table_idx; Type: INDEX; Schema: Áá_schema; Owner: user_role_b; Tablespace: 
--

CREATE INDEX user_table_idx ON "Áá" USING btree (b);


--
-- Greenplum Database database dump complete
--
"""

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_tablespace = '';

SET default_with_oids = false;


SET search_path = "Áá_schema", pg_catalog;
ALTER TABLE ONLY "Áá"
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = "Áá_schema", pg_catalog;
CREATE INDEX user_table_idx ON "Áá" USING btree (b);


"""
        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)
        
        dump_schemas = set(['Áá_schema'])
        dump_tables = set([('Áá_schema', 'Áá')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin: 
                process_schema(dump_schemas, dump_tables, fdin, fdout)
        
        with open(out_name, 'r') as fd:
            results = fd.read()
        
        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_cyrillic_char_schema_filter_index(self):

        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;


SET search_path = "Ж_schema", pg_catalog;

--
-- Name: a_pkey; Type: CONSTRAINT; Schema: Ж_schema; Owner: user_role_b; Tablespace: 
--

ALTER TABLE ONLY "Ж"
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = "Ж_schema", pg_catalog;

--
-- Name: user_table_idx; Type: INDEX; Schema: Ж_schema; Owner: user_role_b; Tablespace: 
--

CREATE INDEX user_table_idx ON "Ж" USING btree (b);


--
-- Greenplum Database database dump complete
--
"""

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_tablespace = '';

SET default_with_oids = false;


SET search_path = "Ж_schema", pg_catalog;
ALTER TABLE ONLY "Ж"
    ADD CONSTRAINT a_pkey PRIMARY KEY (a);


SET search_path = "Ж_schema", pg_catalog;
CREATE INDEX user_table_idx ON "Ж" USING btree (b);


"""
        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)
        
        dump_schemas = set(['Ж_schema'])
        dump_tables = set([('Ж_schema', 'Ж')])
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin: 
                process_schema(dump_schemas, dump_tables, fdin, fdout)
        
        with open(out_name, 'r') as fd:
            results = fd.read()
        
        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)
