#!/usrbin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

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

Utility to create a user in the database with specific roles/privileges

------------------------------------------------------------------------------------------
Jira: QA-1750
Common Criteria tests require creating users with specific roles and/or privileges (e.g. LOGIN, CREATEDB). 
We need to provide a generic cdbfast utility that will allow test authors to create different types of users. 
It should be flexible enough to support the following:

Create database user with specific role
Create database user with specific permissions/privileges (e.g. LOGIN, CREATEDB)
Validate that user was created successfully
Validate that user has correct role assigned
Validate that user has correct privileges

------------------------------------------------------------------------------------------
Jira: QA-1751
Common Criteria tests require creating different roles in the database and performing certain operations 
as users who are members of this role (user group).
We need to provide a cdbfast utility class will provide APIs for the following:
Create specific database role (user group) with a specific privilege:
    See postgres documentation on different privileges supported by CREATE ROLE: 
    http://www.postgresql.org/docs/8.2/static/sql-createrole.html
Alter a specific database role (user group).
    See postgres documentation on different privileges supported by ALTER ROLE: 
    http://www.postgresql.org/docs/8.2/static/sql-alterrole.html
Return a list of users that are members of a given database role
Check if a specific user is a member of a given database role
Check if a specific role (user group) is a member of another database role
"""

############################################################################
# Set up some globals, and import gptest 
#    [YOU DO NOT NEED TO CHANGE THESE]
############################################################################


from mpp.lib.Globals import *
from mpp.lib.PgHba import PgHba, Entry
from mpp.lib.network import network
from mpp.lib.gpConfig import GpConfig
from mpp.lib.PSQL import PSQL
from mpp.lib.gpstop import GpStop
from mpp.lib.FileUtil import fileUtil
from mpp.lib.String import mstring


class GpUserRoleError(Exception): pass

class GpUserRole:
    """
    Common utility to create/alter user privilege, to check user/role, and to check role membership.
    @class GpUserRole
    
    @organization: DCD DELTA
    @contact: Huanming Zhang
    @exception: GpUserRoleError
    """
    
    PrivilegesSelect = {
            'SUPERUSER'     :'rolsuper=true',
            'NOSUPERUSER'   :'rolsuper=false',
            'CREATEDB'      :'rolcreatedb=true',
            'NOCREATEDB'    :'rolcreatedb=false',
            'CREATEROLE'    :'rolcreaterole=true',
            'NOCREATEROLE'  :'rolcreaterole=false',
            'INHERIT'       :'rolinherit=true',
            'NOINHERIT'     :'rolinherit=false',
            'LOGIN'         :'rolcanlogin=true',
            'NOLOGIN'       :'rolcanlogin=false',
            'CREATEEXTTABLE':'rolcreaterextgpfd=true',
            'NOCREATEEXTTABLE':'rolcreaterextgpfd=false'
                  }
    
    
    def __init__(self, host, usr, db, entry_type="host", authmethod="password", password=None):
        """ 
        Constructor for GpUserRole 
        @param host: which host to be tested;
        @param usr: user name to login host and login database;
        @param db: database name
        @change: Ritesh Jaltare
        @param entry_type: if a user is created, then we need the type of entry for pg_hba.conf for authen eg host, local, hostssl
        @param authmethod: denotes the authentication method used, eg: trust , password, md5
        @param address: denotes the IP address of the host. it can be both IPV4 or IPV6
        """
        self.gpstop = GpStop()
        self.host = host
        if password is None:
	  self.password = r"'password'"
        else:
          self.password = password
        self.usr = usr
        self.db = db
        if self.db is "all":
            self.pgpassdb = "*"
        else:
            self.pgpassdb=self.db    
        gpconfig = GpConfig()
        masterSSLValue = gpconfig.getParameterMasterOnly("ssl")
        if masterSSLValue is "on" and entry_type is "host":
            self.entry_type = "hostssl"
        else:
            self.entry_type = entry_type    
        
        self.authmethod = authmethod
        if os.path.exists(PGPASSFILE) is False:
            file = open(PGPASSFILE, 'w')
            os.chmod(PGPASSFILE,0600)
		
		
        
    def __normalizePrivileges(self, *privileges):
        privileges = [pri.strip().upper() for pri in privileges]
        if [False for pri in privileges 
                if 'NO' + pri in privileges]:
            raise GpUserRoleError("conflicted privileges: %s" % str(privileges))
        if [False for pri in privileges 
                if not pri in GpUserRole.PrivilegesSelect]:
            raise GpUserRoleError("undefined privileges: %s" % str(privileges))
        return privileges
        
    def __wrapCommand(self, command, user):
        return "\"psql %s -U %s -c \\\"%s\\\" \"" % (self.db, user, command)
    
    def __runCommand(self, command, user=None, dbname=DBNAME):
        return PSQL.run_sql_command(host=self.host, username=user, sql_cmd=command, dbname=dbname)
       
        
    def __getCount(self, output):
        if len(output) < 1:
            return -1
	elif len(output) <=2:
	    return int(output[0])
	else:
	    return int(output[-2])
    
    def __getRows(self, output):
        if len(output) < 1:
            return [];
        return [ line.strip() for line in output[:-1] ]
        
        
    def __addEntryHBA(self, usr):
        """
        Add an entry to $MASTER_DATA_DIRECTORY/pg_hba.conf,
        so that a specified user can access the database from any IP addresses.
         Commands:
         "local all usr trust" >> $MASTER_DATA_DIRECTORY/pg_hba.conf0
            gpstop -u
        @note: If the entry already exists, nothing will be done.
        @param usr: name of the user to be added to pg_hba.conf.
        @param ent: an entry object from PgHba.py which represents a PGHBA_CONF entry
        @raise GpUserRoleError: if pg_hba.conf can not be accessed.
        @return: None.
        
        @change: Johnny Soedomo
        Uses fileUtil to search and append content
        
        @change: Ritesh Jaltare
        Uses PgHba.py to create entry and remove fileUtil
        Uses lib.util.network to get ipv4 and ipv6 address
        Uses lib.modules.gpdb.Gpstop.gpstop_reload
        Add an entry, uses lib.Globals, use location of PGPASSFILE, default is home folder ~/.pgpass
        """
        try:
            #add an entry to the PGPASS file if the the user doesn't exist already
            if not self.searchForUserEntry(PGPASSFILE, r'%s:.*:%s:.*' % (self.host, usr)):   
                if self.password is None:
                        fileUtil.append_content("%s:*:%s:%s:password" % (self.host, self.pgpassdb, usr), PGPASSFILE, self.host)
                else:
                        fileUtil.append_content("%s:*:%s:%s:%s" % (self.host, self.pgpassdb, usr, self.password), PGPASSFILE, self.host)
            #search if the entry for user already exists in PGHBA_CONF          
            if self.searchForUserEntry(PGHBA_CONF, r'.*\s+.*\s+%s\s+.*\s+.*' % usr):
                return False
            pghba = PgHba()
            netobj = network()
            ipv4 = netobj.get_ipv4()
            ipv6 = netobj.get_ipv6()
            
            #add entry if ipv4 address exits
            if ipv4 is not "":
                ipv4 += "/32"
                ent1 = Entry(entry_type=self.entry_type, database=self.db,user=usr, authmethod=self.authmethod, address=ipv4)
                pghba.add_entry(ent1)
                
            #add entry if ipv6 address exists
            if ipv6 is not "":
                ipv6 += "/128"
		ent2 = Entry(entry_type=self.entry_type, database=self.db, user=usr, authmethod=self.authmethod, address=ipv6)
                pghba.add_entry(ent2)    
            pghba.write()
            return True
        except Exception, e:
            raise GpUserRoleError("Error in adding user %s to pg_hba.conf" % (usr))

   
    def checkUserExists(self, userName):
        """
        Searches the catalog to check whether the specified user exists or not 
        if user exists, returns false.
        else true
        """
        sql_command = "select usename from pg_user where usename = '%s'" % userName
        result = PSQL.run_sql_command(sql_cmd=sql_command, flags='-q -t', dbname=DBNAME)
        result = result.strip()
        user_out = result.split('\n')
        user = ''
        for line in user_out:
            if line is not None:
                user = line
	if user != userName:
	    return True
        else:
	    return False
    
    def searchForUserEntry(self, filename, searchpattern, host="localhost"):
        """
        Searches for an entry for the specified user in a specified file. 
        @param filename: the name of the file to be searched
        @param searchpattern: the search string in regular expression
        """
        
        filecontent = fileUtil.get_content(filename, hostname="localhost")
        for entry in filecontent:
            if mstring.match(entry, searchpattern):
                return True
        return False        
                
        
    
    
    def createUser(self, usr, role=""):
        """
        Create a database user with the specific role, using the following SQL:    
            CREATE USER usr
        @param usr: the name of the user to be created.
        @param role: LOGIN, SUPERUSER, etc.
        @return: True if successful.
        @raise GpUserRoleError: if creation fails.
        
       """
        
	if self.checkUserExists(usr) is False:
            print "User %s already created"%usr
	    return False

        if self.__addEntryHBA(usr) is False:
            print "Entry for user %s exists in PGHBA"%usr

	self.__runCommand("CREATE USER %s PASSWORD %s %s" % (usr, self.password, role))
        
        if self.checkUserExists(usr):
            raise GpUserRoleError("CREATE USER %s failed" % usr)
	else:
            self.gpstop.run_gpstop_cmd(reload=True)
	    return True

    def createUserWithRole(self, usr, role):
        """
        Create a database user with the specific role, using the following SQL:    
        CREATE USER usr IN ROLE role
        @param usr: the name of the user to be created.
        @param role: the role to be assigned to usr.
        @return: True if successful.
        @raise GpUserRoleError: if creation fails.
        
        """
        
        if self.validateUserCreated(usr):
            raise GpUserRoleError("Role %s already exists" % usr)
        if not self.validateUserCreated(role):
            raise GpUserRoleError("Role %s dose not exist" % role)
        
        self.__runCommand("CREATE USER %s IN ROLE %s PASSWORD %s" % (usr, role, self.password))
        if not self.memberOfGroup(usr, role):
            raise GpUserRoleError("CREATE USER %s failed" % usr)
        
        self.__addEntryHBA(usr)
        return True
    
    def createUserWithPrivileges(self, usr, *privileges):
        """
        Create a database user with the specified privileges, using the following SQL
            CREATE USER usr privileges
        @attention: if NOLOGIN is specified in the privileges list, a ROLE will be created instead.
        @param usr: the name of the user to be created.
        
        @param privileges: a list of privileges to be assigned. LOGIN will be added by default.
        @return: True if successful.
        @raise GpUserRoleError: if creation fails.
        
        
        """
        privileges = self.__normalizePrivileges(*privileges)
        try: 
            if 'NOLOGIN' in privileges:
                return self.createRoleWithPrivileges(usr, *privileges)
            else:
                if not 'LOGIN' in privileges:
                    privileges.append('LOGIN')
                return self.createRoleWithPrivileges(usr, *privileges)
            
        except GpUserRoleError, e:
            raise GpUserRoleError("CREATE USER %s failed" % usr)
    
    def createRoleWithPrivileges(self, role, *privileges):
        """
        Create a database role with the specified privileges, using the following SQL
            CREATE ROLE role privileges
        @attention: if LOGIN is specified in the privileges list, a USER will be created instead.
        @param role: the name of the role to be created
        @param privileges: a list of privileges to be assigned.
        @return: True if successful.
        @raise GpUserRoleError: if creation fails.
        
        @change: Ritesh Jaltare
        added  @param ent: an entry object from PgHba.py which represents a PGHBA_CONF entry
        """
        privileges = self.__normalizePrivileges(*privileges)
        if self.validateUserCreated(role):
            raise GpUserRoleError("Role %s already exists" % role)
        self.__runCommand("CREATE ROLE %s %s" % (role, ' '.join(privileges)))
        if not self.validateUserPrivileges(role, *privileges):
            raise GpUserRoleError("CREATE ROLE %s failed" % role)
        if 'LOGIN' in privileges:
            self.__addEntryHBA(role)
        return True

    def alterRolePrivileges(self, role, *privileges):
        """
        Alter the database role with the specified privileges, using the following SQL:
            ALTER ROLE role privileges
        @param role: the name of the role to be altered
        @param privileges: a list of privileges to be assigned.
        @return: True if successful.
        @raise GpUserRoleError: if alter fails.
        """
        if not self.validateUserCreated(role):
            raise GpUserRoleError("Role %s dose not exist" % role)
        privileges = self.__normalizePrivileges(*privileges)
        self.__runCommand("ALTER ROLE %s %s" % (role, ' '.join(privileges)))
        if not self.validateUserPrivileges(role, *privileges):
            raise GpUserRoleError("ALTER ROLE %s failed" % role)
        return True
    
    
    def dropRole(self, role):
        """
        Drop the specified database role, using the following SQL
            DROP ROLE role
        @param role: the name of the role to be dropped.
        @raise GpUserRoleError: if DROP fails.
        """
	
	out = self.__runCommand("DROP OWNED BY %s CASCADE" % role)
	out = self.__runCommand("DROP ROLE %s" % role)
        if self.checkUserExists(role) is False:
		raise GpUserRoleError("DROP ROLE %s failed" % role)
	else: 	
	    	fileUtil.backup('%s/pg_hba.conf' % (MASTER_DATA_DIRECTORY), hostname="localhost", backupext=".orig")
            	fileUtil.delete_content(r'%s/pg_hba.conf.orig' % (MASTER_DATA_DIRECTORY), r'.*\s+.*\s+%s\s+.*\s+.*' % role, hostname="localhost")
            	fileUtil.copy(r'%s/pg_hba.conf.orig' % (MASTER_DATA_DIRECTORY), r'%s/pg_hba.conf' % (MASTER_DATA_DIRECTORY), hostlist=["localhost"])		
            	fileUtil.backup(PGPASSFILE, hostname="localhost", backupext=".orig")
	    	fileUtil.delete_content(PGPASSFILE+".orig", r'%s:.*:%s:.*' % (self.host,role) ,hostname="localhost")
	    	fileUtil.copy(PGPASSFILE+".orig", PGPASSFILE, hostlist=["localhost"])
	    	os.chmod(PGPASSFILE , 0600)
            	os.remove(PGPASSFILE+".orig")
	    	os.remove('%s/pg_hba.conf.orig' % (MASTER_DATA_DIRECTORY))	
                self.gpstop.run_gpstop_cmd(reload=True)
    
    def validateUserCreated(self, usr):
        """
        Validate if the specified user was created successfully.
        @param usr: the name of the user/role
        @return: true if the specified user was created successfully; otherwise, return false.
        """
        
        out = self.__runCommand("select count(1) from pg_roles where rolname = '%s'" % usr)
        return self.__getCount(out) == 1
    
    def validateUserRoleAssigned(self, usr, role):
        """
        Validate if the user has the specified role.
        @param usr: the name of the user.
        @param role: the name of the role.
        @return: true if usr has the specified role; otherwise, return false.
        """
        return self.memberOfGroup(usr, role)
    
    def validateUserPrivileges(self, usr, *privileges):
        """
        Validate if the specified user has the specified privileges.
        @param usr: the name of the role to be validated
        @param privileges: a list of privileges to be validated.
        
        @return: true if the specified user has the specified privileges; otherwise, return false.
        @raise GpUserRoleError: if the specified privileges conflict or are undefined.
        """
        privileges = self.__normalizePrivileges(*privileges)
        whereStmt = ' and '.join([GpUserRole.PrivilegesSelect[pri] for pri in privileges])
        out = self.__runCommand("select count(1) from pg_roles where rolname = '%s' and %s" % (usr, whereStmt))
        return self.__getCount(out) == 1

    

    def getRoleMembers(self, role):
        """
        Get a list of the users that are members of the specified database role.
        @param role: the name of the specified role.
        @return: a name list of the role's members.
        """
        selectMemberStmt = "select m.rolname \
            from pg_auth_members au, pg_roles g, pg_roles m \
            where g.oid = au.roleid and m.oid = au.member \
            and g.rolname = '%s'" % (role)
        out = self.__runCommand(selectMemberStmt)
        return self.__getRows(out)
        
    
    def memberOfGroup(self, role, group):
        """
        Check if the specified role is the member of the specified database group
        @param role: the name of the specified role
        @param group: the name of the specified group
        @return: true if the specified role belongs to the specified database group
        """
        membershipStmt = "select count(1) \
            from pg_auth_members au, pg_roles g, pg_roles m \
            where g.oid = au.roleid and m.oid = au.member \
            and g.rolname = '%s' and m.rolname = '%s'" % (group, role)
        out = self.__runCommand(membershipStmt)
        return self.__getCount(out) == 1

