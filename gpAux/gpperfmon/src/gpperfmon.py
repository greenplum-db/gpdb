#!/usr/bin/env python

'''
Controls or queries the web components of gpperfmon.
'''

import os
import stat
import sys
import signal
import time
import socket
import subprocess
import shutil
import ConfigParser
import re
import getpass
import psutil
from gppylib.db import dbconn

GPPERFMONHOME=os.getenv('GPPERFMONHOME')
if not GPPERFMONHOME:
    sys.exit('ERROR: GPPERFMONHOME environment variable is not set.  Please check that you have sourced gpperfmon_path.sh.')

sys.path.append(os.path.join(GPPERFMONHOME, 'lib', 'python'))

try:
    from gppylib.gpparseopts import OptParser
    from gppylib.gpparseopts import OptChecker
    from gppylib.userinput import *
except ImportError, e:    
    sys.exit('ERROR: Cannot import modules.  Please check that you have sourced gpperfmon_path.sh.  Detail: ' + str(e))


script_name = os.path.split(__file__)[-1]
script_version = 'main build 29200'

# consts
STOPPED = 0
RUNNING = 1
STRANDED_GPMONWS = 2
STRANDED_LIGHT = 3
ONLY_LIGHT = 4

RESTART_SUCCESS = 1
RESTART_FAILED = 0

LIGHTY_BIN = os.path.join(GPPERFMONHOME, 'bin', 'lighttpd')
OPENSSL_BIN = os.path.join(GPPERFMONHOME, 'bin', 'openssl')
OPENSSL_CNF = os.path.join(GPPERFMONHOME, 'etc', 'openssl.cnf')

# Custom input validators
len_validator = lambda str, ignore1, ignore2: str if len(str) > 0 else None
len_nospace_validator = lambda str, ignore1, ignore2: str if len(str) > 0 and str.find(' ') == -1 else None


_usage = """{ --start | --stop | --restart | --status | --setup | --upgrade} ["instance name"]
"""
   
_description = (""" 
Controls and configures the Greenplum Performance Monitor web server.
""")

_help = ("""
""")
    
#################
def version():
    print '%s version %s' % (script_name, script_version)
    print 'lighttpd version: %s' % lighty_version()

    
#################
def parse_command_line():
    parser = OptParser(option_class=OptChecker,
                description=' '.join(_description.split()))
    parser.setHelp(_help)
    parser.set_usage('%prog ' + _usage)
    parser.remove_option('-h')
    
    parser.add_option('--start', action='store_true',
                        help='Start the Greenplum Performance Monitor web server.')
    parser.add_option('--stop', action='store_true',
                      help='Stop the Greenplum Performance Monitor web server.')
    parser.add_option('--restart', action='store_true',
                      help='Restart the Greenplum Performance Monitor web server.')                        
    parser.add_option('--status', action='store_true',
                      help='Display the status of the Gerrnplum Performance Monitor web server.')
    parser.add_option('--setup', action='store_true',
                      help='Setup the Greenplum Performance Monitor web server.')
    parser.add_option('--version', action='store_true',
                       help='Display version information')
    parser.add_option('--upgrade', action='store_true',
                      help='Upgrade a previous installation of the Greenplum Performance Monitors web UI')
        
    parser.set_defaults(verbose=False,filters=[], slice=(None, None))
    
    # Parse the command line arguments
    (options, args) = parser.parse_args()

    if options.version:
        version()
        sys.exit(0)
    
    # check for too many options
    opt_count = 0
    if options.start:
        opt_count+=1
    if options.stop:
        opt_count+=1
    if options.setup:
        opt_count+=1
    if options.upgrade:
        opt_count+=1
    if options.status:
        opt_count+=1

    if opt_count > 1:
        parser.print_help()
        parser.exit()
    
    return options, args

#################
def start_application(cmd):
    pid = os.fork()
    if not pid:
        os.execve('/bin/sh', ['sh', '-c', cmd], os.environ)

#################
def get_instance_info(instance):
    instance_info = {}
    
    instance_info['name'] = instance
    instance_info['root_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance)
    instance_info['sessions_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'sessions')
    instance_info['temp_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'tmp')
    instance_info['log_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'logs')
    instance_info['ui_conf_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'conf')
    instance_info['ui_conf_file'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'conf', 'gpperfmonui.conf')
    instance_info['lighttpd_conf_file'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'conf', 'lighttpd.conf')
    instance_info['lighttpd_pid_file'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'lighttpd.pid')
    instance_info['web_root_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'web')
    instance_info['web_lib_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'web', 'lib')
    instance_info['web_static_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'web', 'static')
    instance_info['web_templates_dir'] = os.path.join(GPPERFMONHOME, 'instances', instance, 'web', 'templates')
    
    return instance_info



#################
def lighty_version():
    ver_string = 'Unknown'
    try:
        FILE = os.popen('%s -v' % LIGHTY_BIN)
        if FILE:
            ver_string = FILE.readline().split(' ')[0]
            FILE.close()
    except:
        pass
    
    return ver_string


#################
def lighty_start(instance):
    res = 0
    cfg = ConfigParser.SafeConfigParser()

    pgpass = dbconn.Pgpass()
    if not pgpass.pgpass_valid():
        print 'Error: .pgpass file not valid.  Unable to start instance'
        return 0
    
    instance_info = get_instance_info(instance)
    if lighty_status(instance) == STOPPED:
        # Read in the config so we can set env for remote connections if needed
        try:
            # parse configuration file
            cfg.readfp(open(instance_info['ui_conf_file']))
        except:
            print 'Error loading configuration file %s for instance %s' % (instance_info['ui_conf_file'], instance)
            return 0

        # save off original values
        old_pghost = os.getenv('PGHOST', '')
        old_pgport = os.getenv('PGPORT', '')
        old_pgpassword = os.getenv('PGPASSWORD', '')

        # see if there are values in conf file
        master_hostname = ''
        master_port = ''

        ###########################
        try:
            master_hostname = cfg.get('WEB APP', 'master_hostname')
        except:
            pass
        if master_hostname == '':
            if old_pghost:
                master_hostname = old_pghost
            else:
                master_hostname = '127.0.0.1'
        os.environ['PGHOST'] = master_hostname
        ###########################
        
        ###########################
        try:
            master_port = cfg.get('WEB APP', 'master_port')
        except:
            pass
        if master_port == '':
            if old_pgport:
                master_port = old_pgport
            else:
                master_port = '5432'
        os.environ['PGPORT'] = master_port
        ###########################

        password = pgpass.get_password('gpmon', master_hostname, master_port, 'gpperfmon')
        if not password:
            password = old_pgpassword
        os.environ['PGPASSWORD'] = password

        start_application('%s -f "%s" -m "%s"' % (LIGHTY_BIN, instance_info['lighttpd_conf_file'], os.path.join(GPPERFMONHOME, 'lib')))
        time.sleep(1)
        
        # restore the original values
        os.environ['PGHOST'] = old_pghost
        os.environ['PGPORT'] = old_pgport
        os.environ['PGPASSWORD'] = old_pgpassword
        
    return lighty_status(instance)


#################
def lighty_stop(instance):

    light_bin = "bin/lighttpd"
    instance_conf = "instances/%s/conf/lighttpd.conf" % instance
    instance_info = get_instance_info(instance)

    status = lighty_status(instance)
    if status == RUNNING or status == STRANDED_LIGHT or status == ONLY_LIGHT:
        try:
            for process in psutil.process_iter():

                try:
                    process_args  = process.cmdline()
                except:
                    continue
                    # we do not have privalege to view/kill this process, so skip it

                if len(process_args) < 3:
                    continue
                if not re.search(light_bin, process_args[0]):
                    continue
                if not re.search(instance_conf, process_args[2]):
                    continue

                print "killing pid %d" % process.pid
                os.kill(process.pid, signal.SIGTERM)
            
            # lighty shuts down quickly, but give it a little bit of time.
            time.sleep(1)
        except:
            return lighty_status()
        
    # clean up session info
    try:
        for dir in os.listdir(instance_info['sessions_dir']):
            shutil.rmtree(os.path.join(instance_info['sessions_dir'], dir))
    except Exception, msg:
        pass

    # clean up lighttpd's folder used for compression
    try:
        shutil.rmtree(os.path.join(instance_info['temp_dir'], 'lighttpd'))
    except:
        pass

    return lighty_status(instance)


#################
def kill_orphaned_cgi_scripts():
    "Search for any gpmonws.py that are stranded and do not belong to lighttpd"

    for process in psutil.process_iter():
        try:
            process_args  = process.cmdline()
        except:
            continue
            # we do not have privalege to view/kill this process, so skip it

        if len(process_args) >= 2:
            if process_args[0] == 'python':
                if re.search("gpmonws.py", process_args[1]):
                    if process.ppid() == 1:
                        print "Killing stranded gpmonws.py process with pid %d" % process.pid
                        os.kill(process.pid, signal.SIGTERM)

#################
def lighty_restart(instance):

    if lighty_status(instance) == RUNNING:

        status = lighty_stop(instance)

        if status != STOPPED:
            print 'Failed to stop gpperfmon instance %s during restart' % instance
            return RESTART_FAILED

    status = lighty_start(instance)
    if status != RUNNING:
        return RESTART_FAILED

    return RESTART_SUCCESS


#################
def lighty_status(instance):

    foundLighthttp = False
    foundGpmonws = False
    foundStrandedLight = False
    foundStrandedPython = False

    instance_conf = "instances/%s/conf/lighttpd.conf" % instance
    instance_mon = "instances/%s/web/gpmonws.py" % instance
    instance_info = get_instance_info(instance)
    light_bin = "bin/lighttpd"
    lightpid = 0

    try:
        FILE = open(instance_info['lighttpd_pid_file'], 'r')
        lightpid = int(FILE.readline())
        FILE.close()
    except:
        pass

    try:
        for process in psutil.process_iter():

            try:
                process_args  = process.cmdline()
            except:
                continue
                # we do not have privalege to view/kill this process, so skip it

            if len(process_args) < 1:
                continue

            # lighttpd process
            if re.search(light_bin, process_args[0]):
                if lightpid != 0 and process.pid == lightpid:
                    foundLighthttp = True
                elif len(process_args) >= 3:
                    if re.search(instance_conf, process_args[2]):
                        foundStrandedLight = True

            # gpmonws.py process
            elif re.search("python", process_args[0]):
                if len(process_args) < 2:
                    continue
                if re.search(instance_mon, process_args[1]):
                    if lightpid != 0 and process.ppid() == lightpid:
                        foundGpmonws = True
                    else:
                        foundStrandedPython = True
    except:
        pass

    if foundStrandedLight:
        return STRANDED_LIGHT
    if foundStrandedPython:
        return STRANDED_GPMONWS

    if foundLighthttp and foundGpmonws:
        return RUNNING
    elif foundLighthttp:
        return ONLY_LIGHT
    else: 
        return STOPPED

#################
def webui_setup():
    help = """An instance name is used by the Greenplum Performance monitor as
a way to uniquely identify a Greenplum Database that has the monitoring
components installed and configured.  This name is also used to control
specific instances of the Greenplum Performance monitors web UI.  Instance
names cannot contain spaces."""
    
    instance_name = ask_input(help, 'Please enter a new instance name.  Entering an existing\n'
                                    'instance name will reconfigure that instance', '', 
                                    'default', len_nospace_validator, None)
    instance_info = get_instance_info(instance_name)

    reconfigure = os.path.exists(instance_info['root_dir'])

    # defaults for webapi
    server_name_default = '[server name to display]'
    allow_trust_logon_default = '[no|yes] - setting to yes is insecure and only for testing'

    config = ConfigParser.ConfigParser()
    config.add_section('WEB APP')
    config.set('WEB APP', '#allow_trust_logon', allow_trust_logon_default)

    help = """The web component of the Greenplum Performance Monitor can connect to a
monitor database on a remote Greenplum Database."""

    yn = ask_yesno(help, '\nIs the master host for the Greenplum Database remote?', 'N')
    if yn:
        config.set('WEB APP', 'remote', True)
        master_hostname = ask_input(None, 'What is the hostname of the master', '', '', len_validator, None)
        config.set('WEB APP', 'master_hostname', master_hostname)
        config.set('WEB APP', 'server_name', master_hostname)
    else:
        help = """The display name is shown in the web interface and does not need to be
a hostname.
        """
        display_name = ask_input(help, 'What would you like to use for the display name for this instance', 
                                 '', 'Greenplum Database', len_validator, None)
        config.set('WEB APP', 'server_name', display_name)

    pgport = int(os.getenv('PGPORT', 5432))
    master_port = ask_int(None, 'What port does the Greenplum Database use?', '', pgport, 1, 65535)       
    config.set('WEB APP', 'master_port', master_port)
            
    setup_instance_directory(instance_name)

    enable_ssl = 'disable'
    use_existing_cert = False
    
    #TODO: check available ports?
    lighty_port = 28080

    #lighty conf
    lighty_access_log = os.path.join(instance_info['log_dir'], 'lighttpd-access.log')
    lighty_error_log = os.path.join(instance_info['log_dir'], 'lighttpd-error.log')
    ipv6accesslog = os.path.join(instance_info['log_dir'], 'lighttpd-access-ipv6.log')
    ipv6errorlog = os.path.join(instance_info['log_dir'], 'lighttpd-error-ipv6.log')

    help = """The Greenplum Performance Monitor runs a small web server for the UI and web API.  
This web server by default runs on port 28080, but you may specify any available port."""
    lighty_port = ask_int(help, 'What port would you like the web server to use for this instance?', '', 28080, 1, 65534)

    ssl_cert = ''

    help = """Users logging in to the Performance Monitor must provide database user
credentials.  In order to protect user names and passwords, it is recommended
that SSL be enabled."""

    yn = ask_yesno(help, '\nDo you want to enable SSL for the Web API', 'Y')
    if yn:
        enable_ssl = 'enable'
        if os.path.exists(os.path.join(instance_info['ui_conf_dir'], 'cert.pem')):
            use_existing_cert = ask_yesno(None, '\nFound an existing SSL certificate.  Do you want to use the existing certificate?', 'Y')


    if enable_ssl == 'enable':
        if not use_existing_cert:
            ret = generate_cert(os.path.join(instance_info['ui_conf_dir'], 'cert.pem'))
            if not ret:
                print 'Failed to generate SSL certificate.  SSL will be disabled.'
                enable_ssl = 'disable'
        
        ssl_cert = os.path.join(instance_info['ui_conf_dir'], 'cert.pem')


    yn = ask_yesno(None, '\nDo you want to enable ipV6 for the Web API', 'N')
    if yn:
        useipv6 = True
    else:
        useipv6 = False

    lighty_conf = generate_lighty_conf(instance_info, lighty_port, lighty_access_log, lighty_error_log,
                                       os.path.join(instance_info['web_root_dir'], 'gpmonws.py'), 
                                       enable_ssl, ssl_cert, useipv6, ipv6accesslog, ipv6errorlog)

    try:
        FILE = open(instance_info['lighttpd_conf_file'], 'w')
        FILE.writelines(lighty_conf)
        FILE.close()
        print '\nDone writing lighttpd configuration to %s' % instance_info['lighttpd_conf_file']
    except (errno, errstr):
        print 'Error: Failed to write lighttpd configuration file to %s' % instance_info['lighttpd_conf_file']
        print errstr
        sys.exit(1)

    try:
        FILE = open(instance_info['ui_conf_file'], 'w')
        config.write(FILE)
        FILE.close()
        print 'Done writing web UI configuration to %s' % instance_info['ui_conf_file']
    except (errno, errstr):
        print 'Error: Failed to write gpperfmon configuration to %s' % instance_info['ui_conf_file']
        print errstr
        sys.exit(1)

    webui_url = ''
    if enable_ssl == 'enable':
        webui_url = 'https://'
    else:
        webui_url = 'http://'
    webui_url = webui_url + socket.gethostname() + ':' + str(lighty_port) + '/'
    
    
    print '\nGreenplum Performance Monitor UI configuration is now complete.  If'
    print 'at a later date you want to change certain parameters, you can '
    print 'either re-run \'gpperfmon --setup\' or edit the configuration file'
    print 'located at ' + instance_info['ui_conf_file'] + '.'
    if not reconfigure:
        print '\nThe web UI for this instance is available at %s' % webui_url
        print '\nYou can now start the web UI for this instance by running: gpperfmon --start ' + instance_info['name'] 
    else:
        print '\nRestarting web UI instance %s...' % instance_info['name']
        status = lighty_restart(instance_info['name'])
        if status == RESTART_SUCCESS:
            print 'Done.'
            print 'The web UI for this instance is available at %s' % webui_url
        else:
            print '\nThere was an error restarting web UI instance %s...' % instance_info['name']
      
    
#################
def setup_instance_directory(instance_name):
    instance_info = get_instance_info(instance_name)
    
    try:
        os.mkdir(instance_info['root_dir'])
    except OSError:
        pass #dir exists

    try:
        os.mkdir(instance_info['sessions_dir'])
    except OSError:
        pass #dir exists

    try:
        os.mkdir(instance_info['temp_dir'])
    except OSError:
        pass #dir exists

    try:
        os.mkdir(instance_info['log_dir'])
    except OSError:
        pass #dir exists
    
    try:
        os.mkdir(instance_info['ui_conf_dir'])
    except OSError:
        pass #dir exists

    try:
        os.mkdir(instance_info['web_root_dir'])
    except OSError:
        pass #dir exists

    try:
        if os.path.islink(instance_info['web_lib_dir']):
            os.unlink(instance_info['web_lib_dir'])
        os.symlink(os.path.join(GPPERFMONHOME, 'www', 'lib'), instance_info['web_lib_dir'])
    except OSError, detail:
        print 'Error linking www/lib directory: %s' % detail
        sys.exit(1)

    try:
        if os.path.islink(instance_info['web_static_dir']):
            os.unlink(instance_info['web_static_dir'])
        os.symlink(os.path.join(GPPERFMONHOME, 'www', 'static'), instance_info['web_static_dir'])
    except OSError, detail:
        print 'Error linking www/static directory: %s' % detail
        sys.exit(1)

    try:
        if os.path.islink(instance_info['web_templates_dir']):
            os.unlink(instance_info['web_templates_dir'])
        os.symlink(os.path.join(GPPERFMONHOME, 'www', 'templates'), instance_info['web_templates_dir'])
    except OSError, detail:
        print 'Error linking www/templates directory: %s' % detail
        sys.exit(1)

    try:
        if os.path.islink(os.path.join(instance_info['web_root_dir'], 'gpmonws.py')):
            os.unlink(os.path.join(instance_info['web_root_dir'], 'gpmonws.py'))
        os.symlink(os.path.join(GPPERFMONHOME,'www', 'gpmonws.py'), os.path.join(instance_info['web_root_dir'], 'gpmonws.py'))
    except OSError, detail:
        print 'Error linking www/gpmonws.py: %s' % detail
        sys.exit(1)  

#################
def generate_lighty_conf(instance_info, port, lighty_access_log, 
                         lighty_err_log, webpybin, usessl, certpath, useipv6, ipv6accesslog, ipv6errorlog):
    # TODO: by instance
    fcgisocket = os.path.join(instance_info['root_dir'], 'perfmon.fastcgi.socket')
    fileString = '''server.modules = (
    "mod_rewrite",
    "mod_fastcgi",
    "mod_compress",
    "mod_accesslog" )

server.document-root = "%s"
server.pid-file = "%s"
server.errorlog = "%s"

mimetype.assign = (
  ".pdf"          =>      "application/pdf",
  ".sig"          =>      "application/pgp-signature",
  ".spl"          =>      "application/futuresplash",
  ".class"        =>      "application/octet-stream",
  ".ps"           =>      "application/postscript",
  ".torrent"      =>      "application/x-bittorrent",
  ".dvi"          =>      "application/x-dvi",
  ".gz"           =>      "application/x-gzip",
  ".pac"          =>      "application/x-ns-proxy-autoconfig",
  ".swf"          =>      "application/x-shockwave-flash",
  ".tar.gz"       =>      "application/x-tgz",
  ".tgz"          =>      "application/x-tgz",
  ".tar"          =>      "application/x-tar",
  ".zip"          =>      "application/zip",
  ".mp3"          =>      "audio/mpeg",
  ".m3u"          =>      "audio/x-mpegurl",
  ".wma"          =>      "audio/x-ms-wma",
  ".wax"          =>      "audio/x-ms-wax",
  ".ogg"          =>      "application/ogg",
  ".wav"          =>      "audio/x-wav",
  ".gif"          =>      "image/gif",
  ".jpg"          =>      "image/jpeg",
  ".jpeg"         =>      "image/jpeg",
  ".png"          =>      "image/png",
  ".xbm"          =>      "image/x-xbitmap",
  ".xpm"          =>      "image/x-xpixmap",
  ".xwd"          =>      "image/x-xwindowdump",
  ".css"          =>      "text/css",
  ".html"         =>      "text/html",
  ".htm"          =>      "text/html",
  ".js"           =>      "text/javascript",
  ".asc"          =>      "text/plain",
  ".c"            =>      "text/plain",
  ".cpp"          =>      "text/plain",
  ".log"          =>      "text/plain",
  ".conf"         =>      "text/plain",
  ".text"         =>      "text/plain",
  ".txt"          =>      "text/plain",
  ".dtd"          =>      "text/xml",
  ".xml"          =>      "text/xml",
  ".mpeg"         =>      "video/mpeg",
  ".mpg"          =>      "video/mpeg",
  ".mov"          =>      "video/quicktime",
  ".qt"           =>      "video/quicktime",
  ".avi"          =>      "video/x-msvideo",
  ".asf"          =>      "video/x-ms-asf",
  ".asx"          =>      "video/x-ms-asf",
  ".wmv"          =>      "video/x-ms-wmv",
  ".bz2"          =>      "application/x-bzip",
  ".tbz"          =>      "application/x-bzip-compressed-tar",
  ".tar.bz2"      =>      "application/x-bzip-compressed-tar"
 )

accesslog.filename = "%s"

$HTTP["url"] =~ "\.pdf$" {
  server.range-requests = "disable"
}

static-file.exclude-extensions = ( ".php", ".pl", ".fcgi", ".py" )

server.port = %d

compress.cache-dir = "%s"
compress.filetype = ("text/plain", "text/html", "text/xml")

fastcgi.server = ( "/gpmonws.py" =>
((
   "socket" => "%s",
   "bin-path" => "%s",
   "max-procs" => 1,
   "bin-environment" => (
     "REAL_SCRIPT_NAME" => ""
   ),
   "check-local" => "disable"
))
)

 url.rewrite-once = (
   "^/favicon.ico$" => "/static/favicon.ico",
   "^/static/(.*)$" => "/static/$1",
   "^/(.*)$" => "/gpmonws.py/$1",
 )
''' % (instance_info['web_root_dir'], 
       instance_info['lighttpd_pid_file'], 
       lighty_err_log, lighty_access_log, 
       port, instance_info['temp_dir'], 
       fcgisocket, webpybin)

    if useipv6:

        fileString += '''
$SERVER["socket"] == "[::]:%d" {
    accesslog.filename = "%s"
    server.errorlog = "%s"
}
''' % (port, ipv6accesslog, ipv6errorlog)

    if usessl == 'enable':

        fileString += '''
#### SSL engine
ssl.engine = "%s"
ssl.pemfile = "%s"
''' % (usessl, certpath)

    return fileString


#################
def webui_upgrade():
    master_data_directory = os.getenv('MASTER_DATA_DIRECTORY')
    if not master_data_directory:
        print 'Error - MASTER_DATA_DIRECTORY environment variable not set.'
        sys.exit(1)
        
    if not os.path.exists(os.path.join(master_data_directory, 'gpperfmon')):
        print 'Error - Unable to locate a previously installed version of gpperfmon.'
        print '        The gpperfmon directory does not exist in the MASTER_DATA_DIRECTORY'
        sys.exit(1)
        
    gpperfmon_loc = os.path.dirname(os.path.abspath(os.path.join(sys.argv[0], '..')))
    
    if gpperfmon_loc != GPPERFMONHOME:
        print 'Error - GPPERFMONHOME does not match the location of the new version'
        print '        Make sure that gpperfmon_path.sh from the new installation'
        print '        has been sourced correctly and try running the upgrade again.'
        sys.exit(1)

    # setup a new instance
    setup_instance_directory('default')
    instance_info = get_instance_info('default')
    # copy needed file from previous installation
    gpperfmon_dir = os.path.join(master_data_directory, 'gpperfmon')
    try:
        shutil.rmtree(instance_info['ui_conf_dir'])
        shutil.copytree(os.path.join(gpperfmon_dir, 'conf'), instance_info['ui_conf_dir'])
    except Exception, msg:
        print 'Error - Failed to copy configuration directory from previous installation:'
        print '        %s' % msg
        sys.exit(1)

    # fix up possible path problems and update file
    sed_cmd = 'sed "s/\/\/*/\//g" %s | sed  "s/%s/%s/g" | sed "s/\/tmp.*fastcgi.socket/%s/g" > %s' % (
                                                                 instance_info['lighttpd_conf_file'],
                                                                 gpperfmon_dir.replace('/', '\/'),
                                                                 instance_info['root_dir'].replace('/', '\/'),
                                                                 os.path.join(instance_info['root_dir'], 'perfmon.fastcgi.socket').replace('/', '\/'),
                                                                 instance_info['lighttpd_conf_file'] + '.new')

    ret = os.system(sed_cmd)
    if ret:
        print 'Error - There was an error updating the lighttpd configuration file.'
        sys.exit(1)
    
    shutil.move(os.path.join(instance_info['ui_conf_dir'], 'lighttpd.conf.new'), instance_info['lighttpd_conf_file'])
    # in this version the conf file name has changed.
    shutil.move(os.path.join(instance_info['ui_conf_dir'], 'gpperfmon.conf'), instance_info['ui_conf_file'])
    
    # cleanup unneeded files from the MASTER_DATA_DIRECTORY
    try:
        os.unlink(os.path.join(master_data_directory, 'gpperfmon', 'conf', 'lighttpd.conf'))
    except Exception, msg:
        print 'Warning - Error while trying to delete %s' % os.path.join(master_data_directory, 'gpperfmon', 'conf', 'lighttpd.conf')
        print msg
    try:
        os.unlink(os.path.join(master_data_directory, 'gpperfmon', 'conf', 'cert.pem'))
    except Exception, msg:
        print 'Warning -  could not delete %s' % os.path.join(master_data_directory, 'gpperfmon', 'conf', 'cert.pem')
        print msg

    print 'Upgrade finished.  The Greenplum Performance Monitor web UI can be started'
    print 'by running gpperfmon \'--start\'.'

#################
def generate_cert(destfile):
    cert_gen_cmd = '%s req -config %s -new -x509 -keyout %s -out %s -days 3650 -nodes' % (OPENSSL_BIN, OPENSSL_CNF, destfile, destfile)
    res = os.system(cert_gen_cmd)
    return (res == 0)


#################
def get_instances():
    instances = []
    instance_dir = os.path.join(GPPERFMONHOME, 'instances')
    for item in os.listdir(instance_dir):
        if os.path.isdir(os.path.join(instance_dir, item)):
            instance_info = get_instance_info(item)
            if os.path.isfile(instance_info['ui_conf_file']):
                instances.append(item)

    return instances


#################
# script begin


currentUser = getpass.getuser()
if currentUser == "root":
    print "This utility can not be run as 'root'"
    sys.exit(1)

# read in instances that have been configured
valid_instances = get_instances()

# parse the command line
options, instances = parse_command_line()

# if we weren't given a specific instance, do the op on all of them.
if not instances:
    instances = valid_instances

# validate the instance names given
for instance in instances:
    if instance not in valid_instances:
        print 'Error: %s is an invalid instance name' % instance
        sys.exit(1)

try:
    if options.start:
        # look for stranded gpmonws.py processes
        kill_orphaned_cgi_scripts()
 
        for instance in instances:
            print 'Starting instance %s...' % instance,
            sys.stdout.flush()
            result = lighty_start(instance)
            if result == RUNNING:
                print 'Done.'
            else:
                print 'Failed to start gpperfmon instance %s' % instance

    elif options.stop:

        # look for stranded gpmonws.py processes
        kill_orphaned_cgi_scripts()

        for instance in instances:
            print 'Stopping instance %s...' % instance,
            sys.stdout.flush()
            result = lighty_stop(instance)
            if result == STOPPED:
                print 'Done.'
            else:
                print 'Failed to stop gpperfmon instance %s' % instance

    elif options.restart:

        # look for stranded gpmonws.py processes
        kill_orphaned_cgi_scripts()

        for instance in instances:
            print 'Restarting instance %s...' % instance,
            sys.stdout.flush()
            status = lighty_restart(instance)
            if status == RESTART_SUCCESS:
                print 'Done.'
            else:
                print 'There was an error restarting instance %s.' % instance

        # look for stranded gpmonws.py processes
        kill_orphaned_cgi_scripts()

    elif options.status:
        for instance in instances:
            status = lighty_status(instance)
            if status == RUNNING:
                print 'Greenplum Performance Monitor UI for instance \'%s\' - [RUNNING]' % instance
            elif status == STRANDED_GPMONWS:
                print 'Greenplum Performance Monitor UI for instance \'%s\' - [INCONSISTENT: Stranded gpmonws.py process found]' % instance
            elif status == STRANDED_LIGHT:
                print 'Greenplum Performance Monitor UI for instance \'%s\' - [INCONSISTENT: Stranded lighttpd process found]' % instance
            elif status == ONLY_LIGHT:
                print 'Greenplum Performance Monitor UI for instance \'%s\' - [INCONSISTENT: gpmonws.py not found]' % instance
            else:
                print 'Greenplum Performance Monitor UI for instance \'%s\' - [STOPPED]' % instance
    elif options.setup:
        webui_setup()
    elif options.upgrade:
        webui_upgrade()

except KeyboardInterrupt:
    sys.exit('User canceled')
