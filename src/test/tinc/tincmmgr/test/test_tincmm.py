import os

import unittest2 as unittest
from collections import defaultdict

from tincmmgr.tincmm import PythonFileHandler, SQLFileHandler

from mpp.models import MPPTestCase, SQLTestCase

class PythonFileHandlerTests(unittest.TestCase):

    def test_parser(self):
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        # Check that docstring is not empy
        self.assertTrue(test_file_handle.docstring_tuples)
        # sample_tests.py has many classes and methods:
        # (wd) means with docstring ; (nd) means no docstring ; (ntm) means not test method ; (ntc) means not test class
        
        # SampleMPPTests (wd): test_1 (nd), test_2 (wd), test_3 (wd)
        # SampleSQLTests (wd): test_explicit_1 (nd), test_explicit_2 (wd)
        # SampleTestsNoDocstring (nd): test_no_1 (nd), test_no_2 (nd), test_no_3 (ntm)
        # stray_def: Stray definition that is not test method and that is not part of any test class.
        # StrayClass (ntc): test_stray_1 (ntm), test_stray_2 (ntm)
        # SampleTestsAfterStray (wd): test_last_1 (wd), test_last_2 (wd)
        
        
        # So there should be 5 method docstrings and 3 classes docstring = 8 valid docstrings
        # And then there 4 test methods without docstring and 1 test class without docstring = 5 valid test method/class with no docstring
        self.assertEqual(len(test_file_handle.docstring_tuples), 13)
        
        docstring_present_count = 0
        docstring_absent_count = 0
        for my_tuple in test_file_handle.docstring_tuples:
            if my_tuple.original_docstring:
                docstring_present_count += 1
            else:
                docstring_absent_count += 1
        
        self.assertEquals(docstring_present_count, 8)
        self.assertEquals(docstring_absent_count, 5)
    
    def test_get_metadata_dictionary(self):
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        # Check that docstring is not empy
        self.assertTrue(test_file_handle.docstring_tuples)
        
        # Let's check each docstring metadata
        # All dictionaries will always have __changed__ key
        
        # SampleMPPTests only has @tags class1
        my_dict = test_file_handle.get_metadata_dictionary(class_name = 'SampleMPPTests', method_name = None)
        self.assertEqual(len(my_dict), 2)
        self.assertEqual(my_dict['tags'], "class1")
        
        # SampleMPPTests' test_1 should be empty.
        my_dict = test_file_handle.get_metadata_dictionary(class_name = 'SampleMPPTests', method_name = 'test_1')
        self.assertEqual(len(my_dict), 1)
        
        # SampleMPPTests' test_2 should only have @tags test2.
        my_dict = test_file_handle.get_metadata_dictionary(class_name = 'SampleMPPTests', method_name = 'test_2')
        self.assertEqual(len(my_dict), 2)
        self.assertEqual(my_dict['tags'], "test2")
        
        # SampleMPPTests' test_3 should only have @product_version prod1:
        my_dict = test_file_handle.get_metadata_dictionary(class_name = 'SampleMPPTests', method_name = 'test_3')
        self.assertEqual(len(my_dict), 2)
        self.assertEqual(my_dict['product_version'], "prod1:")
        
        # SampleSQLTests has @tags sql smoke, and @author sql_author
        my_dict = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = None)
        self.assertEqual(len(my_dict), 3)
        self.assertEqual(my_dict['tags'], "sql smoke")
        self.assertEqual(my_dict['author'], "sql_author")
        
        # SampleSQLTests' test_explicit_1 has nothing
        my_dict = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = 'test_explicit_1')
        self.assertEqual(len(my_dict), 1)
        
        # SampleSQLTests' test_explicit_2 has @tags explicit
        my_dict = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = 'test_explicit_2')
        self.assertEqual(len(my_dict), 2)
        self.assertEqual(my_dict['tags'], "explicit")
        
    def test_generate_new_docstring(self):
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = 'test_explicit_2')
        metadata_dictionary['__changed__'] = True
        
        original_docstring = ''
        for my_tuple in test_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleSQLTests' and my_tuple.method_name == 'test_explicit_2':
                original_docstring = my_tuple.original_docstring
                offset = my_tuple.offset
                break
        
        new_docstring = test_file_handle.generate_new_docstring(original_docstring, metadata_dictionary, offset)
        self.assertEquals(new_docstring.strip('\n'), original_docstring)
        
        # Original tags is explicit. Change it to blah
        metadata_dictionary['tags'] = "blah"
        new_docstring = test_file_handle.generate_new_docstring(original_docstring, metadata_dictionary, offset)
        self.assertTrue("@tags blah" in new_docstring)
        self.assertTrue("@tags test_explicit_2" not in new_docstring)
        
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary['author'] = 'mel'
        new_docstring = test_file_handle.generate_new_docstring(original_docstring, metadata_dictionary, offset)
        self.assertTrue("@author mel" in new_docstring)
    
    def test_generate_new_docstring_complicated(self):
        # Work with more complicated docstring: SampleSQLTests
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = None)
        metadata_dictionary['__changed__'] = True
        
        original_docstring = ''
        for my_tuple in test_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleSQLTests' and my_tuple.method_name == None:
                original_docstring = my_tuple.original_docstring
                offset = my_tuple.offset
                break
        
        new_docstring = test_file_handle.generate_new_docstring(original_docstring, metadata_dictionary, offset)
        self.assertEquals(new_docstring.strip('\n'), original_docstring)
        
        # Original author is sql_author. Change it to blah
        metadata_dictionary['author'] = "blah"
        new_docstring = test_file_handle.generate_new_docstring(original_docstring, metadata_dictionary, offset)
        self.assertTrue("@author blah" in new_docstring)
        self.assertTrue("@author sql_author" not in new_docstring)
        
        # Verify that the comment is still there
        self.assertTrue("Comment here" in new_docstring)
        self.assertTrue("Some space above" in new_docstring)
        
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary['bugs'] = 'mpp-1'
        new_docstring = test_file_handle.generate_new_docstring(original_docstring, metadata_dictionary, offset)
        self.assertTrue("@bugs mpp-1" in new_docstring)
    
    def test_update_docstring_class(self):
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = None)
        metadata_dictionary['__changed__'] = True
        
        # Original author is sql_author. Change it to bal_author
        metadata_dictionary['author'] = "bal_author"
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary['bugs'] = 'mpp-1'
        
        test_file_handle.update_docstring(class_name = 'SampleSQLTests', method_name = None, metadata_dictionary = metadata_dictionary)
        
        # Go through the docstring_tuples and find our tuple
        tuple_found = None
        for my_tuple in test_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleSQLTests' and my_tuple.method_name == None:
                tuple_found = my_tuple
                break
        
        self.assertTrue(tuple_found is not None)
        self.assertTrue("@author bal_author" in tuple_found.new_docstring)
        self.assertTrue("@author sql_author" not in tuple_found.new_docstring)
        
        # Verify that the comment is still there
        self.assertTrue("Comment here" in tuple_found.new_docstring)
        self.assertTrue("Some space above" in tuple_found.new_docstring)
        
        # Verify that new metadata got added with 4 spaces
        self.assertTrue("    @bugs mpp-1" in tuple_found.new_docstring)
    
    def test_update_docstring_method(self):
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = "test_explicit_2")
        metadata_dictionary['__changed__'] = True
        
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary['bugs'] = 'mpp-2'
        
        test_file_handle.update_docstring(class_name = 'SampleSQLTests', method_name = 'test_explicit_2', metadata_dictionary = metadata_dictionary)
        
        # Go through the docstring_tuples and find our tuple
        tuple_found = None
        for my_tuple in test_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleSQLTests' and my_tuple.method_name == 'test_explicit_2':
                tuple_found = my_tuple
                break
        
        self.assertTrue(tuple_found is not None)
        # Verify that new metadata got added with 8 spaces for method
        self.assertTrue("        @bugs mpp-2" in tuple_found.new_docstring)
    
    def test_update_docstring_no_original_class(self):
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary = test_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = None)
        metadata_dictionary['__changed__'] = True
        
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary['bugs'] = 'mpp-3'
        metadata_dictionary['tags'] = 'smoke'
        
        test_file_handle.update_docstring(class_name = 'SampleTestsNoDocstring', method_name = None, metadata_dictionary = metadata_dictionary)
        
        # Go through the docstring_tuples and find our tuple
        tuple_found = None
        for my_tuple in test_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleTestsNoDocstring' and my_tuple.method_name == None:
                tuple_found = my_tuple
                break
        
        self.assertTrue(tuple_found is not None)
        # Verify that new metadata got added with 4 spaces for class
        self.assertTrue("    @bugs mpp-3" in tuple_found.new_docstring)
        self.assertTrue("    @tags smoke" in tuple_found.new_docstring)
        self.assertTrue(tuple_found.original_docstring is None)
    
    def test_update_docstring_no_original_method(self):
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary = test_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = 'test_no_1')
        metadata_dictionary['__changed__'] = True
        
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary['bugs'] = 'mpp-4'
        metadata_dictionary['tags'] = 'smokey'
        
        test_file_handle.update_docstring(class_name = 'SampleTestsNoDocstring', method_name = 'test_no_1', metadata_dictionary = metadata_dictionary)
        
        # Go through the docstring_tuples and find our tuple
        tuple_found = None
        for my_tuple in test_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleTestsNoDocstring' and my_tuple.method_name == 'test_no_1':
                tuple_found = my_tuple
                break
        
        self.assertTrue(tuple_found is not None)
        # Verify that new metadata got added with 4 spaces for class
        self.assertTrue("        @bugs mpp-4" in tuple_found.new_docstring)
        self.assertTrue("        @tags smokey" in tuple_found.new_docstring)
        self.assertTrue(tuple_found.original_docstring is None)
    
    def test_update_file_existing_docstring(self):
        
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary_class = test_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = None)
        metadata_dictionary_class['__changed__'] = True
        
        # Original author is sql_author. Change it to bal_author
        metadata_dictionary_class['author'] = "bal_author"
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary_class['bugs'] = 'mpp-1'
        
        # Modify method one as well
        metadata_dictionary_method = test_file_handle.get_metadata_dictionary(class_name = 'SampleMPPTests', method_name = 'test_3')
        metadata_dictionary_method['__changed__'] = True
        
        # Original product_version is prod1:. Delete it.
        metadata_dictionary_method.pop('product_version')
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary_method['tags'] = 'smoke'
        
        new_file = os.path.join(os.path.dirname(__file__), 'sample_tests_new.py')
        test_file_handle.update_docstring(class_name = 'SampleSQLTests', method_name = None, metadata_dictionary = metadata_dictionary_class)
        test_file_handle.update_docstring(class_name = 'SampleMPPTests', method_name = 'test_3', metadata_dictionary = metadata_dictionary_method)
        test_file_handle.update_file(new_file)
        
        # Verify that new file exists
        self.assertTrue(os.path.exists(new_file))
        # Now, get the docstring from new file
        new_file_handle = PythonFileHandler(new_file)
        
        new_class_tuple = None
        new_method_tuple = None
        for my_tuple in new_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleSQLTests' and my_tuple.method_name == None:
                new_class_tuple = my_tuple
            if my_tuple.class_name == 'SampleMPPTests' and my_tuple.method_name == 'test_3':
                new_method_tuple = my_tuple
        
        self.assertTrue(new_class_tuple is not None)
        self.assertTrue(new_method_tuple is not None)
        
        # Verify original docstring of new_file
        self.assertTrue("    Comment here" in new_class_tuple.original_docstring)
        self.assertTrue("    @author bal_author" in new_class_tuple.original_docstring)
        self.assertTrue("    @bugs mpp-1" in new_class_tuple.original_docstring)
        self.assertTrue("        @tags smoke" in new_method_tuple.original_docstring)
        self.assertTrue("@product_version" not in new_method_tuple.original_docstring)
        
        # Verify that update_file in-place works
        new_class_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = None)
        new_method_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleMPPTests', method_name = 'test_3')
        new_class_dict['__changed__'] = True
        new_method_dict['__changed__'] = True
        
        new_class_dict.pop('author')
        new_class_dict.pop('tags')
        new_class_dict.pop('bugs')
        new_method_dict.pop('tags')
        
        new_file_handle.update_docstring(class_name = 'SampleSQLTests', method_name = None, metadata_dictionary = new_class_dict)
        new_file_handle.update_docstring(class_name = 'SampleMPPTests', method_name = 'test_3', metadata_dictionary = new_method_dict)
        new_file_handle.update_file()
        
        new_file_handle = PythonFileHandler(new_file)
        new_class_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleSQLTests', method_name = None)
        new_method_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleMPPTests', method_name = 'test_3')
        # Should have no keys (except for __changed__)
        self.assertEqual(len(new_class_dict), 1)
        self.assertEqual(len(new_method_dict), 1)
        
        os.remove(new_file)
    
    @unittest.skip("Failing for now...")
    def test_update_file_no_docstring(self):
        
        test_file_handle = PythonFileHandler(os.path.join(os.path.dirname(__file__), 'sample_tests.py'))
        metadata_dictionary_class = test_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = None)
        metadata_dictionary_class['__changed__'] = True
        
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary_class['bugs'] = 'mpp-1'
        
        # Modify method one as well
        metadata_dictionary_method = test_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = 'test_no_1')
        metadata_dictionary_method['__changed__'] = True
        
        # Add something to metadata_dictionary that wasn't there before
        metadata_dictionary_method['tags'] = 'smoke'
        
        new_file = os.path.join(os.path.dirname(__file__), 'sample_tests_new.py')
        test_file_handle.update_docstring(class_name = 'SampleTestsNoDocstring', method_name = None, metadata_dictionary = metadata_dictionary_class)
        test_file_handle.update_docstring(class_name = 'SampleTestsNoDocstring', method_name = 'test_no_1', metadata_dictionary = metadata_dictionary_method)
        test_file_handle.update_file(new_file)
        
        # Verify that new file exists
        self.assertTrue(os.path.exists(new_file))
        # Now, get the docstring from new file
        new_file_handle = PythonFileHandler(new_file)
        
        new_class_tuple = None
        new_method_tuple = None
        for my_tuple in new_file_handle.docstring_tuples:
            if my_tuple.class_name == 'SampleTestsNoDocstring' and my_tuple.method_name == None:
                new_class_tuple = my_tuple
            if my_tuple.class_name == 'SampleTestsNoDocstring' and my_tuple.method_name == 'test_no_1':
                new_method_tuple = my_tuple
        
        self.assertTrue(new_class_tuple is not None)
        self.assertTrue(new_method_tuple is not None)
        
        # Verify original docstring of new_file
        self.assertTrue(new_class_tuple.original_docstring is not None)
        self.assertTrue(new_method_tuple.original_docstring is not None)
        self.assertTrue("    @bugs mpp-1" in new_class_tuple.original_docstring)
        self.assertTrue("        @tags smoke" in new_method_tuple.original_docstring)
        
        # Verify that update_file in-place works
        new_class_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = None)
        new_method_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = 'test_no_1')
        new_class_dict['__changed__'] = True
        new_method_dict['__changed__'] = True
        
        new_class_dict.pop('bugs')
        new_method_dict.pop('tags')
        
        new_file_handle.update_docstring(class_name = 'SampleTestsNoDocstring', method_name = None, metadata_dictionary = new_class_dict)
        new_file_handle.update_docstring(class_name = 'SampleTestsNoDocstring', method_name = 'test_no_1', metadata_dictionary = new_method_dict)
        new_file_handle.update_file()
        
        new_file_handle = PythonFileHandler(new_file)
        new_class_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = None)
        new_method_dict = new_file_handle.get_metadata_dictionary(class_name = 'SampleTestsNoDocstring', method_name = 'test_no_1')
        # Should have no keys (except for __changed__)
        self.assertEqual(len(new_class_dict), 1)
        self.assertEqual(len(new_method_dict), 1)
        
        os.remove(new_file)

class SQLFileHandlerTests(unittest.TestCase):

    def test_parser(self):
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query01.sql'))

        # Check that docstring is not empty
        self.assertTrue(test_file_handle.original_docstring)
        
        # Verify that the original docstring is correct
        self.assertTrue("-- @tags tag1 tag2 tag3 bug-3" in test_file_handle.original_docstring)
        self.assertTrue("-- comment" in test_file_handle.original_docstring)
        self.assertTrue("--@bugs MPP-1" in test_file_handle.original_docstring)
        
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query02.sql'))
        # Check that docstring is not empty
        self.assertTrue(test_file_handle.original_docstring)
        
        # Verify that the original docstring is correct
        self.assertTrue("-- @product_version gpdb:" in test_file_handle.original_docstring)
        
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query_nodocstring.sql'))
        # Check that docstring is empty
        self.assertTrue(test_file_handle.original_docstring is "")
    
    def test_get_metadata_dictionary(self):
        
        # All dictionaries will always have __changed__ key
        
        # query01 has tags and bugs
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query01.sql'))
        self.assertTrue(test_file_handle.original_docstring)
        my_dict = test_file_handle.get_metadata_dictionary()
        self.assertEqual(len(my_dict), 3)
        self.assertEqual(my_dict['tags'], "tag1 tag2 tag3 bug-3")
        self.assertEqual(my_dict['bugs'], "MPP-1")
        
        # query02 has product_version
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query02.sql'))
        self.assertTrue(test_file_handle.original_docstring)
        my_dict = test_file_handle.get_metadata_dictionary()
        self.assertEqual(len(my_dict), 2)
        self.assertEqual(my_dict['product_version'], "gpdb:")
        
        # query_nodocstring has nothing
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query_nodocstring.sql'))
        self.assertTrue(test_file_handle.original_docstring == "")
        my_dict = test_file_handle.get_metadata_dictionary()
        self.assertEqual(len(my_dict), 1)
    
    def test_update_docstring_existing(self):
        
        # Query01 (scenario that has some docstring)
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query01.sql'))
        self.assertTrue(test_file_handle.original_docstring)
        my_dict = test_file_handle.get_metadata_dictionary()
        my_dict['__changed__'] = True
        
        # Change original tags to smoke
        my_dict['tags'] = 'smoke'
        # Remove bugs
        my_dict.pop('bugs')
        # Add author
        my_dict['author'] = 'blah'
        
        test_file_handle.update_docstring(my_dict)
        
        # Verify that new_docstring exists
        self.assertTrue(test_file_handle.new_docstring)
        # Verify that tags is updated
        self.assertTrue("@tags smoke" in test_file_handle.new_docstring)
        # Verify that bugs is removed
        self.assertTrue('@bugs' not in test_file_handle.new_docstring)
        # Verify that author is added
        self.assertTrue('@author blah' in test_file_handle.new_docstring)
        # Verify that the comment is still there
        self.assertTrue('-- comment' in test_file_handle.new_docstring)
    
    def test_update_docstring_new(self):
        
        # Query01 (scenario that has some docstring)
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query_nodocstring.sql'))
        self.assertTrue(test_file_handle.original_docstring == "")
        my_dict = test_file_handle.get_metadata_dictionary()
        my_dict['__changed__'] = True
        
        # Add tags 
        my_dict['tags'] = 'smoke'
        # Add author
        my_dict['author'] = 'blah'
        
        test_file_handle.update_docstring(my_dict)
        
        # Verify that new_docstring exists
        self.assertTrue(test_file_handle.new_docstring)
        # Verify that tags is added
        self.assertTrue("@tags smoke" in test_file_handle.new_docstring)
        # Verify that author is added
        self.assertTrue('@author blah' in test_file_handle.new_docstring)
    
    def test_update_file_existing_docstring(self):
        # Query01 (scenario that has some docstring)
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query01.sql'))
        self.assertTrue(test_file_handle.original_docstring)
        my_dict = test_file_handle.get_metadata_dictionary()
        my_dict['__changed__'] = True
        
        # Change original tags to smoke
        my_dict['tags'] = 'smoke'
        # Remove bugs
        my_dict.pop('bugs')
        # Add author
        my_dict['author'] = 'blah'
        
        new_file = os.path.join(os.path.dirname(__file__), 'sql', 'query01_new.sql')
        test_file_handle.update_docstring(my_dict)
        test_file_handle.update_file(new_file)
        
        # Verify that new file exists
        self.assertTrue(os.path.exists(new_file))
        # Now, get the metadata dictionary from new file
        new_file_handle = SQLFileHandler(new_file)
        self.assertTrue(new_file_handle.original_docstring)
        new_dict = new_file_handle.get_metadata_dictionary()
        # Verify the updated metadata in new_dict's original_docstring
        # Verify that tags is updated
        self.assertTrue("@tags smoke" in new_file_handle.original_docstring)
        # Verify that bugs is removed
        self.assertTrue('@bugs' not in new_file_handle.original_docstring)
        # Verify that author is added
        self.assertTrue('@author blah' in new_file_handle.original_docstring)
        # Verify that the comment is still there
        self.assertTrue('-- comment' in new_file_handle.original_docstring)
        
        # Verify that all metadata can be removed and update_file in-place works
        new_dict['__changed__'] = True
        new_dict.pop('tags')
        new_dict.pop('author')
        
        new_file_handle.update_docstring(new_dict)
        new_file_handle.update_file()
        
        # Get the file content
        new_file_content = None
        with open(new_file, "r") as new_file_object:
            new_file_content = new_file_object.read().replace('\n', '')
        
        self.assertTrue(new_file_content is not None)
        self.assertTrue('tags' not in new_file_content)
        self.assertTrue('author' not in new_file_content)
        self.assertTrue('-- comment' in new_file_content)
        
        os.remove(new_file)
    
    def test_update_file_no_docstring(self):
        # Query01 (scenario that has some docstring)
        test_file_handle = SQLFileHandler(os.path.join(os.path.dirname(__file__), 'sql', 'query_nodocstring.sql'))
        self.assertTrue(test_file_handle.original_docstring == "")
        my_dict = test_file_handle.get_metadata_dictionary()
        my_dict['__changed__'] = True
        
        # Add tags 
        my_dict['tags'] = 'smoke'
        # Add author
        my_dict['author'] = 'blah'
        
        new_file = os.path.join(os.path.dirname(__file__), 'sql', 'query_nodocstring_new.sql')
        test_file_handle.update_docstring(my_dict)
        test_file_handle.update_file(new_file)
        
        # Verify that new file exists
        self.assertTrue(os.path.exists(new_file))
        # Now, get the metadata dictionary from new file
        new_file_handle = SQLFileHandler(new_file)
        self.assertTrue(new_file_handle.original_docstring)
        new_dict = new_file_handle.get_metadata_dictionary()
        # Verify the new metadata in new_dict's original_docstring
        # Verify that tags is added
        self.assertTrue("@tags smoke" in new_file_handle.original_docstring)
        # Verify that author is added
        self.assertTrue('@author blah' in new_file_handle.original_docstring)
        
        # Verify that all metadata can be removed and update_file in-place works
        new_dict['__changed__'] = True
        new_dict.pop('tags')
        new_dict.pop('author')
        
        new_file_handle.update_docstring(new_dict)
        new_file_handle.update_file()
        
        # Get the file content
        new_file_content = None
        with open(new_file, "r") as new_file_object:
            new_file_content = new_file_object.read().replace('\n', '')
        
        self.assertTrue(new_file_content is not None)
        self.assertTrue('tags' not in new_file_content)
        self.assertTrue('author' not in new_file_content)
        self.assertTrue('--' not in new_file_content)
        
        os.remove(new_file)

