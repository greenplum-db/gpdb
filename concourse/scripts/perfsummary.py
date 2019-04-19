#!/usr/bin/env python
#
# Extracts summary info from a log file that compiles and executes test suite
# queries. Creates a CSV (comma-separated values) summary.
#
# Usage:
#
# perfsummary.py [ <log-file-name> ] [ --baseLog <base-log-file-name> ]
#
# Log files must have queries that were executed with timing on.
#
# Log files with more than one query must be in the following format:
#
# Filename: XXXXXXXX
#
# Query Plan
# XXXXXXXXXXXX
#
# Time: XXXX
#
# Filename: XXXX


import sys
import re
import argparse

# the state of multiple test suite queries executed in a log file
class FileState:
    def __init__(self):
        self.reset_curr_query_state()
        self.first_file_name_match = True
        
        # an ordered list of query ids, including secondary queries
        # for queries that are split into multiple selects
        self.query_id_list = []
        
        # explain/planning time for each query
        self.query_explain_time_map = {}
        
        # the explain plan for each query (as a list of strings)
        self.query_explain_plan_map = {}
        
        # execution time for each query
        self.query_exe_time_map = {}
        
        # comments for each query
        self.query_comment_map = {}

    # reset the state relating to the current query
    def reset_curr_query_state(self):
        self.curr_query_id = ""
        self.planning_time1 = -1
        self.planning_time2 = -1
        self.exe_time1 = -1
        self.exe_time2 = -1
        # current plan, as it is assembled from multiple lines
        self.curr_plan = []
        # list of plans, one for each explain done in a query section
        self.plans = []
        self.comment1 = ""
        self.comment2 = ""
        self.exe_phase = False
        self.explain_phase = False
        self.plan_phase = False
        self.second_query = False

    # record query id and measurements for a single query,
    # called several times with different seq_nums for multi-part queries
    def recordQuery(self, seq_num):
        query_id = self.curr_query_id
        if seq_num > 0:
            query_id = query_id + "b"
        self.query_id_list.append(query_id)
        if seq_num == 0:
            self.query_explain_time_map[query_id] = self.planning_time1
        else:
            self.query_explain_time_map[query_id] = self.planning_time2
        if seq_num == 0:
            self.query_exe_time_map[query_id] = self.exe_time1
        else:
            self.query_exe_time_map[query_id] = self.exe_time2
        if seq_num == 0:
            self.query_comment_map[query_id] = self.comment1
        else:
            self.query_comment_map[query_id] = self.comment2
        self.query_explain_plan_map[query_id] = self.plans[seq_num]

    # process a single line of a log file
    def processLogFileLine(self, line_lf):
        line             = line_lf.rstrip('\r\n')
        fileNameMatch    = re.match(r'Filename: ',         line)
        explainMatch     = re.match(r'[ ]*QUERY PLAN[ ]*', line)
        executionMatch   = re.match(r'EXECUTION:',         line)
        timeMatch        = re.match(r'Time: ',             line)
        optimizerMatch   = re.match(r' Optimizer[ a-z]*: ',line)
        eofMatch         = re.match(r'EOF-MARKER$',        line)

        if executionMatch:
            self.exe_phase = True
            self.explain_phase = False
        elif explainMatch:
            self.exe_phase = False
            self.explain_phase = True
            self.plan_phase = True
        elif timeMatch:
            time = re.sub(r'Time: ([0-9]+).*', r'\1', line)
            if self.exe_phase:
                if self.exe_time1 < 0:
                    self.exe_time1 = time
                else:
                    self.exe_time2 = time
                    self.second_query = True
            elif self.explain_phase:
                if self.planning_time1 < 0:
                    self.planning_time1 = time
                else:
                    self.planning_time2 = time
                    self.second_query = True
            # finalize the explain plan for this query and store it
            if self.plan_phase:
                self.plan_phase = False
                self.plans.append(self.curr_plan)
                self.curr_plan = []
        elif optimizerMatch:
            # Since this script can be used on older explain logs, match both
            # original and new ORCA names
            if (not re.search(' PQO ', line) or not re.search(' Pivotal Optimizer ', line)):
                if self.planning_time1 < 0:
                    self.comment1 = self.comment1 + "Fallback "
                else:
                    self.comment2 = self.comment2 + "Fallback "

        if self.plan_phase:
            # this is a line of an explain plan, remember it
            self.curr_plan.append(line_lf)
            
        if (fileNameMatch and not self.first_file_name_match) or eofMatch:
            # end of a query
            self.recordQuery(0)
            if self.second_query:
                self.recordQuery(1)

        if fileNameMatch:
            # beginning of a new query
            self.reset_curr_query_state()
            self.first_file_name_match = False
            self.curr_query_id = re.sub(r'Filename: 1([0-9]+).*', r'\1', line)

    # process an entire log file
    def processLogFile(self, logFileLines):
        for line in logFileLines:
            self.processLogFileLine(line)
        self.processLogFileLine('EOF-MARKER')

    def comparePlans(self, base, queryId):
        myPlan = self.query_explain_plan_map[queryId]
        basePlan = base.query_explain_plan_map[queryId]
        # return value (0 = same, 1 = cost change, 2 = cardinality change, 3 = other change)

        plan_change_found = False
        cost_change_found = False
        rows_change_found = False

        if len(myPlan) != len(basePlan):
            # plans are different (different number of lines)
            return "plan change found"

        for l in range(len(myPlan)):
            myLine = myPlan[l]
            baseLine = basePlan[l]
            if myLine != baseLine:
                myLineNoCost = re.sub(r'cost=[0-9.]*', 'cost=xxx', myLine)
                baseLineNoCost = re.sub(r'cost=[0-9.]*', 'cost=xxx', baseLine)
                if myLineNoCost == baseLineNoCost:
                    cost_change_found = True
                else:
                    myLineNoRows = re.sub(r'rows=[0-9]*', 'rows=xxx', myLineNoCost)
                    baseLineNoRows = re.sub(r'rows=[0-9]*', 'rows=xxx', baseLineNoCost)
                    if myLineNoRows == baseLineNoRows:
                        rows_change_found = True
                    else:
                        myOptimizer = re.sub(r'Optimizer[ a-z]*:[ a-zA-Z0-9.]*', 'Optimizer:xxx', myLine)
                        baseOptimizer = re.sub(r'Optimizer[ a-z]*:[ a-zA-Z0-9.]*', 'Optimizer:xxx', baseLine)
                        if myOptimizer != baseOptimizer:
                            # lines are different even with masked-out rows and cost
                            plan_change_found = True

        if plan_change_found:
            return "plan change found"
        elif cost_change_found and rows_change_found:
            return "cost and cardinality change found"
        elif rows_change_found:
            return "row change found"
        elif cost_change_found:
            return "cost change found"
        return ""

    # print header for CSV file
    def printHeader(self, numFiles):
        if (numFiles == 1):
            print 'Query id, planning time, execution time, comment'
        else:
            print 'Query id, base planning time, planning time, base execution time, execution time, plan changes, base comment, comment'

    # print result for all recorded queries in CSV format for a single log file
    def printme(self):
        for q in self.query_id_list:
            print "%s, %s, %s, %s" % (q, self.query_explain_time_map[q], self.query_exe_time_map[q], self.query_comment_map[q])

    # print a CSV file with a comparison between a base file and a test file
    def printComparison(self, base):
        for q in self.query_id_list:
            planDiffs = self.comparePlans(base, q)
            print "%s, %s, %s, %s, %s, %s, %s, %s" % (q, base.query_explain_time_map[q], self.query_explain_time_map[q], base.query_exe_time_map[q], self.query_exe_time_map[q], planDiffs, base.query_comment_map[q], self.query_comment_map[q])

            

def main():
    parser = argparse.ArgumentParser(description='Summarize the test suite execute and explain log')
    parser.add_argument('log_file', nargs = '?', help='log file with explain/execute output')
    parser.add_argument('--baseLog',
                        help='specify a log file from a base version to compare to')

    args = parser.parse_args()

    inputfile = args.log_file
    basefile = args.baseLog

    if inputfile is None:
        print "Expected the name of a log file with test suite queries\n"
        exit(1)

    if basefile is not None:
        with open(basefile, "r") as fpb:
            baseState = FileState()
            baseState.processLogFile(fpb)

    with open(inputfile, "r") as fp:
        testState = FileState()
        testState.processLogFile(fp)

    if basefile is None:
        testState.printHeader(1)
        testState.printme()
    else:
        testState.printHeader(2)
        testState.printComparison(baseState)

if __name__== "__main__":
    main()
