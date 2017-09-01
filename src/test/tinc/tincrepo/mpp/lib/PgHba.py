#!python

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

pg_hba.py - utilities for handling the pg_hba.conf file
"""

import os
import re
import sys

# only necessary for module testing:
# import pprint


ENTRY_FIELDS = ( 'type', 'database', 'user', 'ipmask', 'address',
                'authmethod', 'auth_options' )

class PgHba( object ):
    '''Class for parsing/searching/editing the pg_hba.conf file'''

    # Modified regex to support all record formats in pg_hba and also 
    # support tabs as well as spaces. From postgres docs, a pg_hba 
    # record can have one of the following seven formats:
    #  1. local      database  user  auth-method  [auth-option]
    #  2. host       database  user  CIDR-address  auth-method  [auth-option]
    #  3. hostssl    database  user  CIDR-address  auth-method  [auth-option]
    #  4. hostnossl  database  user  CIDR-address  auth-method  [auth-option]
    #  5. host       database  user  IP-address  IP-mask  auth-method  [auth-option]
    #  6. hostssl    database  user  IP-address  IP-mask  auth-method  [auth-option]
    #  7. hostnossl  database  user  IP-address  IP-mask  auth-method  [auth-option]
    #
    # _re_entry1 matches record type 1
    # _re_entry2 matches record types 2,3,4
    # _re_entry3 matches record types 5,6,7
    _re_entry1 = re.compile( r'^\s*(?P<type>local)(\s|\t)+(?P<database>\S+)(\s|\t)+(?P<user>\S+)(\s|\t)+(?P<method>\S+)(\s|\t)+(\b\S+=\S+\b\s*)*' )
    _re_entry2 = re.compile( r'^\s*(?P<type>\S+)(\s|\t)+(?P<database>\S+)(\s|\t)+(?P<user>\S+)(\s|\t)+(?P<address>\S+)(\s|\t)+(?P<method>\S+)(\s|\t)+(\b\S+=\S+\b\s*)*' )
    _re_entry3 = re.compile( r'^\s*(?P<type>\S+)(\s|\t)+(?P<database>\S+)(\s|\t)+(?P<user>\S+)(\s|\t)+(?P<address>\S+)(\s|\t)+(?P<ipmask>\d+\.\d+.\d+.\d+\/*\d*)(\s|\t)+(?P<method>\S+)(\s|\t)+(\b\S+=\S+\b\s*)*' )

    # ignore comments etc
    _re_ignore = re.compile( r'^(\s*#.*|\s*)$' )
    
    
    def __init__( self, file_path = None ):
        '''
        Return a new, empty pg_hba object.
        @param file_path: path to a pg_hba.conf file to parse/edit.
        '''
        if not file_path:
            if not os.environ.has_key( 'MASTER_DATA_DIRECTORY' ):
                raise Exception, \
                   "A pg_hba.conf file must be provided or MASTER_DATA_DIRECTORY must point to one"
            self._file_path = os.path.join( os.environ['MASTER_DATA_DIRECTORY'], 'pg_hba.conf' )
        else:
            self._file_path = file_path
        self._parse()

    def _parse( self ):
        '''Read and parse a pg_hba.conf file'''

        self._contents = list()

        fdr = open( self._file_path, 'rb' )

        for line in fdr:
            if self._re_ignore.match( line ):
                self._contents.append( line )
            else:
                #Match with one of the three possible regex defined above
                line_match = self._re_entry1.match( line )
                if line_match is not None:
                    #Parse contents and form entry. auth_opts_index will be 8 for _re_entry1
                    self._parse_contents_match( line_match, 8 )
                    continue
                
                line_match = self._re_entry2.match( line )
                if line_match is not None:
                    #Parse contents and form entry. auth_opts_index will be 10 for _re_entry2
                    self._parse_contents_match( line_match, 10 )
                    continue
                
                line_match = self._re_entry3.match( line )
                if line_match is not None:
                    #Parse contents and form entry. auth_opts_index will be 12 for _re_entry3
                    self._parse_contents_match( line_match, 12 )
                    continue
                else:
                    raise PgHbaException, "error parsing file. Did not match expected regex." + line
    
              
        fdr.close()
        
    def _parse_contents_match( self, line_match, auth_opts_index ):
        '''
        Parses match object to form an appropriate Entry object. 
        @param line_match: line_match object returned by re.match()
        @param auth_opts_index: The index at which auth_opts should be located. 8 for _re_entry1, 10 for _re_entry2 and 12 for _re_entry3
        '''
        match_groups = line_match.groups()
        auth_opts = dict()
        try:
            for grp in match_groups[auth_opts_index:]:
                if grp is None:
                    break
                key, val = grp.split( '=' )
                auth_opts[key] = val
        except IndexError:
            pass
        
        if auth_opts_index == 8:
            ent = Entry( entry_type = line_match.group( 'type' ),
                         database = line_match.group( 'database' ),
                         user = line_match.group( 'user' ),
                         authmethod = line_match.group( 'method' ),
                         auth_options = auth_opts )
            self._contents.append( ent )
            return
        
        if auth_opts_index == 10:
            ent = Entry( entry_type = line_match.group( 'type' ),
                         database = line_match.group( 'database' ),
                         user = line_match.group( 'user' ),
                         address = line_match.group( 'address' ),
                         authmethod = line_match.group( 'method' ),
                         auth_options = auth_opts )
            self._contents.append( ent )
            return
        
        if auth_opts_index == 12:
            ent = Entry( entry_type = line_match.group( 'type' ),
                         database = line_match.group( 'database' ),
                         user = line_match.group( 'user' ),
                         address = line_match.group( 'address' ),
                         ipmask = line_match.group( 'ipmask' ),
                         authmethod = line_match.group( 'method' ),
                         auth_options = auth_opts )
            self._contents.append( ent )
            return
            
    def write( self, out_file = None ):
        '''Write the pg_hba.conf file from its parsed contents.'''
        if not out_file:
            out_file = self._file_path
        fdw = open( out_file, 'wb' )
        for line in self._contents:
            if isinstance( line, Entry ) and line.is_deleted():
                continue
            fdw.write( str( line ) )
        fdw.close()

    def search( self, **kwargs ):
        '''Search for an entry in a parsed file, return list of matches.
        @param type: the type field
        @param database:
        @param user:
        @param authmethod:
        @param type:
        @param database:
        @param user:
        @param auth_options:
        '''
        for k in kwargs.keys():
            if k not in ENTRY_FIELDS:
                raise PgHbaException, "%s is not a valid entry field" % k

        result = list()
        match_mapping = dict()

        for line in self._contents:
            if not isinstance( line, Entry ):
                continue

            for key in kwargs.keys():
                match_mapping[key] = False
            
            all_match = True
            
            for key, val in kwargs.items():
                attr = line.get_attribute( key )
                if attr:
                    if attr.count( val ) > 0:
                        match_mapping[key] = True

            for key, val in match_mapping.items():
                if val == False:
                    all_match = False

            if all_match:
                result.append( line )

        return result

    def add_entry(self, ent):
        '''add an entry object to the end of the file'''
        self._contents.append( ent )

    def get_contents(self):
        '''return the parsed contents as a list'''
        return self._contents

    def get_contents_without_comments(self):
        '''return the parsed contents, without comments, as a list'''
        result = []
        for line in self._contents:
            if not isinstance(line, Entry):
                continue
            result.append(line)
        return result

    def __str__( self ):
        '''return a string representation of the contents of the file'''
        buf = ""
        for line in self._contents:
            buf += str( line )
        return buf


class PgHbaException( Exception ):
    '''exception that can be raised by PgHba objects'''
    pass


class Entry( object ):
    '''container class for representing an entry in a pg_hba.conf file'''

    _types = ( 'local', 'host', 'hostssl', 'hostnossl' )
    _methods = ( 'trust', 'reject', 'md5', 'password', 'gss', 'sspi',
                'krb5', 'ident', 'pam', 'ldap', 'radius', 'cert' )
    
    def __init__( self, entry_type = None, database = None,
                       user = None, authmethod = None,
                       ipmask = None, address = None,
                       auth_options = None ):
        '''
        Create a new pga_conf.entry object.
        @param type: the type field
        @param database: the database field
        @param user: the user field
        @param authmethod: the authmethod field
        @param ipmask: the ipmask field
        @param address: the address field
        @param auth_options: the auth options field
        '''
        
        if entry_type is None or \
           database is None or \
           user is None or \
           authmethod is None:
            raise PgHbaException, \
              "pg_hba.entry must provide with type, database, address, user"

        if auth_options is None:
            auth_options = dict()

        self._fields = dict()

        self.set_type( entry_type )
        self._fields['database'] = database
        self._fields['user'] = user
        self._fields['ipmask'] = ipmask
        self._fields['address'] = address
        self.set_authmethod( authmethod )
        self._fields['auth_options'] = auth_options

        self._deleted = False

    def delete( self ):
        '''mark this entry as deleted'''
        self._deleted = True

    def is_deleted( self ):
        '''return True/False if this entry has been marked as deleted'''
        return self._deleted

    def __str__( self ):
        '''return a string that can be inserted into a pg_hba.conf file'''
        a_opts = ""
        if self._fields['auth_options']:
            a_opts = " ".join( [ "%s=%s" % ( key, val ) \
                            for key, val in self._fields['auth_options'].items() ] )
        return "%s \t %s \t %s \t %s \t %s \t %s \t %s\n" % \
               ( self._fields['type'],
                 self._fields['database'],
                 self._fields['user'],
                 self._fields['address'] or "",
                 self._fields['ipmask'] or "",
                 self._fields['authmethod'],
                 a_opts )

    def get_attribute( self, name ):
        '''get entry fields by attribute name'''
        return self._fields[name]

    def get_type( self ):
        '''get entry type field'''
        return self._fields['type']

    def get_database( self ):
        '''get entry database field'''
        return self._fields['database']

    def get_user( self ):
        '''get entry user field'''
        return self._fields['user']

    def get_address( self ):
        '''get entry address field'''
        return self._fields['address']

    def get_authmethod( self ):
        '''get entry authmethod field'''
        return self._fields['authmethod']

    def get_auth_options( self ):
        '''get entry auth options field, as a dict'''
        return self._fields['auth_options']

    def get_ipmask( self ):
        '''get entry ipmask field'''
        return self._fields['ipmask']

    def set_type( self, entry_type ):
        '''set entry type field'''
        if entry_type not in self._types:
            raise PgHbaException, "type '%s' unsupported, supported types are: %s" % \
                  ( entry_type, ', '.join( ["'%s'" % sup_type for sup_type in self._types] ) )
        self._fields['type'] = entry_type

    def set_database( self, database ):
        '''set entry database field'''
        self._fields['database'] = database

    def set_user( self, user ):
        '''set entry user field'''
        self._fields['user'] = user

    def set_address( self, address ):
        '''set entry address field'''
        self._fields['address'] = address

    def set_authmethod( self, authmethod ):
        '''set entry authmethod field'''
        if authmethod not in self._methods:
            raise PgHbaException, "auth method '%s' unsupported, supported methods are: %s" % \
                  ( authmethod, ', '.join( ["'%s'" % method for method in self._methods ] ) )
        self._fields['authmethod'] = authmethod

    def set_ipmask( self, ipmask ):
        '''set entry ipmask field'''
        self._fields['ipmask'] = ipmask

    def set_auth_options( self, key, val ):
        '''set entry auth option, by name'''
        self._fields['auth_options'][key] = val



if __name__ == '__main__':
    pg_file = PgHba( sys.argv[1] )

    # p = pprint.PrettyPrinter(indent=2)
    # for c in pg_file._get_contents():
    #    print c

    # print pg_file

    # result = pg_file.search(database='all', entry_type='local')

    #for e in result:
    #    print e,

    res = pg_file.search( database = 'postgres', authmethod = 'md5' )

    try:
        print res[0]
        res[0].delete()
    except IndexError:
        print "None found."

    new_ent = Entry( entry_type = 'local', database = 'test_database',
                    user = 'test_user', authmethod = 'sspi' )
    pg_file.add_entry( new_ent )

    pg_file.write( 'example-processed-pg_hba.conf' )
