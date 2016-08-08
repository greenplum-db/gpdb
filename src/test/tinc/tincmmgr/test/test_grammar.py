import inspect
import os
import re

from tincmmgr.grammar import TINCMMQueryHandler
from tincmmgr import TINCMMException

import unittest2 as unittest

class TINCMMQueryGrammarTests(unittest.TestCase):

    def test_positive_select_queries(self):
        test_queries = []
        test_queries.append("select metadata")
        handler = TINCMMQueryHandler(test_queries)
        self.assertEquals(len(handler.query_list), len(test_queries))

    def test_negative_select_queries(self):
        test_queries = []
        test_queries.append("select")
        test_queries.append("select !")
        test_queries.append("select metadata remaining text")
        test_queries.append("select *;")
        test_queries.append("select metadata1, metadata2, metadata3")
        for query in test_queries:
            try:
                handler = TINCMMQueryHandler(query)
            except TINCMMException:
                continue
            self.fail("Query %s did not throw an exception" %query)

    def test_positive_insert_queries(self):
        test_queries = []
        test_queries.append("insert metadata=value")
        test_queries.append("insert product_version = 'gpdb: , hawq: '")
        test_queries.append("insert tags='smoke'")
        test_queries.append("insert optimizer_mode=on")
        test_queries.append("insert optimizer_mode  =   off")
        test_queries.append("insert product_version = 'gpdb: [4.2-], hawq: [1.1.0.0-1.2.0.0]'")
        handler = TINCMMQueryHandler(test_queries)
        self.assertEquals(len(handler.query_list), len(test_queries))

    def test_negative_insert_queries(self):
        test_queries = []
        test_queries.append("insert")
        test_queries.append("insert metadata")
        test_queries.append("insert metadata value")
        test_queries.append("insert metadata =")
        for query in test_queries:
            try:
                handler = TINCMMQueryHandler(query)
            except TINCMMException:
                continue
            self.fail("Query %s did not throw an exception" %query)


    def test_positive_update_queries(self):
        test_queries = []
        test_queries.append("update metadata=value")
        test_queries.append("update product_version = 'gpdb: , hawq: '")
        test_queries.append("update tags='smoke'")
        test_queries.append("update optimizer_mode=on")
        test_queries.append("update optimizer_mode  =   off")
        test_queries.append("update product_version = 'gpdb: [4.2-], hawq: [1.1.0.0-1.2.0.0]'")
        handler = TINCMMQueryHandler(test_queries)
        self.assertEquals(len(handler.query_list), len(test_queries))

    def test_negative_update_queries(self):
        test_queries = []
        test_queries.append("update")
        test_queries.append("update metadata")
        test_queries.append("update metadata value")
        test_queries.append("update metadata =")
        for query in test_queries:
            try:
                handler = TINCMMQueryHandler(query)
            except TINCMMException:
                continue
            self.fail("Query %s did not throw an exception" %query)

    def test_positive_delete_queries(self):
        test_queries = []
        test_queries.append("delete metadata")
        test_queries.append("delete product_version=gpdb:")
        test_queries.append("delete product_version='gpdb: 4.2.x, hawq: 1.x'")
        test_queries.append("delete tags=smoke")
        handler = TINCMMQueryHandler(test_queries)
        self.assertEquals(len(handler.query_list), len(test_queries))

    def test_negative_delete_queries(self):
        test_queries = []
        test_queries.append("delete")
        test_queries.append("delete !")
        test_queries.append("delete metadata remaining text")
        test_queries.append("delete *;")
        test_queries.append("delete metadata1, metadata2, metadata3")
        test_queries.append("delete *")
        test_queries.append("delete product_version=")
        test_queries.append("delete tags=")
        test_queries.append("delete some_other_metadata=value")
        for query in test_queries:
            try:
                handler = TINCMMQueryHandler(query)
            except TINCMMException:
                continue
            self.fail("Query %s did not throw an exception" %query)
        

        
        
            
        

    
