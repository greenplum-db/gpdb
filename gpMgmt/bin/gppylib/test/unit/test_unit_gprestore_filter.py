#!/usr/bin/env python
# coding: utf-8

import os, sys
import unittest
import difflib
from gppylib import gplog
from mock import patch
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gprestore_filter import get_table_schema_set, extract_schema, extract_table, \
                            process_data, get_table_info, process_schema, check_valid_schema, check_valid_relname, \
                            check_dropped_table, get_table_from_alter_table, process_line, Arguments, ParserState


logger = gplog.get_unittest_logger()

class GpRestoreFilterProcessLineTestCase(unittest.TestCase):

    def test_begin_block(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        input_line = "BEGIN"

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.in_block)
        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)

    def test_end_block(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        state.in_block = True
        input_line = "END"

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.in_block)
        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)

    def test_within_block(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        state.in_block = True
        input_line = "SOMETHING"

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.in_block)
        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)

    def test_set_search_path_for_pg_catalog(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        input_line = "SET search_path = pg_catalog;\n"

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(newState.schema, 'pg_catalog')
        self.assertEquals(newState.cast_func_schema, None)
        self.assertEquals(line, input_line)

    def test_set_search_path_schema_in_table_file(self):
        arguments = Arguments(set(['schemaIcareAbout']), set([('schemaIcareAbout', 'table')]))
        state = ParserState()
        input_line = "SET search_path = schemaIcareAbout, pg_catalog;\n"

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(newState.schema, 'schemaIcareAbout')
        self.assertEquals(newState.cast_func_schema, 'schemaIcareAbout')
        self.assertEquals(line, input_line)

    def test_set_search_path_schema_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = ['schemaIcareAbout']
        state = ParserState()
        input_line = "SET search_path = schemaIcareAbout, pg_catalog;\n"

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(newState.schema, 'schemaIcareAbout')
        self.assertEquals(newState.cast_func_schema, 'schemaIcareAbout')
        self.assertEquals(line, input_line)

    def test_set_search_path_change_schema_in_table_file(self):
        arguments = Arguments(set(['schemaIcareAbout']), set([('schemaIcareAbout', 'table')]))
        arguments.change_schema_name = 'newSchema'
        state = ParserState()
        input_line = "SET search_path = schemaIcareAbout, pg_catalog;\n"

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(newState.schema, 'schemaIcareAbout')
        self.assertEquals(newState.cast_func_schema, 'schemaIcareAbout')
        self.assertEquals(line, 'SET search_path = "newSchema", pg_catalog;\n')

    def test_set_search_path_change_quoted_schema_in_table_file_(self):
        arguments = Arguments(set(['schemaIcareAbout']), set([('schemaIcareAbout', 'table')]))
        arguments.change_schema_name = 'newSchema'
        state = ParserState()
        input_line = 'SET search_path = "schemaIcareAbout", pg_catalog;\n'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(newState.schema, 'schemaIcareAbout')
        self.assertEquals(newState.cast_func_schema, 'schemaIcareAbout')
        self.assertEquals(line, 'SET search_path = "newSchema", pg_catalog;\n')

    def test_set_search_path_ignores_unineresting_schemas(self):
        arguments = Arguments(set(['schemaIcareAbout']), set([('schemaIcareAbout', 'table')]))
        state = ParserState()
        input_line = "SET search_path = someOtherSchema, pg_catalog;\n"

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(newState.schema, 'someOtherSchema')
        self.assertEquals(newState.cast_func_schema, None)
        self.assertEquals(line, input_line)

    def test_set_assignment_outputs(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        input_line = "SET SOMETHING=SOMETHING"

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)

    def test_drop_table_statement_drop_schema_section_passed(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        state.passedDropSchemaSection=True
        input_line = "DROP TABLE mytable"

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)

    def test_drop_external_table_statement_drop_schema_section_passed(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        state.passedDropSchemaSection=True
        input_line = "DROP EXTERNAL TABLE mytable"

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)

    def test_drop_table_statement_drop_schema_section_passed(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        state.passedDropSchemaSection=True
        input_line = "DROP EXTERNAL TABLE mytable"

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)

    def test_drop_not_table(self):
        arguments = Arguments(set(['schema']), set([('schema', 'table')]))
        state = ParserState()
        state.passedDropSchemaSection=True
        input_line = "DROP SOME_RANDOM_THING"

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)

    def test_cast_function_schema_with_function(self):
        arguments = Arguments()
        arguments.change_schema_name = 'newSchema'
        state = ParserState()
        state.change_cast_func_schema = True
        state.cast_func_schema = 'schemaToReplace'
        input_line = "CREATE CAST castName WITH FUNCTION schemaToReplace.castToInt(text) AS ASSIGNMENT"

        newState, line = process_line(state, input_line, arguments)

        output_line = 'CREATE CAST castName WITH FUNCTION "newSchema".castToInt(text) AS ASSIGNMENT'
        self.assertFalse(newState.output)
        self.assertEquals(line, output_line)
        self.assertFalse(newState.change_cast_func_schema)
        self.assertEquals(newState.cast_func_schema, None)

    def test_cast_function_schema_with_function_with_quotes(self):
        arguments = Arguments()
        arguments.change_schema_name = 'newSchema'
        state = ParserState()
        state.change_cast_func_schema = True
        state.cast_func_schema = 'schemaToReplace'
        input_line = 'CREATE CAST castName WITH FUNCTION "schemaToReplace".castToInt(text) AS ASSIGNMENT'

        newState, line = process_line(state, input_line, arguments)

        output_line = 'CREATE CAST castName WITH FUNCTION "newSchema".castToInt(text) AS ASSIGNMENT'
        self.assertFalse(newState.output)
        self.assertEquals(line, output_line)
        self.assertFalse(newState.change_cast_func_schema)
        self.assertEquals(newState.cast_func_schema, None)

    def test_schema_expression_in_comments_exists_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: schemaICareAbout; Type: SCHEMA; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_schema_expression_in_comments_exists_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = 'schemaICareAbout'
        state = ParserState()
        input_line = '-- Name: schemaICareAbout; Type: SCHEMA; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_schema_expression_in_comments_does_not_exist(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: schemaICareAbout; Type: SCHEMA; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_non_existent_type_expression_in_comments(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: arbitrary; Type: RANDOM; Schema: schema; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, 'schema')

    def test_table_expression_in_comments_exists_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_table; Type: TABLE; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_table_expression_in_comments_does_not_exist_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_other_table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_table; Type: TABLE; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_external_table_expression_in_comments_exists_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_table; Type: EXTERNAL TABLE; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_external_table_expression_in_comments_does_not_exist_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_other_external_table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_external_table; Type: EXTERNAL TABLE; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_view_expression_in_comments_exists_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_table; Type: VIEW; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_view_expression_in_comments_does_not_exist_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_other_view')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_view; Type: VIEW; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_sequence_expression_in_comments_exists_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_table; Type: SEQUENCE; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_sequence_expression_in_comments_does_not_exist_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_other_sequence')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_sequence; Type: SEQUENCE; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertFalse(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_constraint_expression_in_comments_exists_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_constraint; Type: CONSTRAINT; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.line_buff, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertTrue(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_constraint_expression_in_comments_schema_does_not_exist_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_constraint; Type: CONSTRAINT; Schema: other_schema; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.line_buff, '')
        self.assertFalse(newState.function_ddl)
        self.assertTrue(newState.further_investigation_required)
        self.assertEqual(newState.schema, 'other_schema')

    def test_ACL_expression_in_comments_exists_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'some_ACL')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_ACL; Type: ACL; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_ACL_expression_in_comments_schema_does_not_exist_in_table_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: public; Type: ACL; Schema: other_schema; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, 'other_schema')

    def test_ACL_expression_in_comments_exists_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = set(['schemaICareAbout'])
        state = ParserState()
        input_line = '-- Name: schemaICareAbout; Type: ACL; Schema: -; Owner: user_role_a;'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, '-')

    def test_ACL_expression_in_comments_does_not_exist_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = set(['schemaICareAbout'])
        state = ParserState()
        input_line = '-- Name: other_schema; Type: ACL; Schema: -; Owner: user_role_a;'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.function_ddl)
        self.assertEqual(newState.schema, '-')

    def test_function_expression_in_comments_exists_in_schema_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_function; Type: FUNCTION; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertTrue(newState.function_ddl)
        self.assertEqual(newState.schema, 'schemaICareAbout')

    def test_function_expression_in_comments_schema_does_not_exist_in_schema_file(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_function; Type: FUNCTION; Schema: other_schema; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertTrue(newState.function_ddl)
        self.assertEqual(newState.schema, 'other_schema')

    def test_cast_expression_in_comments(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_cast; Type: CAST; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertTrue(newState.change_cast_func_schema)

    def test_procedural_language_expression_in_comments(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Name: some_proc_lang; Type: PROCEDURAL LANGUAGE; Schema: schemaICareAbout; Owner: user_role_b; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertTrue(newState.change_cast_func_schema)

    def test_data_expression_in_comments_type_a_without_table_data(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        state = ParserState()
        input_line = '-- Data: ao_part_table; Type: SOMETHING; Schema: some_other_schema; Owner: dcddev; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.schema, 'some_other_schema')
        self.assertFalse(newState.further_investigation_required)

    def test_data_expression_in_comments_type_b_without_table_data(self):
        arguments = Arguments(set(['schemaICareAbout']), set([('schemaICareAbout', 'table')]))
        state = ParserState()
        input_line = '-- Data for Name: ao_table; Type: SOMETHING; Schema: some_other_schema; Owner: dcddev'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.schema, 'some_other_schema')
        self.assertFalse(newState.further_investigation_required)

    def test_data_expression_in_comments_type_a_with_table_data_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = ['schemaICareAbout']
        state = ParserState()
        input_line = '-- Data: ao_part_table; Type: TABLE DATA; Schema: schemaICareAbout; Owner: dcddev; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.schema, 'schemaICareAbout')
        self.assertFalse(newState.further_investigation_required)

    def test_data_expression_in_comments_type_a_with_table_data_not_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Data: ao_part_table; Type: TABLE DATA; Schema: some_other_schema; Owner: dcddev; Tablespace:'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.schema, 'some_other_schema')
        self.assertFalse(newState.further_investigation_required)

    def test_data_expression_in_comments_type_b_with_table_data_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = ['schemaICareAbout']
        state = ParserState()
        input_line = '-- Data for Name: ao_table; Type: TABLE DATA; Schema: schemaICareAbout; Owner: dcddev'
        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.schema, 'schemaICareAbout')
        self.assertFalse(newState.further_investigation_required)

    def test_data_expression_in_comments_type_b_with_table_data_not_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = None
        state = ParserState()
        input_line = '-- Data for Name: ao_table; Type: TABLE DATA; Schema: some_other_schema; Owner: dcddev'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertEquals(state.schema, 'some_other_schema')
        self.assertFalse(newState.further_investigation_required)

    def test_further_investigation_without_alter_expression(self):
        arguments = Arguments()
        state = ParserState()
        state.further_investigation_required = True
        input_line = 'RANDOM ARBITRARY EXPRESSION'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertTrue(newState.further_investigation_required)

    def test_further_investigation_with_alter_table_expression_not_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = None
        state = ParserState()
        state.further_investigation_required = True
        input_line = 'ALTER TABLE schema1.table1 OWNER TO gpadmin;'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.further_investigation_required)

    def test_further_investigation_with_alter_table_only_expression_not_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = None
        state = ParserState()
        state.further_investigation_required = True
        state.schema = 'SchemaICareAbout'
        input_line = 'ALTER TABLE ONLY schema.table1 OWNER TO gpadmin;'

        newState, line = process_line(state, input_line, arguments)

        self.assertFalse(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.further_investigation_required)

    def test_further_investigation_with_alter_table_expression_in_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = ['SchemaICareAbout']
        state = ParserState()
        state.schema = 'SchemaICareAbout'
        state.further_investigation_required = True
        input_line = 'ALTER TABLE SchemaICareAbout.table1 OWNER TO gpadmin;'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.further_investigation_required)

    def test_further_investigation_with_alter_table_expression_in_schema_file_with_line_buff(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = ['SchemaICareAbout']
        state = ParserState()
        state.schema = 'SchemaICareAbout'
        state.line_buff = 'some previously saved text'
        state.further_investigation_required = True
        input_line = 'ALTER TABLE SchemaICareAbout.table1 OWNER TO gpadmin;'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, 'some previously saved text' + input_line)
        self.assertFalse(newState.further_investigation_required)

    def test_further_investigation_with_alter_table_expression_in_schema_file_with_quotes(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = ['SchemaICareAbout']
        state = ParserState()
        state.schema = 'SchemaICareAbout'
        state.further_investigation_required = True
        input_line = 'ALTER TABLE SchemaICareAbout."""table1""" OWNER TO gpadmin;'

        newState, line = process_line(state, input_line, arguments)

        self.assertTrue(newState.output)
        self.assertEquals(line, input_line)
        self.assertFalse(newState.further_investigation_required)

class GpRestoreFilterTestCase(unittest.TestCase):

    def prettyAssertEquals(self, actual, expected):
        if actual != expected:
            difflines = difflib.unified_diff(actual.splitlines(), expected.splitlines(), fromfile="actual", tofile="expected")
            diffstring = '\n'.join(difflines)
            self.fail("%s" % diffstring)

    def test_get_table_schema_set00(self):
        fname = os.path.join(os.getcwd(), 'test1')
        with open(fname, 'w') as fd:
            fd.write('public.ao1\n')
            fd.write(' pepper.ao2   \n')

        (sc, tb) = get_table_schema_set(fname)
        self.assertEquals(sc, set(['public', ' pepper']))
        self.assertEquals(tb, set([('public','ao1'), (' pepper','ao2   ')]))

        os.remove(fname)

    def test_get_table_schema_set01(self):
        fname = os.path.join(os.getcwd(), 'test1')
        with open(fname, 'w') as fd:
            fd.write('publicao1\n')
            fd.write(' pepper.ao2   \n')

        with self.assertRaisesRegexp(Exception, "need more than 1 value to unpack"):
            get_table_schema_set(fname)

        os.remove(fname)

    def test_get_table_schema_set02(self):
        fname = os.path.join(os.getcwd(), 'test1')
        with open(fname, 'w') as fd:
            fd.write('')

        (sc, tb) = get_table_schema_set(fname)
        self.assertEquals(sc, set())
        self.assertEquals(tb, set())

        os.remove(fname)

    def test_extract_schema00(self):
        line = 'SET search_path = pepper, pg_catalog;'
        schema = extract_schema(line)
        self.assertEquals(schema, 'pepper')

    def test_extract_schema01(self):
        line = 'SET search_path = pepper pg_catalog;'
        with self.assertRaisesRegexp(Exception, "Failed to extract schema name"):
            schema = extract_schema(line)

    def test_extract_table_with_no_columns(self):
        line = 'COPY nocolumn  FROM stdin;'
        table = extract_table(line)
        self.assertEquals(table, 'nocolumn')

    def test_extract_table_with_one_column(self):
        line = 'COPY onecolumn (a) FROM stdin;'
        table = extract_table(line)
        self.assertEquals(table, 'onecolumn')

    def test_extract_table_with_two_columns(self):
        line = 'COPY twocolumns (a,b) FROM stdin;'
        table = extract_table(line)
        self.assertEquals(table, 'twocolumns')

    def test_extract_table_fails(self):
        line = 'COPYao_table(column1column2column3)FROMstdin;'
        with self.assertRaisesRegexp(Exception, "Failed to extract table name"):
            table = extract_table(line)

    def test_get_table_from_alter_table_with_schemaname(self):
        line = 'ALTER TABLE schema1.table1 OWNER TO gpadmin;'
        alter_expr = "ALTER TABLE"
        res = get_table_from_alter_table(line, alter_expr)
        self.assertEqual(res, 'table1')

    def test_get_table_from_alter_table_without_schemaname(self):
        line = 'ALTER TABLE table1 OWNER TO gpadmin;'
        alter_expr = "ALTER TABLE"
        res = get_table_from_alter_table(line, alter_expr)
        self.assertEqual(res, 'table1')

    def test_get_table_from_alter_table_with_specialchar(self):
        line = 'ALTER TABLE Tab#$_1 OWNER TO gpadmin;'
        alter_expr = "ALTER TABLE"
        res = get_table_from_alter_table(line, alter_expr)
        self.assertEqual(res, 'Tab#$_1')

    def test_get_table_from_alter_table_with_specialchar_and_schema(self):
        line = 'ALTER TABLE "Foo#$1"."Tab#$_1" OWNER TO gpadmin;'
        alter_expr = "ALTER TABLE"
        res = get_table_from_alter_table(line, alter_expr)
        self.assertEqual(res, '"Tab#$_1"')

    def test_get_table_from_alter_table_with_specialchar(self):
        line = 'ALTER TABLE "T a""b#$_1" OWNER TO gpadmin;'
        alter_expr = "ALTER TABLE"
        res = get_table_from_alter_table(line, alter_expr)
        self.assertEqual(res, '"T a""b#$_1"')

    def test_get_table_from_alter_table_with_specialchar_and_double_quoted_schema(self):
        line = 'ALTER TABLE "schema1".table1 OWNER TO gpadmin;'
        alter_expr = "ALTER TABLE"
        res = get_table_from_alter_table(line, alter_expr)
        self.assertEqual(res, 'table1')

    def test_process_data00(self):

        test_case_buf = """
--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = pepper, pg_catalog;

SET default_with_oids = false;

--
-- Data for Name: ao_table; Type: TABLE DATA; Schema: pepper; Owner: dcddev
--

COPY ao_table (column1, column2, column3) FROM stdin;
3	backup	2010-01-04
7	backup	2010-01-08
11	backup	2010-01-12
15	backup	2010-01-16
19	backup	2010-01-20
23	backup	2010-01-24
\.


--
-- Greenplum Database database dump complete
--
"""

        expected_out = """SET search_path = pepper, pg_catalog;
COPY ao_table (column1, column2, column3) FROM stdin;
3	backup	2010-01-04
7	backup	2010-01-08
11	backup	2010-01-12
15	backup	2010-01-16
19	backup	2010-01-20
23	backup	2010-01-24
\.
"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['pepper'])
        dump_tables = set([('pepper', 'ao_table')])
        arguments = Arguments(dump_schemas, dump_tables)

        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_data(arguments, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.prettyAssertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)



    def test_process_data01(self):

        test_case_buf = """
--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = public, pg_catalog;

SET default_with_oids = false;

--
-- Data for Name: ao_index_table; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_index_table (column1, column2, column3) FROM stdin;
3	backup	2010-01-04
7	backup	2010-01-08
11	backup	2010-01-12
15	backup	2010-01-16
1091	restore	2012-12-27
\.


--
-- Data for Name: ao_part_table; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_part_table (column1, column2, column3) FROM stdin;
\.


--
-- Data for Name: ao_part_table_1_prt_p1; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_part_table_1_prt_p1 (column1, column2, column3) FROM stdin;
\.


--
-- Data for Name: ao_part_table_1_prt_p1_2_prt_1; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_part_table_1_prt_p1_2_prt_1 (column1, column2, column3) FROM stdin;
2	backup	2010-01-03
6	backup	2010-01-07
10	backup	2010-01-11
14	backup	2010-01-15
18	backup	2010-01-19
22	backup	2010-01-23
26	backup	2010-01-27
30	backup	2010-01-31
34	backup	2010-02-04
361	backup	2010-12-28
\.


--
-- Data for Name: ao_part_table_1_prt_p1_2_prt_2; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_part_table_1_prt_p1_2_prt_2 (column1, column2, column3) FROM stdin;
365	backup	2011-01-01
369	backup	2011-01-05
719	backup	2011-12-21
723	backup	2011-12-25
727	backup	2011-12-29
\.

--
-- Data for Name: ao_part_table_comp; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_part_table_comp (column1, column2, column3) FROM stdin;
\.


--
-- Data for Name: ao_part_table_comp_1_prt_p1; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_part_table_comp_1_prt_p1 (column1, column2, column3) FROM stdin;
\.


--
-- Data for Name: ao_part_table_comp_1_prt_p1_2_prt_1; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY ao_part_table_comp_1_prt_p1_2_prt_1 (column1, column2, column3) FROM stdin;
1	backup	2010-01-02
5	backup	2010-01-06
9	backup	2010-01-10
13	backup	2010-01-14
17	backup	2010-01-18
1063	restore	2012-11-29
1067	restore	2012-12-03
1071	restore	2012-12-07
1075	restore	2012-12-11
1079	restore	2012-12-15
1083	restore	2012-12-19
1087	restore	2012-12-23
1091	restore	2012-12-27
\.

--
-- Data for Name: set_search_table; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY set_search_table (column1) FROM stdin;
SET search_path = other_schema, pg_catalog;
\.

--
-- Data for Name: some_table; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY some_table (column1, column2, column3) FROM stdin;
1	backup	2010-01-02
5	backup	2010-01-06
\.
--
-- Greenplum Database database dump complete
--
"""

        expected_out = """SET search_path = public, pg_catalog;
COPY ao_part_table_1_prt_p1_2_prt_1 (column1, column2, column3) FROM stdin;
2	backup	2010-01-03
6	backup	2010-01-07
10	backup	2010-01-11
14	backup	2010-01-15
18	backup	2010-01-19
22	backup	2010-01-23
26	backup	2010-01-27
30	backup	2010-01-31
34	backup	2010-02-04
361	backup	2010-12-28
\.
COPY ao_part_table_comp_1_prt_p1_2_prt_1 (column1, column2, column3) FROM stdin;
1	backup	2010-01-02
5	backup	2010-01-06
9	backup	2010-01-10
13	backup	2010-01-14
17	backup	2010-01-18
1063	restore	2012-11-29
1067	restore	2012-12-03
1071	restore	2012-12-07
1075	restore	2012-12-11
1079	restore	2012-12-15
1083	restore	2012-12-19
1087	restore	2012-12-23
1091	restore	2012-12-27
\.
COPY set_search_table (column1) FROM stdin;
SET search_path = other_schema, pg_catalog;
\.
COPY some_table (column1, column2, column3) FROM stdin;
1	backup	2010-01-02
5	backup	2010-01-06
\.
"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['public'])
        dump_tables = set([('public', 'ao_part_table_comp_1_prt_p1_2_prt_1'), ('public', 'ao_part_table_1_prt_p1_2_prt_1'), ('public', 'set_search_table'), ('public', 'some_table')])
        arguments = Arguments(dump_schemas, dump_tables)
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_data(arguments, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.prettyAssertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)


    def test_process_data03(self):

        test_case_buf = """
COPY ao_table (column1, column2, column3) FROM stdin;
3	backup	2010-01-04
7	backup	2010-01-08
11	backup	2010-01-12
15	backup	2010-01-16
19	backup	2010-01-20
23	backup	2010-01-24
\.
"""
        expected_out = ''

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['public'])
        dump_tables = set([('public', 'ao_table')])
        arguments = Arguments(dump_schemas, dump_tables)
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_data(arguments, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.prettyAssertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)


    def test_process_data04(self):

        test_case_buf = """
--
-- Greenplum Database database dump
--

SET search_path = pepper, pg_catalog;

--
-- Data for Name: ao_table; Type: TABLE DATA; Schema: pepper; Owner: dcddev
--

 COPY ao_table (column1, column2, column3) FROM stdin;
3	backup	2010-01-04
7	backup	2010-01-08
11	backup	2010-01-12
15	backup	2010-01-16
19	backup	2010-01-20
23	backup	2010-01-24
\.


--
-- Greenplum Database database dump complete
--
"""

        expected_out = """SET search_path = pepper, pg_catalog;
"""

        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['pepper'])
        dump_tables = set([('pepper', 'ao_table')])
        arguments = Arguments(dump_schemas, dump_tables)
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_data(arguments, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.prettyAssertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_process_data_multi_byte_char(self):

        test_case_buf = """SET search_path = public, pg_catalog;

--
-- Data for Name: 测试; Type: TABLE DATA; Schema: public; Owner: dcddev
--

COPY "测试" (column1, column2, column3) FROM stdin;
3	backup	2010-01-04
7	backup	2010-01-08
11	backup	2010-01-12
15	backup	2010-01-16
19	backup	2010-01-20
23	backup	2010-01-24
\.
"""
        expected_out = """SET search_path = public, pg_catalog;
COPY "测试" (column1, column2, column3) FROM stdin;
3	backup	2010-01-04
7	backup	2010-01-08
11	backup	2010-01-12
15	backup	2010-01-16
19	backup	2010-01-20
23	backup	2010-01-24
\.
"""
        in_name = os.path.join(os.getcwd(), 'infile')
        out_name = os.path.join(os.getcwd(), 'outfile')
        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        dump_schemas = set(['public'])
        dump_tables = set([('public', '测试')])
        arguments = Arguments(dump_schemas, dump_tables)
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_data(arguments, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()

        self.prettyAssertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)

    def test_get_table_info00(self):
        line = ''
        (name, type, schema) = get_table_info(line, '-- Name: ')
        self.assertEquals(name, None)
        self.assertEquals(type, None)
        self.assertEquals(schema, None)

    def test_get_table_info01(self):
        line = '-- Name: public; Type: ACL; Schema: -; Owner: root'
        comment_expr = '-- Name: '
        (name, type, schema) = get_table_info(line, comment_expr)
        self.assertEquals(name, 'public')
        self.assertEquals(type, 'ACL')
        self.assertEquals(schema, '-')

    def test_process_schema00(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        dump_schemas = ['public']
        dump_tables = [('public', 'heap_table1')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_function_with_begin_end_block_should_exclude(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

SET search_path = public, pg_catalog;

SET default_with_oids = false;

--
-- Name: check_password(text, text); Type: FUNCTION; Schema: public; Owner: chajas
--

CREATE FUNCTION check_password(uname text, pass text) RETURNS boolean
    AS $$
DECLARE passed BOOLEAN;
BEGIN
        create table public.test (id int) ;
        drop table public.test ;
END;
$$
    LANGUAGE plpgsql NO SQL;


ALTER FUNCTION public.check_password(uname text, pass text) OWNER TO chajas;

--
-- Name: public; Type: ACL; Schema: -; Owner: chajas
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM chajas;
GRANT ALL ON SCHEMA public TO chajas;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- Greenplum Database database dump complete
--"""

        dump_schemas = ['testschema']
        arguments = Arguments(dump_schemas)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

SET default_with_oids = false;

--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_function_with_begin_end_block_should_include(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

SET search_path = public, pg_catalog;

SET default_with_oids = false;

--
-- Name: check_password(text, text); Type: FUNCTION; Schema: public; Owner: chajas
--

CREATE FUNCTION check_password(uname text, pass text) RETURNS boolean
    AS $$
DECLARE passed BOOLEAN;
BEGIN
        create table public.test (id int) ;
        drop table public.test ;
END;
$$
    LANGUAGE plpgsql NO SQL;


ALTER FUNCTION public.check_password(uname text, pass text) OWNER TO chajas;

--
-- Name: public; Type: ACL; Schema: -; Owner: chajas
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM chajas;
GRANT ALL ON SCHEMA public TO chajas;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- Greenplum Database database dump complete
--"""

        dump_schemas = ['public']
        arguments = Arguments(dump_schemas)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

SET search_path = public, pg_catalog;

SET default_with_oids = false;

--
-- Name: check_password(text, text); Type: FUNCTION; Schema: public; Owner: chajas
--

CREATE FUNCTION check_password(uname text, pass text) RETURNS boolean
    AS $$
DECLARE passed BOOLEAN;
BEGIN
        create table public.test (id int) ;
        drop table public.test ;
END;
$$
    LANGUAGE plpgsql NO SQL;


ALTER FUNCTION public.check_password(uname text, pass text) OWNER TO chajas;

--
-- Name: public; Type: ACL; Schema: -; Owner: chajas
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM chajas;
GRANT ALL ON SCHEMA public TO chajas;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- Greenplum Database database dump complete
--"""
        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)
    def test_process_schema_matching_table(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

--
-- Name: heap_table; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        dump_schemas = ['public']
        dump_tables = [('public', 'heap_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: heap_table; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_mismatched_table(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        dump_schemas = ['public']
        dump_tables = [('pepper', 'heap_table1')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
"""
        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_mismatched_schema(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        dump_schemas = ['public']
        dump_tables = [('public', 'heap_table1'), ('pepper','ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_missing_schema(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        dump_schemas = ['public']
        dump_tables = [('public', 'heap_table1'), ('pepper','ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_matching_constraint(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        dump_schemas = ['public']
        dump_tables = [('public', 'heap_table1'), ('public','ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_mismatched_constraint(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        dump_schemas = ['public']
        dump_tables = [('public', 'heap_table1')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: heap_table1; Type: TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE heap_table1 (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1);"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_data(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Data: ao_part_table; Type: TABLE DATA; Schema: public; Owner: dcddev; Tablespace:
--

COPY ao_part_table from stdin;
1
2
3
4
5
6
\.
"""

        dump_schemas = ['public']
        dump_tables = [('public', 'ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Data: ao_part_table; Type: TABLE DATA; Schema: public; Owner: dcddev; Tablespace:
--

COPY ao_part_table from stdin;
1
2
3
4
5
6
\.
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_function(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Data: ao_part_table; Type: TABLE DATA; Schema: public; Owner: dcddev; Tablespace:
--

COPY ao_part_table from stdin;
1
2
3
4
5
6
\.
"""

        dump_schemas = ['public']
        dump_tables = [('public', 'ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Data: ao_part_table; Type: TABLE DATA; Schema: public; Owner: dcddev; Tablespace:
--

COPY ao_part_table from stdin;
1
2
3
4
5
6
\.
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_function_external_table(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)"""

        dump_schemas = ['public']
        dump_tables = [('public', 'ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_matching_view_function_seq(self):
        test_case_buf = """--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: gpadmin
--

COMMENT ON SCHEMA public IS 'Standard public schema';

--
-- Name: s1; Type: SCHEMA; Schema: -; Owner: gpadmin
--

CREATE SCHEMA s1;

ALTER SCHEMA s1 OWNER TO gpadmin;

SET search_path = public, pg_catalog;

--
-- Name: table1; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE table1 (
    i integer,
    j text
) DISTRIBUTED BY (i);
;

ALTER TABLE public.table1 OWNER TO gpadmin;

--
-- Name: view1; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW view1 AS SELECT * FROM table1;

ALTER TABLE public.view1 OWNER TO gpadmin;

SET search_path = s1, pg_catalog;

--
-- Name: t1; Type: TABLE; Schema: s1; Owner: gpadmin; Tablespace:
--

CREATE TABLE t1 (
    c1 integer,
    c2 text,
    c3 date
) DISTRIBUTED BY (c1);
;

ALTER TABLE s1.t1 OWNER TO gpadmin;

--
-- Name: v1; Type: VIEW; Schema: s1; Owner: gpadmin
--

CREATE VIEW v1 AS
    SELECT t1.c1, t1.c2 FROM t1;

ALTER TABLE s1.v1 OWNER TO gpadmin;

--
-- Name: user_defined_function; Type: FUNCTION; Schema: s1; Owner: gpadmin; Tablespace:
--

CREATE FUNCTION user_defined_function as $$
print 'Hello, World'
$$ LANGUAGE as plpgsql;

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: s1; Owner: gpadmin; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)

--
-- Name: id_seq; Type: SEQUENCE; Schema: s1; Owner: gpadmin
--

CREATE SEQUENCE id_seq
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


ALTER TABLE s1.id_seq OWNER TO gpadmin;"""

        in_name = '/tmp/infile'
        out_name = '/tmp/outfile'
        arguments = Arguments()
        arguments.schemas_in_schema_file=['s1']

        with open(in_name, 'w') as fd:
            fd.write(test_case_buf)

        schemas_in_schema_file=['s1']
        with open(out_name, 'w') as fdout:
            with open(in_name, 'r') as fdin:
                process_schema(arguments, fdin, fdout)

        with open(out_name, 'r') as fd:
            results = fd.read()


        expected_out = """-- Name: s1; Type: SCHEMA; Schema: -; Owner: gpadmin
--

CREATE SCHEMA s1;

ALTER SCHEMA s1 OWNER TO gpadmin;

SET search_path = s1, pg_catalog;

--
-- Name: t1; Type: TABLE; Schema: s1; Owner: gpadmin; Tablespace:
--

CREATE TABLE t1 (
    c1 integer,
    c2 text,
    c3 date
) DISTRIBUTED BY (c1);
;

ALTER TABLE s1.t1 OWNER TO gpadmin;

--
-- Name: v1; Type: VIEW; Schema: s1; Owner: gpadmin
--

CREATE VIEW v1 AS
    SELECT t1.c1, t1.c2 FROM t1;

ALTER TABLE s1.v1 OWNER TO gpadmin;

--
-- Name: user_defined_function; Type: FUNCTION; Schema: s1; Owner: gpadmin; Tablespace:
--

CREATE FUNCTION user_defined_function as $$
print 'Hello, World'
$$ LANGUAGE as plpgsql;

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: s1; Owner: gpadmin; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)

--
-- Name: id_seq; Type: SEQUENCE; Schema: s1; Owner: gpadmin
--

CREATE SEQUENCE id_seq
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


ALTER TABLE s1.id_seq OWNER TO gpadmin;"""

        self.assertEquals(results, expected_out)
        os.remove(in_name)
        os.remove(out_name)


    def test_process_schema_function_empty_fitler_list(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Name: ao_part_table_view; Type: VIEW; Schema: public; Owner: dcddev; Tablespace:
--
CREATE VIEW ao_part_table_view as SELECT * FROM ao_part_table;

--
-- Name: user_defined_function; Type: FUNCTION; Schema: public; Owner: dcddev; Tablespace:
--
CREATE FUNCTION user_defined_function as $$
print 'Hello, World'
$$ LANGUAGE as plpgsql;

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)"""

        dump_schemas = []
        dump_tables = []
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET default_tablespace = '';

--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_function_non_matching_filter(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: public; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)"""

        dump_schemas = ['no_match_schema']
        dump_tables = [('no_match_schema', 'no_match_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET default_tablespace = '';

--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_process_schema_function_non_matching_constraint(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';

SET search_path = some_schema, pg_catalog;

--
-- Name: ao_part_table_constraint; Type: CONSTRAINT; Schema: some_schema; Owner: dcddev; Tablespace:
--

ALTER TABLE ONLY public.ao_part_table
    ADD CONSTRAINT constraint_name PRIMARY KEY (name);


SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)"""

        dump_schemas = ['public']
        dump_tables = [('public', 'ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: EXTERNAL TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)"""


        with open(outfile, 'r') as fd:
            results = fd.read()
        self.assertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_check_valid_schema01(self):
        dump_schemas = set(['schema1', 'schema2'])
        name = 'schema1'
        output = check_valid_schema(name, dump_schemas)
        self.assertEquals(output, True)

    def test_check_valid_schema02(self):
        dump_schemas = set(['schema1', 'schema2'])
        name = 'schema3'
        output = check_valid_schema(name, dump_schemas)
        self.assertEquals(output, False)

    def test_check_valid_schema03(self):
        dump_schemas = set(['schema1', 'schema2'])
        name = ''
        output = check_valid_schema(name, dump_schemas)
        self.assertEquals(output, False)

    def test_check_valid_schema04(self):
        dump_schemas = set()
        name = 'schema1'
        output = check_valid_schema(name, dump_schemas)
        self.assertEquals(output, False)

    def test_check_valid_relname01(self):
        dump_tables = [('public', 'ao_part_table')]
        name = 'ao_part_table'
        schema = 'public'
        output = check_valid_relname(schema, name, dump_tables)
        self.assertEquals(output, True)

    def test_check_valid_relname02(self):
        dump_tables = [('public', 'ao_part_table')]
        name = 'ao_part_table'
        schema = 'pepper'
        output = check_valid_relname(schema, name, dump_tables)
        self.assertEquals(output, False)

    def test_check_valid_relname03(self):
        dump_tables = [('public', 'ao_part_table')]
        name = 'co_part_table'
        schema = 'public'
        output = check_valid_relname(schema, name, dump_tables)
        self.assertEquals(output, False)

    def test_process_schema_single_table(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';


--
-- Name: user_schema_a; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_a;


ALTER SCHEMA user_schema_a OWNER TO user_role_a;

--
-- Name: user_schema_b; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_b;


ALTER SCHEMA user_schema_b OWNER TO user_role_a;

--
-- Name: user_schema_c; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_c;


ALTER SCHEMA user_schema_c OWNER TO user_role_a;

--
-- Name: user_schema_d; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_d;


ALTER SCHEMA user_schema_d OWNER TO user_role_a;

--
-- Name: user_schema_e; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_e;


ALTER SCHEMA user_schema_e OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;


SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

--
-- Name: user_table; Type: TABLE; Schema: user_schema_a; Owner: user_role_b; Tablespace:
--

CREATE TABLE user_table (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_a.user_table OWNER TO user_role_b;

SET search_path = user_schema_b, pg_catalog;

--
-- Name: test_table; Type: TABLE; Schema: user_schema_b; Owner: dcddev; Tablespace:
--

CREATE TABLE test_table (
    a integer
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_b.test_table OWNER TO dcddev;

SET search_path = user_schema_c, pg_catalog;

--
-- Name: test_table; Type: TABLE; Schema: user_schema_c; Owner: user_role_b; Tablespace:
--

CREATE TABLE test_table (
    a integer
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_c.test_table OWNER TO user_role_b;

SET search_path = user_schema_d, pg_catalog;

--
-- Name: test_table; Type: TABLE; Schema: user_schema_d; Owner: dcddev; Tablespace:
--

CREATE TABLE test_table (
    a integer
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_d.test_table OWNER TO dcddev;

SET search_path = user_schema_e, pg_catalog;

--
-- Name: test_table; Type: TABLE; Schema: user_schema_e; Owner: dcddev; Tablespace:
--

CREATE TABLE test_table (
    a integer
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_e.test_table OWNER TO dcddev;

SET search_path = user_schema_a, pg_catalog;

--
-- Data for Name: user_table; Type: TABLE DATA; Schema: user_schema_a; Owner: user_role_b
--

COPY user_table (a, b) FROM stdin;
\.


SET search_path = user_schema_b, pg_catalog;

--
-- Data for Name: test_table; Type: TABLE DATA; Schema: user_schema_b; Owner: dcddev
--

COPY test_table (a) FROM stdin;
\.


SET search_path = user_schema_c, pg_catalog;

--
-- Data for Name: test_table; Type: TABLE DATA; Schema: user_schema_c; Owner: dcddev
--

COPY test_table (a) FROM stdin;
\.


SET search_path = user_schema_d, pg_catalog;

--
-- Data for Name: test_table; Type: TABLE DATA; Schema: user_schema_d; Owner: dcddev
--

COPY test_table (a) FROM stdin;
\.


SET search_path = user_schema_e, pg_catalog;

--
-- Data for Name: test_table; Type: TABLE DATA; Schema: user_schema_e; Owner: dcddev
--

COPY test_table (a) FROM stdin;
\.


--
-- Name: public; Type: ACL; Schema: -; Owner: dcddev
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM dcddev;
GRANT ALL ON SCHEMA public TO dcddev;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- Name: user_schema_b; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA user_schema_b FROM PUBLIC;
REVOKE ALL ON SCHEMA user_schema_b FROM user_role_a;
GRANT ALL ON SCHEMA user_schema_b TO user_role_a;

--
-- Name: user_schema_a; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA user_schema_a FROM PUBLIC;
REVOKE ALL ON SCHEMA user_schema_a FROM user_role_a;
GRANT ALL ON SCHEMA user_schema_a TO user_role_a;


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_table; Type: ACL; Schema: user_schema_a; Owner: user_role_b
--

REVOKE ALL ON TABLE user_table FROM PUBLIC;
REVOKE ALL ON TABLE user_table FROM user_role_b;
GRANT ALL ON TABLE user_table TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""

        dump_schemas = ['user_schema_a', 'user_schema_e']
        dump_tables = [('user_schema_a', 'user_table'), ('user_schema_e', 'test_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: user_schema_a; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_a;


ALTER SCHEMA user_schema_a OWNER TO user_role_a;

--
-- Name: user_schema_e; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_e;


ALTER SCHEMA user_schema_e OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;


SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

--
-- Name: user_table; Type: TABLE; Schema: user_schema_a; Owner: user_role_b; Tablespace:
--

CREATE TABLE user_table (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_a.user_table OWNER TO user_role_b;

SET search_path = user_schema_e, pg_catalog;

--
-- Name: test_table; Type: TABLE; Schema: user_schema_e; Owner: dcddev; Tablespace:
--

CREATE TABLE test_table (
    a integer
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_e.test_table OWNER TO dcddev;

SET search_path = user_schema_a, pg_catalog;

--
-- Data for Name: user_table; Type: TABLE DATA; Schema: user_schema_a; Owner: user_role_b
--

COPY user_table (a, b) FROM stdin;
\.


SET search_path = user_schema_e, pg_catalog;

--
-- Data for Name: test_table; Type: TABLE DATA; Schema: user_schema_e; Owner: dcddev
--

COPY test_table (a) FROM stdin;
\.


--
-- Name: user_schema_a; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA user_schema_a FROM PUBLIC;
REVOKE ALL ON SCHEMA user_schema_a FROM user_role_a;
GRANT ALL ON SCHEMA user_schema_a TO user_role_a;


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_table; Type: ACL; Schema: user_schema_a; Owner: user_role_b
--

REVOKE ALL ON TABLE user_table FROM PUBLIC;
REVOKE ALL ON TABLE user_table FROM user_role_b;
GRANT ALL ON TABLE user_table TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

        os.remove(infile)
        os.remove(outfile)

    def test_check_dropped_table00(self):
        line = 'DROP TABLE public.ao_part_table;'
        dump_tables = [('public', 'ao_part_table')]
        drop_table_expr = 'DROP TABLE '
        output = check_dropped_table(line, dump_tables, None, drop_table_expr)
        self.assertTrue(output)

    def test_check_dropped_table01(self):
        line = 'DROP TABLE public.ao_part_table;'
        dump_tables = [('pepper', 'ao_part_table')]
        drop_table_expr = 'DROP TABLE '
        output = check_dropped_table(line, dump_tables, None, drop_table_expr)
        self.assertFalse(output)

    def test_check_dropped_table02(self):
        line = 'DROP TABLE public.ao_part_table;'
        dump_tables = [('public', 'ao_table')]
        drop_table_expr = 'DROP TABLE '
        output = check_dropped_table(line, dump_tables, None, drop_table_expr)
        self.assertFalse(output)

    def test_process_schema_foreign_table(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
-- Name: ao_part_table; Type: FOREIGN TABLE; Schema: public; Owner: dcddev; Tablespace:
--

CREATE FOREIGN TABLE ao_part_table (
    column1 integer,
    column2 character varying(20),
    column3 date
) DISTRIBUTED BY (column1); with (appendonly=true)"""

        dump_schemas = ['public']
        dump_tables = [('public', 'ao_part_table')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET search_path = public, pg_catalog;

SET default_tablespace = '';

--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

    def get_test_case_buf_for_process_schema_with_privileges(self):
        return """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: dcddev
--

COMMENT ON SCHEMA public IS 'Standard public schema';


--
-- Name: user_schema_a; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_a;


ALTER SCHEMA user_schema_a OWNER TO user_role_a;

--
-- Name: user_schema_b; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_b;


ALTER SCHEMA user_schema_b OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;


SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

--
-- Name: user_table_1; Type: TABLE; Schema: user_schema_a; Owner: user_role_b; Tablespace:
--

CREATE TABLE user_table_1 (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_a.user_table_1 OWNER TO user_role_b;

--
-- Name: user_table_2; Type: TABLE; Schema: user_schema_a; Owner: user_role_b; Tablespace:
--

CREATE TABLE user_table_2 (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_a.user_table_2 OWNER TO user_role_b;

SET search_path = user_schema_b, pg_catalog;

--
-- Name: test_table; Type: TABLE; Schema: user_schema_b; Owner: dcddev; Tablespace:
--

CREATE TABLE test_table (
    a integer
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_b.test_table OWNER TO dcddev;

SET search_path = user_schema_a, pg_catalog;

--
-- Data for Name: user_table_1; Type: TABLE DATA; Schema: user_schema_a; Owner: user_role_b
--

COPY user_table_1 (a, b) FROM stdin;
\.


SET search_path = user_schema_a, pg_catalog;

--
-- Data for Name: user_table_2; Type: TABLE DATA; Schema: user_schema_a; Owner: user_role_b
--

COPY user_table_2 (a, b) FROM stdin;
\.


SET search_path = user_schema_b, pg_catalog;

--
-- Data for Name: test_table; Type: TABLE DATA; Schema: user_schema_b; Owner: dcddev
--

COPY test_table (a) FROM stdin;
\.


--
-- Name: public; Type: ACL; Schema: -; Owner: dcddev
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM dcddev;
GRANT ALL ON SCHEMA public TO dcddev;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- Name: user_schema_b; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA user_schema_b FROM PUBLIC;
REVOKE ALL ON SCHEMA user_schema_b FROM user_role_a;
GRANT ALL ON SCHEMA user_schema_b TO user_role_a;

--
-- Name: user_schema_a; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA user_schema_a FROM PUBLIC;
REVOKE ALL ON SCHEMA user_schema_a FROM user_role_a;
GRANT ALL ON SCHEMA user_schema_a TO user_role_a;


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_table_1; Type: ACL; Schema: user_schema_a; Owner: user_role_b
--

REVOKE ALL ON TABLE user_table_1 FROM PUBLIC;
REVOKE ALL ON TABLE user_table_1 FROM user_role_b;
GRANT ALL ON TABLE user_table_1 TO user_role_b;


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_table_2; Type: ACL; Schema: user_schema_a; Owner: user_role_b
--

REVOKE ALL ON TABLE user_table_2 FROM PUBLIC;
REVOKE ALL ON TABLE user_table_2 FROM user_role_b;
GRANT ALL ON TABLE user_table_2 TO user_role_b;

--
-- Greenplum Database database dump complete
--
"""
    def test_process_schema_with_privileges_using_table_file(self):

        dump_schemas = ['user_schema_a']
        dump_tables = [('user_schema_a', 'user_table_1')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(self.get_test_case_buf_for_process_schema_with_privileges())

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: user_schema_a; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_a;


ALTER SCHEMA user_schema_a OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;


SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

--
-- Name: user_table_1; Type: TABLE; Schema: user_schema_a; Owner: user_role_b; Tablespace:
--

CREATE TABLE user_table_1 (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_a.user_table_1 OWNER TO user_role_b;

--
SET search_path = user_schema_a, pg_catalog;

--
-- Data for Name: user_table_1; Type: TABLE DATA; Schema: user_schema_a; Owner: user_role_b
--

COPY user_table_1 (a, b) FROM stdin;
\.


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_schema_a; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA user_schema_a FROM PUBLIC;
REVOKE ALL ON SCHEMA user_schema_a FROM user_role_a;
GRANT ALL ON SCHEMA user_schema_a TO user_role_a;


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_table_1; Type: ACL; Schema: user_schema_a; Owner: user_role_b
--

REVOKE ALL ON TABLE user_table_1 FROM PUBLIC;
REVOKE ALL ON TABLE user_table_1 FROM user_role_b;
GRANT ALL ON TABLE user_table_1 TO user_role_b;


SET search_path = user_schema_a, pg_catalog;

--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

    def test_process_schema_with_privileges_using_schema_file(self):
        arguments = Arguments()
        arguments.schemas_in_schema_file = ['user_schema_a']

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(self.get_test_case_buf_for_process_schema_with_privileges())

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: user_schema_a; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA user_schema_a;


ALTER SCHEMA user_schema_a OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;


SET search_path = user_schema_a, pg_catalog;

SET default_tablespace = '';

--
-- Name: user_table_1; Type: TABLE; Schema: user_schema_a; Owner: user_role_b; Tablespace:
--

CREATE TABLE user_table_1 (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_a.user_table_1 OWNER TO user_role_b;

--
-- Name: user_table_2; Type: TABLE; Schema: user_schema_a; Owner: user_role_b; Tablespace:
--

CREATE TABLE user_table_2 (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE user_schema_a.user_table_2 OWNER TO user_role_b;

SET search_path = user_schema_a, pg_catalog;

--
-- Data for Name: user_table_1; Type: TABLE DATA; Schema: user_schema_a; Owner: user_role_b
--

COPY user_table_1 (a, b) FROM stdin;
\.


SET search_path = user_schema_a, pg_catalog;

--
-- Data for Name: user_table_2; Type: TABLE DATA; Schema: user_schema_a; Owner: user_role_b
--

COPY user_table_2 (a, b) FROM stdin;
\.


-- Name: user_schema_a; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA user_schema_a FROM PUBLIC;
REVOKE ALL ON SCHEMA user_schema_a FROM user_role_a;
GRANT ALL ON SCHEMA user_schema_a TO user_role_a;


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_table_1; Type: ACL; Schema: user_schema_a; Owner: user_role_b
--

REVOKE ALL ON TABLE user_table_1 FROM PUBLIC;
REVOKE ALL ON TABLE user_table_1 FROM user_role_b;
GRANT ALL ON TABLE user_table_1 TO user_role_b;


SET search_path = user_schema_a, pg_catalog;

--
-- Name: user_table_2; Type: ACL; Schema: user_schema_a; Owner: user_role_b
--

REVOKE ALL ON TABLE user_table_2 FROM PUBLIC;
REVOKE ALL ON TABLE user_table_2 FROM user_role_b;
GRANT ALL ON TABLE user_table_2 TO user_role_b;

--
-- Greenplum Database database dump complete
--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)


    def test_special_char_schema_name_filter(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: 测试_schema; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA "测试_schema";


ALTER SCHEMA "测试_schema" OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;

SET search_path = "测试_schema", pg_catalog;

--
-- Name: 测试; Type: TABLE; Schema: 测试_schema; Owner: user_role_b; Tablespace:
--

CREATE TABLE "测试" (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE "测试_schema"."测试" OWNER TO user_role_b;

SET search_path = "测试_schema", pg_catalog;

--
-- Data for Name: 测试; Type: TABLE DATA; Schema: 测试_schema; Owner: user_role_b
--

COPY "测试" (a, b) FROM stdin;
\.


--
-- Name: public; Type: ACL; Schema: -; Owner: dcddev
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM dcddev;
GRANT ALL ON SCHEMA public TO dcddev;
GRANT ALL ON SCHEMA public TO PUBLIC;

--
-- Name: 测试_schema; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA "测试_schema" FROM PUBLIC;
REVOKE ALL ON SCHEMA "测试_schema" FROM user_role_a;
GRANT ALL ON SCHEMA "测试_schema" TO user_role_a;

SET search_path = "测试_schema", pg_catalog;

--
-- Name: 测试; Type: ACL; Schema: 测试_schema; Owner: user_role_b
--

REVOKE ALL ON TABLE "测试" FROM PUBLIC;
REVOKE ALL ON TABLE "测试" FROM user_role_b;
GRANT ALL ON TABLE "测试" TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""
        dump_schemas = ['测试_schema']
        dump_tables = [('测试_schema', '测试')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: 测试_schema; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA "测试_schema";


ALTER SCHEMA "测试_schema" OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;

SET search_path = "测试_schema", pg_catalog;

--
-- Name: 测试; Type: TABLE; Schema: 测试_schema; Owner: user_role_b; Tablespace:
--

CREATE TABLE "测试" (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE "测试_schema"."测试" OWNER TO user_role_b;

SET search_path = "测试_schema", pg_catalog;

--
-- Data for Name: 测试; Type: TABLE DATA; Schema: 测试_schema; Owner: user_role_b
--

COPY "测试" (a, b) FROM stdin;
\.


--
-- Name: 测试_schema; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA "测试_schema" FROM PUBLIC;
REVOKE ALL ON SCHEMA "测试_schema" FROM user_role_a;
GRANT ALL ON SCHEMA "测试_schema" TO user_role_a;

SET search_path = "测试_schema", pg_catalog;

--
-- Name: 测试; Type: ACL; Schema: 测试_schema; Owner: user_role_b
--

REVOKE ALL ON TABLE "测试" FROM PUBLIC;
REVOKE ALL ON TABLE "测试" FROM user_role_b;
GRANT ALL ON TABLE "测试" TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

    def test_euro_char_schema_name_filter(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: Áá_schema; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA "Áá_schema";


ALTER SCHEMA "Áá_schema" OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;

SET search_path = "Áá_schema", pg_catalog;

--
-- Name: Áá; Type: TABLE; Schema: Áá_schema; Owner: user_role_b; Tablespace:
--

CREATE TABLE "Áá" (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE "Áá_schema"."Áá" OWNER TO user_role_b;

SET search_path = "Áá_schema", pg_catalog;

--
-- Data for Name: Áá; Type: TABLE DATA; Schema: Áá_schema; Owner: user_role_b
--

COPY "Áá" (a, b) FROM stdin;
\.


--
-- Name: public; Type: ACL; Schema: -; Owner: dcddev
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM dcddev;
GRANT ALL ON SCHEMA public TO dcddev;
GRANT ALL ON SCHEMA public TO PUBLIC;

--
-- Name: Áá_schema; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA "Áá_schema" FROM PUBLIC;
REVOKE ALL ON SCHEMA "Áá_schema" FROM user_role_a;
GRANT ALL ON SCHEMA "Áá_schema" TO user_role_a;

SET search_path = "Áá_schema", pg_catalog;

--
-- Name: Áá; Type: ACL; Schema: Áá_schema; Owner: user_role_b
--

REVOKE ALL ON TABLE "Áá" FROM PUBLIC;
REVOKE ALL ON TABLE "Áá" FROM user_role_b;
GRANT ALL ON TABLE "Áá" TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""
        dump_schemas = ['Áá_schema']
        dump_tables = [('Áá_schema', 'Áá')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: Áá_schema; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA "Áá_schema";


ALTER SCHEMA "Áá_schema" OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;

SET search_path = "Áá_schema", pg_catalog;

--
-- Name: Áá; Type: TABLE; Schema: Áá_schema; Owner: user_role_b; Tablespace:
--

CREATE TABLE "Áá" (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE "Áá_schema"."Áá" OWNER TO user_role_b;

SET search_path = "Áá_schema", pg_catalog;

--
-- Data for Name: Áá; Type: TABLE DATA; Schema: Áá_schema; Owner: user_role_b
--

COPY "Áá" (a, b) FROM stdin;
\.


--
-- Name: Áá_schema; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA "Áá_schema" FROM PUBLIC;
REVOKE ALL ON SCHEMA "Áá_schema" FROM user_role_a;
GRANT ALL ON SCHEMA "Áá_schema" TO user_role_a;

SET search_path = "Áá_schema", pg_catalog;

--
-- Name: Áá; Type: ACL; Schema: Áá_schema; Owner: user_role_b
--

REVOKE ALL ON TABLE "Áá" FROM PUBLIC;
REVOKE ALL ON TABLE "Áá" FROM user_role_b;
GRANT ALL ON TABLE "Áá" TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

    def test_cyrillic_char_schema_name_filter(self):
        test_case_buf = """--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: Ж_schema; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA "Ж_schema";


ALTER SCHEMA "Ж_schema" OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;

SET search_path = "Ж_schema", pg_catalog;

--
-- Name: Ж; Type: TABLE; Schema: Ж_schema; Owner: user_role_b; Tablespace:
--

CREATE TABLE "Ж" (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE "Ж_schema"."Ж" OWNER TO user_role_b;

SET search_path = "Ж_schema", pg_catalog;

--
-- Data for Name: Ж; Type: TABLE DATA; Schema: Ж_schema; Owner: user_role_b
--

COPY "Ж" (a, b) FROM stdin;
\.


--
-- Name: public; Type: ACL; Schema: -; Owner: dcddev
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM dcddev;
GRANT ALL ON SCHEMA public TO dcddev;
GRANT ALL ON SCHEMA public TO PUBLIC;

--
-- Name: Ж_schema; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA "Ж_schema" FROM PUBLIC;
REVOKE ALL ON SCHEMA "Ж_schema" FROM user_role_a;
GRANT ALL ON SCHEMA "Ж_schema" TO user_role_a;

SET search_path = "Ж_schema", pg_catalog;

--
-- Name: Ж; Type: ACL; Schema: Ж_schema; Owner: user_role_b
--

REVOKE ALL ON TABLE "Ж" FROM PUBLIC;
REVOKE ALL ON TABLE "Ж" FROM user_role_b;
GRANT ALL ON TABLE "Ж" TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""
        dump_schemas = ['Ж_schema']
        dump_tables = [('Ж_schema', 'Ж')]
        arguments = Arguments(dump_schemas, dump_tables)

        infile = '/tmp/test_schema.in'
        outfile = '/tmp/test_schema.out'
        with open(infile, 'w') as fd:
            fd.write(test_case_buf)

        with open(infile, 'r') as fdin:
            with open(outfile, 'w') as fdout:
                process_schema(arguments, fdin, fdout)

        expected_out = """SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: Ж_schema; Type: SCHEMA; Schema: -; Owner: user_role_a
--

CREATE SCHEMA "Ж_schema";


ALTER SCHEMA "Ж_schema" OWNER TO user_role_a;

--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: dcddev
--

CREATE PROCEDURAL LANGUAGE plpgsql;
ALTER FUNCTION plpgsql_call_handler() OWNER TO dcddev;
ALTER FUNCTION plpgsql_validator(oid) OWNER TO dcddev;

SET search_path = "Ж_schema", pg_catalog;

--
-- Name: Ж; Type: TABLE; Schema: Ж_schema; Owner: user_role_b; Tablespace:
--

CREATE TABLE "Ж" (
    a character(1) NOT NULL,
    b character(60)
) DISTRIBUTED BY (a);


ALTER TABLE "Ж_schema"."Ж" OWNER TO user_role_b;

SET search_path = "Ж_schema", pg_catalog;

--
-- Data for Name: Ж; Type: TABLE DATA; Schema: Ж_schema; Owner: user_role_b
--

COPY "Ж" (a, b) FROM stdin;
\.


--
-- Name: Ж_schema; Type: ACL; Schema: -; Owner: user_role_a
--

REVOKE ALL ON SCHEMA "Ж_schema" FROM PUBLIC;
REVOKE ALL ON SCHEMA "Ж_schema" FROM user_role_a;
GRANT ALL ON SCHEMA "Ж_schema" TO user_role_a;

SET search_path = "Ж_schema", pg_catalog;

--
-- Name: Ж; Type: ACL; Schema: Ж_schema; Owner: user_role_b
--

REVOKE ALL ON TABLE "Ж" FROM PUBLIC;
REVOKE ALL ON TABLE "Ж" FROM user_role_b;
GRANT ALL ON TABLE "Ж" TO user_role_b;


--
-- Greenplum Database database dump complete
--
"""

        with open(outfile, 'r') as fd:
            results = fd.read()
        self.prettyAssertEquals(results, expected_out)

