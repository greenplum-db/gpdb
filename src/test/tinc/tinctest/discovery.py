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


from modgrammar import *

__all__ = ['TINCDiscoveryQueryHandler', 'TINCDiscoveryException']

_module_keyword = 'module'
_method_keyword = 'method'
_package_keyword = 'package'
_class_keyword = 'class'

# A dict of keywords in the grammar refering to the attributes in TINCTestCase
_keyword_attribute_dict = {_module_keyword : 'module_name', _method_keyword : 'method_name',
                           _package_keyword : 'package_name', _class_keyword : 'class_name' }

class TINCDiscoveryException(Exception):
    """
    The excpetion that is thrown when there is an error while evaluating tinc queries
    during discovery.
    """
    pass

class PredicateKeywordAttribute (Grammar):
    """
    Defines the grammar for keyword search attributes. (method, module, package, class)
    """
    grammar = (LITERAL(_module_keyword) | LITERAL(_method_keyword) | LITERAL(_package_keyword) | LITERAL(_class_keyword))

class PredicateMetadataAttribute (Grammar):
    """
    Defines the grammar for metadata search attributes
    """
    grammar = WORD("A-Z0-9a-z._")

class PredicateOperator (Grammar):
    """
    Operators that we support for predicates. (=, !=)
    """
    grammar = (LITERAL("=") | LITERAL("!="))
    
class Predicate (Grammar):
    """
    Defines the grammar for a predicate.
    Eg: module = test*, package=a.b.*, method=test_smoke, class=Test*Smoke*Tests
    """
    grammar = (PredicateKeywordAttribute | PredicateMetadataAttribute, PredicateOperator, OPTIONAL(LITERAL("\"") | LITERAL("'")),
               WORD("*A-Z0-9a-z._-"), OPTIONAL(LITERAL("\"") | LITERAL("'")))

    def elem_init(self, sessiondata=None):
        """
        Override this method to initialize some variables after a Predicate is parsed.
        """
        # Perform some evaluation on quotes provided
        # If an opening quote is provided, there should be a matching closing quote
        if self.elements[2]:
            if not self.elements[4] or (self.elements[2].string != self.elements[4].string):
                raise TINCDiscoveryException("Predicate %s in the query is invalid" % self.string)
            
        # set the keyword, this can either be class,method, module or package or
        # any 'test metadata'
        self._key = self.elements[0].string
        # Operator will either be '=' or '!='
        self._operator = self.elements[1].string
        # This is the value on which the predicate is evaluated.
        # For eg: test = test* 
        self._value = self.elements[3].string.strip()
        # Convert value to a regex and have it pre-compiled
        re_str = "^%s$" %(self._value.replace(".","\.").replace("*", ".*"))
        self._re = re.compile(re_str)
                        
    def eval(self, test):
        """
        Implement logic here to evaluate the predicate by calling out to
        the appropriate apis in TINCTestCase based on the keyword
        """
        if isinstance(self.elements[0], PredicateKeywordAttribute):
            return self._eval_keyword_predicate(test)
        else:
            # Otherwise it is a metadata predicate
            return self._eval_metadata_predicate(test)
                        
    def _eval_keyword_predicate(self, test):
        """
        Evaluates the keyword predicate by looking up the appropriate test instance attributes
        """
        # get the test instance's keyword value
        test_value = getattr(test, _keyword_attribute_dict[self._key])
        predicate_match = self._re.match(test_value)
        if self._operator == '=':
            return True if predicate_match else False
        elif self._operator == '!=':
            return False if predicate_match else True
        else:
            raise TINCDiscoveryException("Invalid operator encountered in the predicate %s" %self.string)

    def _eval_metadata_predicate(self, test):
        """
        Evaluates a metadata predicate by calling test.match_metadata
        """
        match = test.match_metadata(self._key, self._value)
        if self._operator == '=':
            return True if match else False
        elif self._operator == '!=':
            return False if match else True
        else:
            raise TINCDiscoveryException("Invalid operator encountered in the predicate %s" %self.string)        


class AndOperator(Grammar):
    """
    AND Operator supported between predicates.
    """
    grammar = (LITERAL("and") | LITERAL("AND"))

    def elem_init(self, sessiondata=None):
        self.string = self.string.lower()

class OrOperator(Grammar):
    """
    OR Operator supported between predicates.
    """
    grammar = (LITERAL("or") | LITERAL("OR"))

    def elem_init(self, sessiondata=None):
        self.string = self.string.lower()

        
class Subquery(Grammar):
    """
    Grammar that encapsulates a subquery within a pair of parenthesis
    """
    # Forward reference to 'Query' grammar
    grammar = (L('('), REF('Query'), L(')'))

    def eval(self, test):
        return self.elements[1].eval(test)

class FirstConstruct (Grammar):
    """
    Grammar that encapsulates the first construct that will be evaluated.
    This will either be a subquery or a predicate. Note that subquery will
    be matched first which will give precedence for subqueries to be evaluated
    first. For a query like "(test=smoke* and class=Test*) or tags=smoke, this
    will make sure we match the subquery 'test=smoke* and class=Test*' first.
    """
    grammar = (Subquery | Predicate)

    def eval(self, test):
        return self.elements[0].eval(test)

class AndExpression (Grammar):
    """
    A grammar that encapsulates and expressions in a query.
    """
    grammar = (FirstConstruct, ONE_OR_MORE(AndOperator, FirstConstruct))

    def eval(self, test):
        result = self.elements[0].eval(test)
        for element in self.elements[1]:
            # TODO: Possibly short-circuit here if result is False. Have to investigate
            result = result and element[1].eval(test)
        return result

class SecondConstruct (Grammar):
    """
    Grammar that encapsulates the second construct that will be evaluated.
    This will either be an and expression,  subquery or a predicate. Note that
    we give precedence to AndExpression here so that it will
    be matched first for evaluation.
    For a query like "(test=smoke* and class=Test*) and tags=smoke, this
    will make sure we match 'test=smoke* and class=Test*'
    """
    grammar = (AndExpression | Subquery | Predicate)

    def eval(self, test):
        return self.elements[0].eval(test)

class OrExpression (Grammar):
    """
    A grammar that encapsulates and expressions in a query.
    """
    grammar = (SecondConstruct, ONE_OR_MORE(OrOperator, SecondConstruct))

    def eval(self, test):
        result = self.elements[0].eval(test)
        for element in self.elements[1]:
            # TODO: Possibly short-circuit here if result is True. Have to investigate
            result = result or element[1].eval(test)
        return result

class Query (Grammar):
    """
    Grammar for the final query expression which will just be a list of predicates separated by the operator.
    """
    grammar = (OrExpression | AndExpression | Subquery | Predicate)

    def eval(self, test):
        """
        The method that will be called for every single test.
        Make sure this method is not an obvious performance overhead. 
        """
        tinctest.logger.trace_in("Evaluating query %s for test %s" %(self.string, test.full_name))
        result = self.elements[0].eval(test)
        tinctest.logger.trace_out("Query: %s test: %s result: %s" %(self.string, test, str(result)))
        return result


"""
    All we have to do after this is implement the logic to evaluate each predicate in the query.
    For eg: evaluate class=TestSmoke* , package=storage.uao.* etc and then replace the predicates
    in the query with the result to form a boolean expression that can be evaluated using python -
    That way we can avoid taking care of operator precedence etc since python would take care of it.

    For eg: if the user gives a query class=TestSmoke* and method=test_smoke_* or package=storage.uao.*,
    we will parse that using the above grammar and walk through the result and evaluate each predicate
    and get it's value.

    So the query evaluator will translate the above query to an expression like
    'False and True or False' which python can evaluate for us.

    Even when we support paranthesis , python can take care of the evaluation for us. All we have to
    do is improve our grammar to support paranthesis. 
"""


class TINCDiscoveryQueryHandler(object):
    """
    The class that lets discovery in loader to filter tests based on a set of tinc queries.
    TINC queries are provided by the user which is a simple AND / OR based grammar on
    <package, class, method, module> patterns and other test metadata.
    An example of a TINC query would be:
    -q "module=test_smoke_* OR class=*SmokeTests OR package=storage.uao.* AND method=test*_smoke_*
    Usage: TINCDiscoveryQueryHandler(['class=test*',...]).apply_queries(<test_case_instance>)
    """

    def __init__(self, queries):
        """
        @param queries: An iterable of query strings that should be evaluated with this handler.
        @type queries: list or any iterable of query strings or a single query string
        """
        self.query_list = []
        self.query_result_list = []
        if isinstance(queries, basestring):
            # query = queries.strip(';') + ';'
            self.query_list.append(queries.strip())
        else:
            try:
                for query in queries:
                    # Note - a bug in modgrammar does not recognize repeats
                    # and list_of without a redundant character
                    # For eg: a grammar like LIST_OF(Predicate, sep=",") does not work
                    # However a grammar like (LIST_OF(Predicate, sep=","), ";") works fine
                    # query = query.strip(';') + ';'
                    self.query_list.append(query.strip())
            except TypeError, e:
                tinctest.logger.exception("Argument 'queries' should be an iterable")
                raise TINCDiscoveryException("Argument 'queries' should be a string or an iterable")
        self._validate_queries()

    def _validate_queries(self):
        query_parser = Query.parser()
        for query in self.query_list:
            self.query_result_list.append(self._parse_query(query))

    def _parse_query(self, query):
        """
        Parse the given query string using TINC query grammar and return the result
        """
        result = None
        query_parser = Query.parser()
        try:
            result = query_parser.parse_string(query, reset=True, eof=True, matchtype='longest')
        except ParseError, e:
            tinctest.logger.exception("Error while parsing query %s" %query)
            raise TINCDiscoveryException("Invalid query %s.Check the syntax of the query." %query)
        except TINCDiscoveryException, e:
            raise TINCDiscoveryException("Invalid query %s: %s" %(query, e))

        if query_parser.remainder():
            tinctest.logger.error("No complete match found while parsing query %s: Matched text: %s Remainder: %s" %(query,
                                                                                                                     result,
                                                                                                                     query_parser.remainder()))
            raise TINCDiscoveryException("Error while parsing query %s" %query)
        
        if not result:
            tinctest.logger.error("No result obtained from parsing the query: %s" %query)
            raise TINCDiscoveryException("Error while parsing query %s" %query)

        return result

    def apply_queries(self, test):
        """
        Applies all the queries to the test instance and returns True only if
        the test passes all the queries.
        @type test: L{TINCTestCase}
        @param test: The test instance object on which the queries should be applied

        @rtype: boolean
        @return: True if the test matches all the queries in this handler, False otherwise
        """
        for query, result in zip(self.query_list, self.query_result_list):
            tinctest.logger.debug("Applying query %s to test %s" %(query, test.full_name))
            if not result.eval(test):
                tinctest.logger.debug("Test %s did not match query %s" %(test.full_name, query))
                return False
        return True
