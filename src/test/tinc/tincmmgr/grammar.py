"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import re

import tinctest

from tincmmgr import TINCMMException

from modgrammar import *

class Metadata (Grammar):
    """
    Defines the grammar for metadata key
    """
    grammar = WORD("A-Z0-9a-z._-")

    def elem_init(self, sessiondata=None):
        self.metadata = self.string.strip()

class SelectOperator(Grammar):
    """
    Defines the grammar for a select operator
    """
    grammar = LITERAL("select") | LITERAL("SELECT") | LITERAL("Select")

    def elem_init(self, sessiondata=None):
        self.op = 'SELECT'

class UpdateOperator(Grammar):
    """
    Defines the operator for an update operator in the query
    """
    grammar = LITERAL("update") | LITERAL("UPDATE") | LITERAL("Update")
    
    def elem_init(self, sessiondata=None):
        self.op = 'UPDATE'


class DeleteOperator(Grammar):
    """
    Defines the grammar for a delete operator in the query
    """
    grammar = LITERAL("delete") | LITERAL("DELETE") | LITERAL("Delete")

class InsertOperator(Grammar):
    """
    Defines the grammar for an insert operator in the query
    """
    grammar = LITERAL("insert") | LITERAL("INSERT") | LITERAL("Insert")
        
class InsertQuery(Grammar):
    """
    Defines the grammar for an insert query. Supported syntax is:
    insert metadata = value
    """
    grammar = (InsertOperator, Metadata, L('='), REST_OF_LINE)

    def elem_init(self, sessiondata=None):
        """
        Override this method to perform validation and initialization
        after an InsertQuery is parsed
        """
        value = self.elements[3].string.strip(" '\"")
        metadata = self.elements[1].string.strip(" '\"")
        # Verify if a value is provided
        if not value:
            raise TINCMMException("Invalid syntax for an insert query. Value not specified: %s" %self.string)
        # Verify if a metadata is provided
        if not metadata:
            raise TINCMMException("Invalid syntax for an insert query. Metadata not specified: %s" %self.string)

        self.metadata = metadata
        self.value = value

class UpdateQuery(Grammar):
    """
    Defines the grammar for an update query. Supported syntax is:
    update metadata = value
    """
    grammar = (UpdateOperator, Metadata, L('='), REST_OF_LINE)

    def elem_init(self, sessiondata=None):
        """
        Override this method to perform validation and initialization
        after an UpdateQuery is parsed
        """
        value = self.elements[3].string.strip(" '\"")
        metadata = self.elements[1].string.strip(" '\"")
        # Verify if a value is provided
        if not value:
            raise TINCMMException("Invalid syntax for an update query. Value not specified: %s" %self.string)
        # Verify if a metadata is provided
        if not value:
            raise TINCMMException("Invalid syntax for an update query. Metadata not specified: %s" %self.string)

        self.metadata = metadata
        self.value = value

class SelectQuery(Grammar):
    """
    Defines the grammar for a select query. Supported syntax is:
    select * or select <list of metadata separated by comma>
    """
    grammar = (SelectOperator, Metadata)

    def elem_init(self, sessiondata=None):
        """
        Override this method to perform validation and initialization
        after a SelectQuery is parsed
        """
        metadata = self.elements[1].string.strip(" '\"")

        if not metadata:
            raise TINCMMException("Invalid syntax for select query. Metadata not specified: %s" %self.string)

        self.metadata = metadata
        self.value = None
        
class DeleteQuery(Grammar):
    """
    Defines the grammar for a delete query. Supported syntax is:
    delete metadata <=value>
    """
    grammar = (DeleteOperator, Metadata, OPTIONAL(L('='), REST_OF_LINE))

    def elem_init(self, sessiondata=None):
        """
        Override this method to perform validation and initialization
        after a DeleteQuery is parsed
        """
        metadata = self.elements[1].string.strip(" '\"")
        # Verify if a metadata is provided
        if not metadata:
            raise TINCMMException("Invalid syntax for a delete query. Metadata not specified: %s" %self.string)

        self.metadata = metadata
        self.value = None
        
        # Verify if a value is provided.
        # Value is optional because it might be required only for certain kind of metadata
        # (like tags , product_version).
        # Note: delete metadata=vale will be supported only for tags and product_version
        # Hardcoding the check here as we do not have a metadata language at this point
        # to make this metadata agnostic. In an ideal world every metadata should have a type
        # and this should be allowed for sequence metadata types
        if len(self.elements) > 2 and self.elements[2]:
            # Verify if a value is provided , value will be the second element in the optional grammar
            if not len(self.elements[2].elements) == 2:
                raise TINCMMException("Invalid syntax for a delete query: %s" %self.string)
            if not self.elements[2].elements[1].string.strip(" '\""):
                raise TINCMMException("Invalid syntax for a delete query. Value not specified: %s" %self.string)
            if not (self.metadata == 'tags' or self.metadata == 'product_version'):
                raise TINCMMException("Invalid syntax for a delete query. Value should be specified only for tags and product_version: %s" %self.string)
            self.value = self.elements[2].elements[1].string.strip(" '\"")

class TINCMMQueryGrammar(Grammar):
    """
    Defines the grammar for a tinc mm query
    """
    grammar = (SelectQuery | UpdateQuery | InsertQuery | DeleteQuery)

    def elem_init(self, sessiondata=None):

        self.query_kind = ''
        if isinstance(self.elements[0], SelectQuery):
            self.query_kind = 'select'
        elif isinstance(self.elements[0], InsertQuery):
            self.query_kind = 'insert'
        elif isinstance(self.elements[0], UpdateQuery):
            self.query_kind = 'update'
        elif isinstance(self.elements[0], DeleteQuery):
            self.query_kind = 'delete'
        else:
            raise TINCMMException("Invalid query specified: %s" %self.string)

        self.metadata = self.elements[0].metadata
        self.value = self.elements[0].value

class TINCMMQueryHandler(object):
    """
    The class that is responsible for handling tincmm queries. TINCMM queries are provided by
    the user which is a simple select, insert, update, delete query on a metadata or a list of metadata.
    An example of a tincmm query would be:
    select metadata
    insert metadata=value
    update metadata=value
    delete metadata
    """

    def __init__(self, queries):
        """
        @param queries: An iterable of query strings
        @type queries: list or any iterable of query strings or a single query string
        """
        self.query_list = []
        self.query_result_list = []
        if isinstance(queries, basestring):
            self.query_list.append(queries.strip())
        else:
            try:
                for query in queries:
                    self.query_list.append(query.strip())
            except TypeError, e:
                tinctest.logger.exception("Argument 'queries' should be a string or an iterable")
                raise TINCMMException("Argument 'queries' should be a string or an iterable")

        self._validate_queries()

    def _validate_queries(self):
        for query in self.query_list:
            self.query_result_list.append(self._parse_query(query))

    def _parse_query(self, query):
        """
        Parse the given query string using TINCMM query grammar and return the result
        """
        result = None
        query_parser = TINCMMQueryGrammar.parser()
        try:
            result = query_parser.parse_string(query, reset=True, eof=True, matchtype='longest')
        except ParseError,e:
            tinctest.logger.exception("Error while parsing query %s" %query)
            raise TINCMMException("Invalid query %s. Check the syntax of the query." %query)

        if query_parser.remainder():
            tinctest.logger.error("No complete match found while parsing query %s: Matched text: %s Remainder: %s" %(query,
                                                                                                                     result,
                                                                                                                     query_parser.remainder()))
            raise TINCMMException("Error while parsing query %s" %query)
        
        if not result:
            tinctest.logger.error("No result obtained from parsing the query: %s" %query)
            raise TINCMMException("Error while parsing query %s" %query)

        return result

if __name__ == '__main__':
    parser = SelectQuery.parser()
    parser.parse_string("select metadata1,metadata2;")
