#!/usr/bin/env python

class HostType:
    GPCHECK_HOSTTYPE_UNDEFINED  = 0
    GPCHECK_HOSTTYPE_APPLIANCE = 1
    GPCHECK_HOSTTYPE_GENERIC_LINUX = 2
    GPCHECK_HOSTTYPE_GENERIC_SOLARIS = 3

def hosttype_str(type):
    if type == HostType.GPCHECK_HOSTTYPE_APPLIANCE:
        return "GPDB Appliance"
    elif type == HostType.GPCHECK_HOSTTYPE_GENERIC_LINUX:
        return "Generic Linux Cluster"
    elif type == HostType.GPCHECK_HOSTTYPE_GENERIC_SOLARIS:
        return "Generic Solaris Cluster"
    else:
        return "Undetected Platform"

class sysctl:
    def __init__(self):
        self.variables = dict() # dictionary of values
        self.errormsg = None

class omreport:
    def __init__(self):
        self.biossetup = dict()     # key value pairs
        self.biossetup_errormsg = None

        self.bootorder = list()     # list of Devices in order of boot
        self.bootorder_errormsg = None

        self.remoteaccess = dict()  # key value pairs
        self.remoteaccess_errormsg = None

        self.vdisks = list()        # list of dicts, 1 for each virtual disk
        self.vdisks_errormsg = None 

        self.controller = dict()    # key value pairs
        self.controller_errormsg = None

        self.omversion = None
        self.omversion_errormsg = None

        self.bios = dict()          # key value pairs
        self.bios_errormsg = None

        self.alerts = list()        # list of alerts... each alert is a dictionary of key value pairs
        self.alerts_errormsg = None

class chkconfig:
    def __init__(self):
        self.services = dict() # hash of services, each entry is hash of run levels and boolean value
        self.xinetd = dict()   # hash of services, value is boolean
        self.errormsg = None

class grubconf:
    def __init__(self):
        self.serial_declaration = False
        self.terminal_declaration = False
        self.ttyS1_declaration = False
        self.errormsg = None

    def __str__(self):
        return "serial_declaration(%s) terminal_declaration(%s) ttyS1_declaration(%s)" % (self.serial_declaration, self.terminal_declaration, self.ttyS1_declaration)

class inittab:
    def __init__(self):
        self.s1 = False
        self.defaultRunLevel = None
        self.errormsg = None

    def __str__(self):
        return "s1_declaration(%s) default_run_level(%s)" % (self.s1, self.defaultRunLevel)

class uname:
    def __init__(self):
        self.output = None
        self.errormsg = None

    def __str__(self):
        return self.output

class connectemc:
    def __init__(self):
        self.output = None
        self.errormsg = None

    def __str__(self):
        return self.output

class securetty:
    def __init__(self):
        self.errormsg = None
        self.data = set() 

class bcu:
    def __init__(self):
        self.firmware = None
        self.biosversion = None
        self.errormsg = None

    def __str__(self):
        return "firmware_version=%s|biosversion=%s" % (self.firmware, self.biosversion)

class ioschedulers:
    def __init__(self):
        self.devices = dict() # key is device name value is scheduler name
        self.errormsg = ''

class blockdev:
    def __init__(self):
        self.ra = dict() # key is device name value is getra value
        self.errormsg = ''

class rclocal:
    def __init__(self):
        self.isexecutable = False # check that /etc/rc.d/rc.local is executable permissions

    def __str__(self):
        return "executable(%s)" % self.isexecutable

class limitsconf_entry:
    def __init__(self, domain, type, item, value):
        self.domain = domain
        self.type = type
        self.item = item
        self.value = value

    def __str__(self):
        return "%s %s %s %s" % (self.domain, self.type, self.item, self.value)

class limitsconf:
    def __init__(self):
        self.lines = list() 
        self.errormsg = None

    def __str__(self):
        output = ""
        for line in self.lines:
            output = "%s\n%s" % (output, line)
        return output

class GpMount:
    def __init__(self):
        self.partition = None
        self.dir= None
        self.type = None
        self.options = set() # mount options

    def __str__(self):
        optionstring = ''
        first = True
        for k in self.options:

            if not first:
                optionstring = "%s," % optionstring

            thisoption = k
            optionstring = "%s%s" % (optionstring, thisoption)
            first = False

        return "%s on %s type %s (%s)" % (self.partition, self.dir, self.type, optionstring)

class ntp:
    def __init__(self):
        self.running = False
        self.hosts = set()
        self.currentime = None
        self.errormsg = None

    def __str__(self):
        return "(running %s) (time %f) (peers: %s)" % (self.running, self.currenttime, self.hosts)

                
 
class mounts:

    def __init__(self):
        self.entries = dict() # dictionary key=partition value=mount object
        self.errormsg = None

    def __str__(self):
        output = ''
        for k in self.entries.keys():
            output = "%s\n%s" % (output, self.entries[k].__str__())
        return output

class solaris_etc_system:
    def __init__(self):
        self.parameters = dict() # dictionary of values
        self.errormsg = None

class solaris_etc_project:
    def __init__(self):
        self.lines = list() # list of lines in the file
        self.errormsg = None

class solaris_etc_user_attr:
    def __init__(self):
        self.lines = list() # list of lines in the file
        self.errormsg = None

class GenericSolarisOutputData:

    def __init__(self):
        self.etc_system = None
        self.etc_project = None
        self.etc_user_attr = None
        self.uname = None

    def __str__(self):

        grc = "============= /etc/system====================\n"
        gre = "============= /etc/system ERRORMSG===========\n"
        output = "%s%s\n%s%s" % (grc, self.etc_system.parameters.__str__(), gre, self.etc_system.errormsg)

        grc = "============= /etc/project===================\n"
        gre = "============= /etc/project ==================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.etc_project.lines.__str__(), gre, self.etc_project.errormsg)

        grc = "============= /etc/user_att==================\n"
        gre = "============= /etc/user_att==================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.etc_user_attr.lines.__str__(), gre, self.etc_user_attr.errormsg)

        grc = "============= UNAME==========================\n"
        gre = "============= UNAME ERRORMSG=================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.uname.__str__(), gre, self.uname.errormsg)

        return output


class GenericLinuxOutputData:
    def __init__(self):
        self.mounts = None
        self.uname = None
        self.blockdev = None
        self.ioschedulers = None
        self.sysctl = None
        self.limitsconf = None
        self.ntp = None

    def __str__(self):

        grc = "============= SYSCTL=========================\n"
        gre = "============= SYSCTL ERRORMSG================\n"
        output = "%s%s\n%s%s" % (grc, self.sysctl.variables.__str__(), gre, self.sysctl.errormsg)

        grc = "============= LIMITS=========================\n"
        gre = "============= LIMITS ERRORMSG================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.limitsconf.__str__(), gre, self.limitsconf.errormsg)

        mnt = "============= MOUNT==========================\n"
        mte = "============= MOUNT ERRORMSG=================\n"
        output = "%s\n%s%s\n%s%s" % (output, mnt, self.mounts.__str__(), mte, self.mounts.errormsg)

        grc = "============= UNAME==========================\n"
        gre = "============= UNAME ERRORMSG=================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.uname.__str__(), gre, self.uname.errormsg)

        grc = "============= IO SCHEDULERS==================\n"
        gre = "============= IO SCHEDULERS  ERRORMSG========\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.ioschedulers.devices.__str__(), gre, self.ioschedulers.errormsg)

        grc = "============= BLOCKDEV RA ====================\n"
        gre = "============= BLOCKDEV RA ERRORMSG============\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.blockdev.ra.__str__(), gre, self.blockdev.errormsg)

        grc = "============= NTPD ===========================\n"
        gre = "============= NTPD ERRORMSG===================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.ntp.__str__(), gre, self.ntp.errormsg)

        return output


class ApplianceOutputData:
    def __init__(self):
        self.chkconfig = None
        self.omreport = None
        self.grubconf = None
        self.mounts = None
        self.inittab = None
        self.uname = None
        self.securetty = None
        self.bcu = None
        self.blockdev = None
        self.rclocal = None
        self.ioschedulers = None
        self.sysctl = None
        self.limitsconf = None
        self.connectemc = None
        self.ntp = None

    def __str__(self):
        ser = "=============SERVICES=======================\n"
        xin = "=============XINETD  =======================\n"
        err = "=============CHKCONFIG ERRORMSG=============\n"
        output = "%s%s\n%s%s\n%s%s" % (ser, self.chkconfig.services.__str__(), xin, self.chkconfig.xinetd.__str__(), err, self.chkconfig.errormsg)

        omr = "=============OMREPORT VERSION ==============\n"
        ome = "=============OMREPORT VERSION ERRORMSG======\n"
        output = "%s\n%s%s\n%s%s" % (output, omr, self.omreport.omversion, ome, self.omreport.omversion_errormsg)

        omr = "=============OMREPORT BIOS==================\n"
        ome = "=============OMREPORT BIOS ERRORMSG=========\n"
        output = "%s\n%s%s\n%s%s" % (output, omr, self.omreport.bios.__str__(), ome,self.omreport.bios_errormsg)

        omr = "=============OMREPORT BIOSSETUP=============\n"
        ome = "=============OMREPORT BIOSSETUP ERRORMSG====\n"
        output = "%s\n%s%s\n%s%s" % (output, omr, self.omreport.biossetup.__str__(), ome,self.omreport.biossetup_errormsg)

        omr = "=============OMREPORT CONTROLLER============\n"
        ome = "=============OMREPORT CONTROLLER ERRORMSG===\n"
        output = "%s\n%s%s\n%s%s" % (output, omr, self.omreport.controller.__str__(), ome,self.omreport.controller_errormsg)

        boo = "=============OMREPORT BOOTORDER=============\n"
        boe = "=============OMREPORT BOOTORDER ERRORMSG====\n"
        output = "%s\n%s%s\n%s%s" % (output, boo, self.omreport.bootorder.__str__(), boe, self.omreport.bootorder_errormsg)

        omr = "=============OMREPORT REMOTEACCESS==========\n"
        ome = "=============OMREPORT REMOTEACCESS ERRORMSG=\n"
        output = "%s\n%s%s\n%s%s" % (output, omr, self.omreport.remoteaccess.__str__(), ome,self.omreport.remoteaccess_errormsg)

        omr = "=============OMREPORT ALERTS==========\n"
        ome = "=============OMREPORT ALERTS ERRORMSG=\n"
        output = "%s\n%s%s\n%s%s" % (output, omr, self.omreport.alerts.__str__(), ome,self.omreport.alerts_errormsg)

        omr = "=============OMREPORT VIRTUAL DISKS=========\n"
        ome = "=============OMREPORT VIRTUAL DISKS ERRORMSG\n"
        output = "%s\n%s%s\n%s%s" % (output, omr, self.omreport.vdisks.__str__(), ome,self.omreport.vdisks_errormsg)

        grc = "============= GRUB.CONF======================\n"
        gre = "============= GRUB.CONF ERRORMSG=============\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.grubconf.__str__(), gre, self.grubconf.errormsg)

        grc = "============= SYSCTL=========================\n"
        gre = "============= SYSCTL ERRORMSG================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.sysctl.variables.__str__(), gre, self.sysctl.errormsg)

        grc = "============= LIMITS=========================\n"
        gre = "============= LIMITS ERRORMSG================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.limitsconf.__str__(), gre, self.limitsconf.errormsg)

        mnt = "============= MOUNT==========================\n"
        mte = "============= MOUNT ERRORMSG=================\n"
        output = "%s\n%s%s\n%s%s" % (output, mnt, self.mounts.__str__(), mte, self.mounts.errormsg)

        grc = "============= INITTAB========================\n"
        gre = "============= INITTAB ERRORMSG===============\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.inittab.__str__(), gre, self.inittab.errormsg)

        grc = "============= UNAME==========================\n"
        gre = "============= UNAME ERRORMSG=================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.uname.__str__(), gre, self.uname.errormsg)

        grc = "============= CONNECTEMC=====================\n"
        gre = "============= CONNECtEMC ERRORMSG============\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.connectemc.__str__(), gre, self.connectemc.errormsg)

        grc = "============= SECURETTY======================\n"
        gre = "============= SECURETTY ERRORMSG=============\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.securetty.data.__str__(), gre, self.securetty.errormsg)

        grc = "============= IO SCHEDULERS==================\n"
        gre = "============= IO SCHEDULERS  ERRORMSG========\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.ioschedulers.devices.__str__(), gre, self.ioschedulers.errormsg)

        grc = "============= BLOCKDEV RA ====================\n"
        gre = "============= BLOCKDEV RA ERRORMSG============\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.blockdev.ra.__str__(), gre, self.blockdev.errormsg)

        grc = "============= BCU CNA ========================\n"
        gre = "============= BCU CNA ERRORMSG================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.bcu.__str__(), gre, self.bcu.errormsg)

        grc = "============= /etc/rc.d/rc.local =============\n"
        output = "%s\n%s%s" % (output, grc, self.rclocal.__str__())

        grc = "============= NTPD ===========================\n"
        gre = "============= NTPD ERRORMSG===================\n"
        output = "%s\n%s%s\n%s%s" % (output, grc, self.ntp.__str__(), gre, self.ntp.errormsg)

        return output
