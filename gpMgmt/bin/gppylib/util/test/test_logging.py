#!/usr/bin/env python3

"""
test script to provide gplog.feature (behave) test with logging samples.
"""
import logging

from gppylib import gplog

logger = gplog.get_default_logger()
gplog.setup_tool_logging("test_logging", "localhost", "gpadmin")
logger.warning("foobar to stdout and file")
gplog.log_to_file_only("blah to file only, not to stdout", logging.WARN)
gplog.log_to_file_only("another to file only, not to stdout")
