# Line too long - pylint: disable=C0301
# Copyright (c) Greenplum Inc 2011. All Rights Reserved.

from optparse import OptionGroup
import os
import sys
import tarfile

try:
    from gppylib import gplog, pgconf
    from gppylib.commands import gp
    from gppylib.commands.base import Command, ExecutionError
    from gppylib.commands.unix import curr_platform, SUNOS
    from gppylib.db import dbconn
    from gppylib.gparray import GpArray
    from gppylib.gpversion import GpVersion
    from gppylib.gpparseopts import OptParser, OptChecker
    from gppylib.mainUtils import addMasterDirectoryOptionForSingleClusterProgram, addStandardLoggingAndHelpOptions, ExceptionNoStackTraceNeeded
    from gppylib.operations.package import MigratePackages, InstallPackage, UninstallPackage, QueryPackage, BuildGppkg, UpdatePackage, CleanGppkg, Gppkg, GPPKG_EXTENSION, GPPKG_ARCHIVE_PATH
    from gppylib.userinput import ask_yesno
    from gppylib.operations.unix import ListFilesByPattern

    import yaml
except ImportError, ex:
    sys.exit('Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(ex))

logger = gplog.get_default_logger()

class GpPkgProgram:
    """ This is the CLI entry point to package management code.  """
    def __init__(self, options, args):
        self.master_datadir = options.masterDataDirectory 

        # TODO: AK: Program logic should not be dictating master, standby, and segment information
        # In other words, the fundamental Operations should have APIs that preclude the need for this.
        self.master_host = None
        self.standby_host = None
        self.segment_host_list = None

        self.query = options.query
        self.build = options.build
        self.install = options.install
        self.remove = options.remove
        self.update = options.update
        self.clean = options.clean
        self.migrate = options.migrate
        self.interactive = options.interactive

        # only one of the following may be provided: --install, --remove, --update, --query, --build, --clean, --migrate
        count = sum([1 for opt in ['install', 'remove', 'update', 'query', 'build', 'clean', 'migrate'] if getattr(self, opt)])
        if count != 1:
            raise ExceptionNoStackTraceNeeded('Exactly one of the following must be provided: --install, --remove, -update, --query, --clean, --migrate')

        if self.query:
            # gppkg -q can be supplemented with --info, --list, --all
            count = sum([1 for opt in ['info', 'list', 'all'] if options.__dict__[opt]])
            if count > 1:
                raise ExceptionNoStackTraceNeeded('For --query, at most one of the following can be provided: --info, --list, --all') 
            # for all query options other than --all, a package path must be provided
            if not options.all and len(args) != 1:
                raise ExceptionNoStackTraceNeeded('A package must be specified for -q, -q --info, and -q --list.')

            if options.info:
                self.query = (QueryPackage.INFO, args[0])
            elif options.list:
                self.query = (QueryPackage.LIST, args[0])
            elif options.all:
                self.query = (QueryPackage.ALL, None)
            else:
                self.query = (None, args[0])   
        elif self.migrate:
            if len(args) != 2:
                raise ExceptionNoStackTraceNeeded('Invalid syntax, expecting "gppkg --migrate <from_gphome> <to_gphome>".')
            self.migrate = (args[0], args[1])

    @staticmethod
    def create_parser():
        parser = OptParser(option_class=OptChecker,
            description="Greenplum Package Manager",
            version='%prog version $Revision: #1 $')
        parser.setHelp([])

        addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=True)

        parser.remove_option('-q')
        parser.remove_option('-l')
        
        add_to = OptionGroup(parser, 'General Options')
        parser.add_option_group(add_to)

        addMasterDirectoryOptionForSingleClusterProgram(add_to)

        # TODO: AK: Eventually, these options may need to be flexible enough to accept mutiple packages
        # in one invocation. If so, the structure of this parser may need to change.
        add_to.add_option('-i', '--install', help='install the given gppkg', metavar='<package>')
        add_to.add_option('-u', '--update', help='update the given gppkg', metavar='<package>')
        add_to.add_option('-r', '--remove', help='remove the given gppkg', metavar='<name>-<version>')
        add_to.add_option('-q', '--query', help='query the gppkg database or a particular gppkg', action='store_true')
        add_to.add_option('-b', '--build', help='build a gppkg', metavar='<directory>')
        add_to.add_option('-c', '--clean', help='clean the cluster of the given gppkg', action='store_true')
        add_to.add_option('--migrate', help='migrate gppkgs from a separate $GPHOME', metavar='<from_gphome> <to_gphome>', action='store_true', default=False)

        add_to = OptionGroup(parser, 'Query Options')
        parser.add_option_group(add_to)
        add_to.add_option('--info', action='store_true', help='print information about the gppkg including name, version, description')
        add_to.add_option('--list', action='store_true', help='print all the files present in the gppkg')
        add_to.add_option('--all', action='store_true', help='print all the gppkgs installed by gppkg')

        return parser
            
    @staticmethod
    def create_program(options, args):
        """ TODO: AK: This convention may be unnecessary. """
        return GpPkgProgram(options, args)    

    def _get_gpdb_host_list(self):
        """
        TODO: AK: Get rid of this. Program logic should not be driving host list building .
        
            This method gets the host names 
            of all hosts in the gpdb array.
            It sets the following variables 
                GpPkgProgram.master_host to master
                GpPkgProgram.standby_host to standby
                GpPkgProgram.segment_host_list to segment hosts
        """
        
        logger.debug('_get_gpdb_host_list')
        
        #Get host list
        gparr = GpArray.initFromCatalog(dbconn.DbURL(port = self.master_port), utility = True)
        master_host = None
        standby_host = None 
        segment_host_list = []

        segs = gparr.getDbList()

        for seg in segs:
            if seg.isSegmentMaster(current_role = True):
                master_host = seg.getSegmentHostName()
            elif seg.isSegmentStandby(current_role = True):
                standby_host = seg.getSegmentHostName()
            else:
                segment_host_list.append(seg.getSegmentHostName())

        #Deduplicate the hosts so that we 
        #dont install multiple times on the same host
        segment_host_list = list(set(segment_host_list))

        #Segments might exist on the master host. Since we store the 
        #master host separately in self.master_host, storing the master_host
        #in the segment_host_list is redundant.
        for host in segment_host_list:
            if host == master_host or host == standby_host:
                segment_host_list.remove(host)

        self.master_host = master_host
        self.standby_host = standby_host
        self.segment_host_list = segment_host_list

    def _get_master_port(self, datadir):
       
        '''
            Obtain the master port from the pgconf file
        '''
       
        logger.debug('_get_master_port')
        pgconf_dict = pgconf.readfile(os.path.join(datadir, 'postgresql.conf'))
        return pgconf_dict.int('port') or os.getenv('PGPORT')

    def run(self):
        if self.build:
            BuildGppkg(self.build).run()    
            return 

        #Check for RPM and Solaris OS
        if curr_platform == SUNOS:
            raise ExceptionNoStackTraceNeeded('gppkg is not supported on Solaris')
                  
        try:
            cmd = Command(name = 'Check for rpm', cmdStr = 'rpm --version')
            cmd.run(validateAfter = True)
            results = cmd.get_results().stdout.strip()
            rpm_version_string = results.split(' ')[-1]

            if not rpm_version_string.startswith('4.'):
                raise ExceptionNoStackTraceNeeded('gppkg requires rpm version 4.x')

        except ExecutionError, ex: 
            results = ex.cmd.get_results().stderr.strip()
            if len(results) != 0 and 'not found' in results:
                raise ExceptionNoStackTraceNeeded('gppkg requires RPM to be available in PATH') 

        if self.migrate:
            MigratePackages(from_gphome = self.migrate[0],
                            to_gphome = self.migrate[1]).run()
            return

        # MASTER_DATA_DIRECTORY and PGPORT must not need to be set for 
        # --build and --migrate to function properly
        if self.master_datadir is None:
            self.master_datadir = gp.get_masterdatadir()
        self.master_port = self._get_master_port(self.master_datadir)

        # TODO: AK: Program logic should not drive host decisions.
        self._get_gpdb_host_list()

        if self.install:
            pkg = Gppkg.from_package_path(self.install)
            InstallPackage(pkg, self.master_host, self.standby_host, self.segment_host_list).run() 
        elif self.query:
            query_type, package_path = self.query
            QueryPackage(query_type, package_path).run()
        elif self.remove:
            if self.remove.count('-') != 1:
                raise ExceptionNoStackTraceNeeded('Please specify the correct <name>-<version>.')
            pkg_file_list = ListFilesByPattern(GPPKG_ARCHIVE_PATH, self.remove + '-*-*' + GPPKG_EXTENSION).run() 
            if len(pkg_file_list) == 0:
                raise ExceptionNoStackTraceNeeded('Package %s has not been installed.' % self.remove)
            assert len(pkg_file_list) == 1
            pkg_file = pkg_file_list[0]
            pkg = Gppkg.from_package_path(os.path.join(GPPKG_ARCHIVE_PATH, pkg_file))
            UninstallPackage(pkg, self.master_host, self.standby_host, self.segment_host_list).run()
        elif self.update:
            logger.warning('WARNING: The process of updating a package includes removing all')
            logger.warning('previous versions of the system objects related to the package. For')
            logger.warning('example, previous versions of shared libraries are removed.')
            logger.warning('After the update process, a database function will fail when it is')
            logger.warning('called if the function references a package file that has been removed.')
            if self.interactive:
                if not ask_yesno(None, 'Do you still want to continue ?', 'N'):
                    logger.info('Skipping update of gppkg based on user input')
                    return
            pkg = Gppkg.from_package_path(self.update)
            UpdatePackage(pkg, self.master_host, self.standby_host, self.segment_host_list).run()
        elif self.clean:
            CleanGppkg(self.standby_host, self.segment_host_list).run()

    def cleanup(self):
        pass
