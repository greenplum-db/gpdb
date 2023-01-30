#!/usr/bin/env python3
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
import os

# types of diffs in plans, by increasing severity
NO_CHANGES   = 0
COST_CHANGES = 1
ROWS_CHANGES = 2
PLAN_CHANGES = 3
planDiffText = { NO_CHANGES: "", COST_CHANGES: "cost change found", ROWS_CHANGES: "row change found", PLAN_CHANGES: "plan change found" }

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
        if len(self.plans) > seq_num:
            self.query_explain_plan_map[query_id] = self.plans[seq_num]
        else:
            self.query_explain_plan_map[query_id] = "error - no plan found"
            self.query_comment_map[query_id] += "error - no plan found"

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
            if (not re.search(' PQO ', line) and not re.search(' Pivotal Optimizer ', line)):
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
            return PLAN_CHANGES

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
                        myOptimizer = re.sub(r'Optimizer[ a-z]*:.*', 'Optimizer:xxx', myLine)
                        baseOptimizer = re.sub(r'Optimizer[ a-z]*:.*', 'Optimizer:xxx', baseLine)
                        if myOptimizer != baseOptimizer:
                            # lines are different even with masked-out rows and cost
                            plan_change_found = True

        if plan_change_found:
            return PLAN_CHANGES
        elif rows_change_found:
            return ROWS_CHANGES
        elif cost_change_found:
            return COST_CHANGES
        return NO_CHANGES

    # print header for CSV file
    def printHeader(self, numFiles):
        if (numFiles == 1):
            print('Query id, planning time, execution time, comment')
        else:
            print('Query id, base planning time, planning time, base execution time, execution time, plan changes, base comment, comment')

    # print result for all recorded queries in CSV format for a single log file
    def printme(self):
        for q in self.query_id_list:
            print("%s, %s, %s, %s" % (q, self.query_explain_time_map[q], self.query_exe_time_map[q], self.query_comment_map[q]))

    # print a CSV file with a comparison between a base file and a test file
    def printComparison(self, base, diffDir, diffThreshold, diffLevel):
        for q in self.query_id_list:
            planDiffs = self.comparePlans(base, q)
            print("%s, %s, %s, %s, %s, %s, %s, %s" % (q, base.query_explain_time_map[q], self.query_explain_time_map[q], base.query_exe_time_map[q], self.query_exe_time_map[q], planDiffText[planDiffs], base.query_comment_map[q], self.query_comment_map[q]))
            if int(diffLevel) <= int(planDiffs):
                baseTime = float(base.query_exe_time_map[q])
                testTime = float(self.query_exe_time_map[q])
                if testTime > baseTime * (1+float(diffThreshold)/100.0) or testTime < 0:
                    baseFileName = diffDir + "/base/" + q + ".plan"
                    testFileName = diffDir + "/test/" + q + ".plan"
                    with open(baseFileName, 'w') as fb:
                        for line in base.query_explain_plan_map[q]:
                            fb.write(line)
                        fb.write("Execution time: %s\n" % base.query_exe_time_map[q])
                    with open(testFileName, 'w') as ft:
                        for line in self.query_explain_plan_map[q]:
                            ft.write(line)
                        ft.write("Execution time: %s\n" % self.query_exe_time_map[q])
            

def main():
    parser = argparse.ArgumentParser(description='Summarize the test suite execute and explain log')
    parser.add_argument('log_file', nargs = '?', help='log file with explain/execute output')
    parser.add_argument('--baseLog',
                        help='specify a log file from a base version to compare to')
    parser.add_argument('--diffDir',
                        help='request diff files to be created and specify a directory to place diffs into')
    parser.add_argument('--diffThreshold',
                        help='specify a numerical threshold to record plan diffs with a performance regression of more than n percent')
    parser.add_argument('--diffLevel',
                        help='specify which diff files to generate: 1 = all diffs, 2 = ignore cost diffs, 3 = plan diffs only')

    args = parser.parse_args()

    inputfile = args.log_file
    basefile = args.baseLog
    makeDiffs = (args.diffDir is not None)
    diffDir = ""
    diffThreshold = -100
    diffLevel = 4
    if makeDiffs:
        # remove trailing slash, if it exists
        diffDir = re.sub(r'(.*)/$','\1', args.diffDir)
        try:
            os.mkdir(diffDir)
            os.mkdir(diffDir + "/base")
            os.mkdir(diffDir + "/test")
        except:
            print("Unable to create diff directory %s" % diffDir)
            exit(1)
        if args.diffThreshold is not None:
            diffThreshold = args.diffThreshold
            if args.diffLevel is None:
                # if only diffThreshold is specified, then default diffLevel to COST_CHANGES
                args.diffLevel = COST_CHANGES
        if args.diffLevel is not None:
            diffLevel = args.diffLevel
    else:
        if (args.diffThreshold is not None or args.diffLevel is not None):
            print("Please specify the --diffDir option with a directory name to request diff files\n")
            exit(1)

    if inputfile is None:
        print("Expected the name of a log file with test suite queries\n")
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
        testState.printComparison(baseState, diffDir, diffThreshold, diffLevel)

if __name__== "__main__":
    main()
