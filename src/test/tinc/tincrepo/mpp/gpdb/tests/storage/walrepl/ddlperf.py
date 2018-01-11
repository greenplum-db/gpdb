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

import logging
import multiprocessing
import optparse
import os
import string
import subprocess
import sys
import datetime

SQLTemplate = """
CREATE TABLE ttt_$tid (a int, b bigint, c numeric, d text)
DISTRIBUTED BY (a)
PARTITION BY RANGE(b)
(START (1) END (100) EVERY (2));
INSERT INTO ttt_$tid SELECT i, i, i, i FROM generate_series(1, 99)i;
DROP TABLE ttt_$tid;

CREATE TABLE ttt_ao_$tid (a int, b bigint, c numeric, d text)
WITH (appendonly=true, compresstype=zlib)
DISTRIBUTED BY (b)
PARTITION BY RANGE(c)
(START (1) END (100) EVERY (2));
INSERT INTO ttt_ao_$tid SELECT i, i, i, i FROM generate_series(1, 99)i;
DROP TABLE ttt_ao_$tid;

CREATE TABLE ttt_co_$tid (a int, b bigint, c numeric, d text)
WITH (appendonly=true, orientation=column, compresstype=zlib)
DISTRIBUTED BY (b)
PARTITION BY RANGE(c)
(START (1) END (100) EVERY (2));
INSERT INTO ttt_co_$tid SELECT i, i, i, i FROM generate_series(1, 99)i;
DROP TABLE ttt_co_$tid;
""".strip()

logger = logging.getLogger(__name__)

def total_seconds(td):
    return float(td.microseconds +
            (td.seconds + td.days * 24 * 3600) * 10**6) / 10**6

def task(tid, iteration, psql_args):
    for i in range(iteration):
        logger.debug("{0:02}-{1:02}: start".format(tid, i))
        t0 = datetime.datetime.now()
        one_task(tid, psql_args)
        t1 = datetime.datetime.now()
        sec = total_seconds(t1 - t0)
        logger.debug("{0:02}-{1:02}: {2:.2f} sec".format(tid, i, sec))

def one_task(tid, psql_args):
    templ = string.Template(SQLTemplate)
    sql = templ.substitute(tid=tid)
    args = ['psql', '-c', sql]
    if psql_args.host is not None:
        args += ['-h', psql_args.host]
    if psql_args.port is not None:
        args += ['-p', psql_args.port]
    if psql_args.dbname is not None:
        args += [psql_args.dbname]
    proc = subprocess.Popen(args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    (stdout, stderr) = proc.communicate()
    if proc.returncode != 0:
        raise StandardError(stderr)


def main():
    parser = optparse.OptionParser(add_help_option=False)
    parser.add_option('-c', '--concurrency', dest='concurrency',
                      type='int', default='3',
                      help='number of concurrency')
    parser.add_option('-i', '--iteration', dest='iteration',
                      type='int', default='2',
                      help='number of iteration')
    parser.add_option('-h', '--host', dest='host',
                      type='string', help='host to connect')
    parser.add_option('-p', '--port', dest='port',
                      type='int', help='port to connect')
    parser.add_option('-v', '--verbose', dest='verbose',
                      action='store_true', help='output more')
    parser.add_option('--help', action='store_true',
                      help='print this help')
    (options, args) = parser.parse_args()

    if options.help:
        return parser.print_help()

    concurrency = options.concurrency
    iteration = options.iteration

    options.dbname = None
    if len(args) > 0:
        options.dbname = args.pop(0)

    ch = logging.StreamHandler(sys.stdout)
    log_level = logging.INFO
    if options.verbose:
        log_level = logging.DEBUG
    logger.setLevel(log_level)
    ch.setLevel(log_level)
    formatter = logging.Formatter('[%(asctime)s] %(message)s')
    ch.setFormatter(formatter)
    logger.addHandler(ch)

    t0 = datetime.datetime.now()
    procs = []
    for c in range(concurrency):
        proc = multiprocessing.Process(target=task,
                                       args=(c, iteration, options))
        proc.start()
        procs.append(proc)

    for proc in procs:
        proc.join()

    t1 = datetime.datetime.now()
    logger.info("COMPLETE: {0:.2f} sec".format(total_seconds(t1 - t0)))

if __name__ == '__main__':
    main()
