#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
#
#
# THIS IMPORT MUST COME FIRST
# import mainUtils FIRST to get python version check
#
from gppylib.mainUtils import *

from gppylib.gplog import *
from gppylib.commands import unix
import pickle, base64
from gppylib import gparray
from gppylib.gpparseopts import OptChecker,OptParser,OptParser
from gppylib.operations.buildMirrorSegments import gDatabaseDirectories, gDatabaseFiles

logger = get_default_logger()

class GpCleanSegmentDirectoryProgram:
    """
    Clean up segment directories on a single host

    The caller should have already stopped the segments before calling this.
    """

    def __init__(self, options):
        self.__options = options

    def run(self):
        if self.__options.pickledArguments is None:
            raise ProgramArgumentValidationException("-p argument is missing")
            
        segments = pickle.loads(base64.urlsafe_b64decode(self.__options.pickledArguments))

        logger.info("Cleaning main data directories")
        for segment in segments:
            segmentdir = segment.getSegmentDataDirectory()
            logger.info("Cleaning %s" % segmentdir)

            unix.RemoveDirectoryContents('clean segment', segmentdir).run(validateAfter=True)

    def cleanup(self):
        pass

    #-------------------------------------------------------------------------
    @staticmethod
    def createParser():

        description = ("""
        Clean segment directories.
        """)

        help = ["""
          To be used internally only.
        """]

        parser = OptParser(option_class=OptChecker,
                    description=' '.join(description.split()),
                    version='%prog version $Revision: #1 $')
        parser.setHelp(help)

        addStandardLoggingAndHelpOptions(parser, True)

        addTo = OptionGroup(parser, "Clean Segment Options")
        parser.add_option_group(addTo)
        addTo.add_option('-p', None, dest="pickledArguments",
                         type='string', default=None, metavar="<pickledArguments>",
                       help="The arguments passed from the original script")

        parser.set_defaults()
        return parser


    @staticmethod
    def createProgram(options, args):
        if len(args) > 0 :
            raise ProgramArgumentValidationException("too many arguments: only options may be specified")
        return GpCleanSegmentDirectoryProgram(options)

#-------------------------------------------------------------------------
if __name__ == '__main__':
    mainOptions = { 'setNonuserOnToolLogger': True}
    simple_main( GpCleanSegmentDirectoryProgram.createParser, GpCleanSegmentDirectoryProgram.createProgram, mainOptions)
