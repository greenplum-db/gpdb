#!/bin/bash
##
## Courtesy - http://stevemorin.blogspot.com/2007/10/bash-get-self-directory-trick.html
## Updated to run on rhel3 (kite12)
##

script_path="$(cd $(dirname $0); pwd -P)/$(basename $0)"

[[ ! -f "$script_path" ]] && script_path="$(cd $(/usr/bin/dirname "$0"); pwd -P)/$(basename "$0")"
[[ ! -f "$script_path" ]] && script_path="" && echo 'No full path to running script found!' && exit 1

script_dir="${script_path%/*}"
BLDWRAP_TOP="${script_dir%/*}"

##
## Pick up some platform defines
##

. ${BLDWRAP_TOP}/lib/platform_defines.sh

##
## Set installer type
##

INSTALLER_TYPE="$1"
echo "INSTALLER_TYPE: ${INSTALLER_TYPE}"

## ======================================================================
## ======================================================================

add_platform_check() {

	if [ "${MPP_ARCH}" = "OSX-i386" ] ; then
		cat <<-EOF_HEADER
			platform=Darwin
			arch=i386
			if [ \`uname -s\` != "\${platform}" ] ; then
			    echo "Error: Installer will only install on ${platform} \${arch}"
			    exit 1
			else
			    if [ \`uname -p\` != "\${arch}" ] ; then
			        echo "Error: Installer will only install on \${platform} \${arch}"
			        exit 1
			    fi
			fi
		EOF_HEADER
	fi
	
	if [ "${MPP_ARCH}" = "RHEL5-x86_64" ] ; then
		cat <<-EOF_HEADER
			platform="RedHat/CentOS"
			arch=x86_64
			if [ -f /etc/redhat-release ]; then
			    if [ \`uname -m\` != "\${arch}" ] ; then
			        echo "Installer will only install on \${platform} \${arch}"
			        exit 1
			    fi
			else
			    echo "Installer will only install on \${platform} \${arch}"
			    exit 1
			fi
		EOF_HEADER
	fi
	
	if [ "${MPP_ARCH}" = "SuSE10-x86_64" ] ; then
		cat <<-EOF_HEADER
			platform="SuSE"
			arch=x86_64
			if [ -f /etc/SuSE-release ]; then
			    if [ \`uname -m\` != "\${arch}" ] ; then
			        echo "Installer will only install on \${platform} \${arch}"
			        exit 1
			    fi
			else
			    echo "Installer will only install on \${platform} \${arch}"
			    exit 1
			fi
		EOF_HEADER
	fi
}
	
## ======================================================================
## ======================================================================
	
generate-pulse-installer-header(){

	if [ -n "$1" ] ; then
	    DEFAULT_INSTALL_PATH=/usr/local/greenplum-db-$1
	else
	    DEFAULT_INSTALL_PATH=/usr/local/greenplum-db
	fi
	
	cat <<-EOF_HEADER
		#!/bin/sh
		
		#Check for needed tools
		UTILS="sed tar awk cat mkdir tail mv more"
		for util in \${UTILS}; do
		    which \${util} > /dev/null 2>&1
		    if [ \$? != 0 ] ; then
		cat <<EOF
		********************************************************************************
		Error: \${util} was not found in your path.
		       \${util} is needed to run this installer.
		       Please add \${util} to your path before running the installer again.
		       Exiting installer.
		********************************************************************************
		EOF
		       exit 1
		    fi
		done
		
		#Verify that tar in path is GNU tar. If not, try using gtar.
		#If gtar is not found, exit.
		TAR=
		tar --version > /dev/null 2>&1
		if [ \$? = 0 ] ; then
		    TAR=tar
		else
		    which gtar > /dev/null 2>&1
		    if [ \$? = 0 ] ; then
		        gtar --version > /dev/null 2>&1
		        if [ \$? = 0 ] ; then
		            TAR=gtar
		        fi
		    fi
		fi
		if [ -z \${TAR} ] ; then
		cat <<EOF
		********************************************************************************
		Error: GNU tar is needed to extract this installer.
		       Please add it to your path before running the installer again.
		       Exiting installer.
		********************************************************************************
		EOF
		    exit 1
		fi
	EOF_HEADER
	
	add_platform_check

	cat <<-EOF_HEADER
		SKIP=\`awk '/^__END_HEADER__/ {print NR + 1; exit 0; }' "\$0"\`
		
		more << EOF
		
		********************************************************************************
		You must read and accept the Pivotal Database license agreement
		before installing
		********************************************************************************
		
	EOF_HEADER

	cat ${BLDWRAP_TOP}/../../../licenses/GPDB-LICENSE.txt

	cat <<-EOF_HEADER
	
		I HAVE READ AND AGREE TO THE TERMS OF THE ABOVE PIVOTAL SOFTWARE
		LICENSE AGREEMENT.

		EOF
		
		agreed=
		while [ -z "\${agreed}" ] ; do
		    cat << EOF
		
		********************************************************************************
		Do you accept the Pivotal Database license agreement? [yes|no]
		********************************************************************************
		
		EOF
		    read reply leftover
		        case \$reply in
		           [yY] | [yY][eE][sS])
		                agreed=1
		                ;;
		           [nN] | [nN][oO])
		                cat << EOF
		
		********************************************************************************
		You must accept the license agreement in order to install Greenplum Database
		********************************************************************************
		                             
		                   **************************************** 
		                   *          Exiting installer           *
		                   **************************************** 
		
		EOF
		                exit 1
		                ;;
		        esac
		done
		
		installPath=
		defaultInstallPath=${DEFAULT_INSTALL_PATH}
		while [ -z "\${installPath}" ] ; do
		    cat <<-EOF
				
				********************************************************************************
				Provide the installation path for Greenplum Database or press ENTER to 
				accept the default installation path: ${DEFAULT_INSTALL_PATH}
				********************************************************************************
				
			EOF

		    read installPath leftover

		    if [ -z "\${installPath}" ] ; then
		        installPath=\${defaultInstallPath}
		    fi

		    if echo "\${installPath}" | grep ' ' > /dev/null; then
			    cat <<-EOF
					
					********************************************************************************
					WARNING: Spaces are not allowed in the installation path.  Please specify
					         an installation path without an embedded space.
					********************************************************************************
					
				EOF
                installPath=
		    else
			    pathVerification=
			    while [ -z "\${pathVerification}" ] ; do
			        cat <<-EOF
						
						********************************************************************************
						Install Greenplum Database into <\${installPath}>? [yes|no]
						********************************************************************************
						
					EOF
	
			        read pathVerification leftover
	
			        case \$pathVerification in
			            [yY] | [yY][eE][sS])
			                pathVerification=1
			                ;;
			            [nN] | [nN][oO])
			                installPath=
			               ;;
			        esac
			    done
			fi
		done
		
		if [ ! -d "\${installPath}" ] ; then
		    agreed=
		    while [ -z "\${agreed}" ] ; do
		    cat << EOF
		
		********************************************************************************
		\${installPath} does not exist.
		Create \${installPath} ? [yes|no ]
		(Selecting no will exit the installer)
		********************************************************************************
		
		EOF
		    read reply leftover
		        case \$reply in
		           [yY] | [yY][eE][sS])
		                agreed=1
		                ;;
		           [nN] | [nN][oO])
		                cat << EOF
		
		********************************************************************************
		                             Exiting the installer
		********************************************************************************
		
		EOF
		                exit 1
		                ;;
		        esac
		    done
		    mkdir -p \${installPath}
		fi
		
		if [ ! -w "\${installPath}" ] ; then
		    echo "\${installPath} does not appear to be writeable for your user account."
		    echo "Continue?"
		    continue=
		    while [ -z "\${continue}" ] ; do
		        read continue leftover
		            case \${continue} in
		                [yY] | [yY][eE][sS])
		                    continue=1
		                    ;;
		                [nN] | [nN][oO])
		                    echo "Exiting Greenplum Database installation."
		                    exit 1
		                    ;;
		            esac
		    done
		fi
		
		if [ ! -d \${installPath} ] ; then
		    echo "Creating \${installPath}"
		    mkdir -p \${installPath}
		    if [ \$? -ne "0" ] ; then
		        echo "Error creating \${installPath}"
		        exit 1
		    fi
		fi 

	EOF_HEADER

	cat <<-EOF_HEADER

		echo ""
		echo "Extracting product to \${installPath}"
		echo ""
	EOF_HEADER
	
	#Solaris tail does not recognize -n
	if [ "`uname -s`" = "SunOS" ]; then
	    cat <<-EOF
			tail +\${SKIP} "\$0" | \${TAR} zxf - -C \${installPath}
		EOF
	else
	    cat <<-EOF
			tail -n +\${SKIP} "\$0" | \${TAR} zxf - -C \${installPath}
		EOF
	fi
	
	cat <<-EOF_HEADER
		if [ \$? -ne 0 ] ; then
		    cat <<-EOF
				********************************************************************************
				********************************************************************************
				                          Error in extracting Greenplum Database
				                               Installation failed
				********************************************************************************
				********************************************************************************
				
			EOF
		    exit 1
		fi
		
		installDir=\`basename \${installPath}\`
		symlinkPath=\`dirname \${installPath}\`
		symlinkLink=greenplum-db
		if [ x"\${symlinkLink}" != x"\${installDir}" ]; then
		    if [ "\`ls \${symlinkPath}/\${symlinkLink} 2> /dev/null\`" = "" ]; then
		        ln -s "./\${installDir}" "\${symlinkPath}/\${symlinkLink}"
		    fi
		fi
		sed "s,^GPHOME.*,GPHOME=\${installPath}," \${installPath}/greenplum_path.sh > \${installPath}/greenplum_path.sh.tmp
		mv \${installPath}/greenplum_path.sh.tmp \${installPath}/greenplum_path.sh

	EOF_HEADER

	cat <<-EOF_HEADER
	    cat <<-EOF
			********************************************************************************
			Installation complete.
			Greenplum Database is installed in \${installPath}
	
			Pivotal Greenplum documentation is available
			for download at http://gpdb.docs.pivotal.io
			********************************************************************************
		EOF
		
		exit 0
		
		__END_HEADER__
	EOF_HEADER
}

## ======================================================================
## ======================================================================
	
generate-applicance-installer-header(){
	
	cat <<-EOF_HEADER
		#!/bin/sh

		installPath=/usr/local/GP-$VERSION
		install_log=\$( pwd )/install-\$( date +%d%m%y-%H%M%S ).log

		cat > \${install_log} <<-EOF
			======================================================================
			                             Greenplum DB
			                    Appliance Automated Installer
			----------------------------------------------------------------------
			Timestamp ......... : \$( date )
			Product Installer.. : $INSTALLER_NAME
			Product Version ... : $VERSION
			Install Dir ....... : \${installPath}
			Install Log file .. : \${install_log}
			======================================================================
			
		EOF

		cat \${install_log}

		# Make sure only root can run our script
		if [ "\`id -u \`" != "0" ]; then
			cat <<-EOF
				======================================================================
				ERROR: This script must be run as root.
				======================================================================
			EOF
		   exit 1
		fi

		# Make sure hostfile exists in current working directory
		if [ ! -f \`pwd\`/hostfile ]; then
			cat <<-EOF
				======================================================================
				ERROR: hostfile does not exist (\`pwd\`/hostfile)
				======================================================================
			EOF
		   exit 1
		fi

		#Check for needed tools
		UTILS="sed tar awk cat mkdir tail mv more"
		for util in \${UTILS}; do
		    which \${util} > /dev/null 2>&1
		    if [ \$? != 0 ] ; then
				cat <<-EOF
					======================================================================
					ERROR: \${util} was not found in your path.
					       Please add \${util} to your path before running
					       the installer again.
					       Exiting installer.
					======================================================================
				EOF
		       exit 1
		    fi
		done
		
		#Verify that tar in path is GNU tar. If not, try using gtar.
		#If gtar is not found, exit.
		TAR=
		tar --version > /dev/null 2>&1
		if [ \$? = 0 ] ; then
		    TAR=tar
		else
		    which gtar > /dev/null 2>&1
		    if [ \$? = 0 ] ; then
		        gtar --version > /dev/null 2>&1
		        if [ \$? = 0 ] ; then
		            TAR=gtar
		        fi
		    fi
		fi

		if [ -z \${TAR} ] ; then
			cat <<-EOF
				======================================================================
				ERROR: GNU tar is needed to extract this installer.
				       Please add it to your path before running the installer again.
				       Exiting installer.
				======================================================================
			EOF
		    exit 1
		fi
	EOF_HEADER
	
	add_platform_check

	cat <<-EOF_HEADER
		SKIP=\`awk '/^__END_HEADER__/ {print NR + 1; exit 0; }' "\$0"\`
		
		if [ ! -d \${installPath} ] ; then
		    echo "Creating \${installPath}"
		    mkdir -p \${installPath}
		    if [ \$? -ne "0" ] ; then
			    cat <<-EOF
					======================================================================
					ERROR: Error creating \${installPath}
					======================================================================
				EOF
		        exit 1
		    fi
		fi 
		
	EOF_HEADER
	
	#Solaris tail does not recognize -n
	if [ "`uname -s`" = "SunOS" ]; then
	    cat <<-EOF
			tail +\${SKIP} "\$0" | \${TAR} zxf - -C \${installPath}
		EOF
	else
	    cat <<-EOF
			tail -n +\${SKIP} "\$0" | \${TAR} zxf - -C \${installPath}
		EOF
	fi
	
	cat <<-EOF_HEADER
		if [ \$? -ne 0 ] ; then
		    cat <<-EOF
				======================================================================
				ERROR: Extraction failed
				======================================================================
			EOF
		    exit 1
		fi
		
		cat <<-EOF
			======================================================================
			Executing Post Appliance Installation Steps
			======================================================================
		
		EOF

		##
		## Update greenplum_path.sh
		##

		symlinkPath="\`dirname \${installPath}\`/greenplum-db"
		sed "s,^GPHOME.*,GPHOME=\${installPath}," \${installPath}/greenplum_path.sh > \${installPath}/greenplum_path.sh.tmp
		mv \${installPath}/greenplum_path.sh.tmp \${installPath}/greenplum_path.sh
		
		echo "Executing: source \${installPath}/greenplum_path.sh" | tee -a \${install_log}
		source \${installPath}/greenplum_path.sh

		echo "" | tee -a \${install_log}

		##
		## Migrate packages
		##

		# If the symlink exists, we consider that this may be a new, separate GPDB version.
		if [ -d \${symlinkPath} ]; then
			# If the symlink points to the current installPath, this is likely a reinstall.
			# TODO: In the case of reinstall, gppkg --migrate will error out because \${symlinkPath} == \${installPath}.
			echo "Executing: gppkg --migrate \${symlinkPath} \${installPath} --verbose" | tee -a \${install_log}
			gppkg --migrate \${symlinkPath} \${installPath} --verbose > \${install_log} 2>&1 
			if [ \$? -ne 0 ] ; then
				cat <<-EOF

					ERROR: gppkg failed. Continuing...

				EOF
			fi
		fi

		echo "" | tee -a \${install_log}

		##
		## Setup new symlink
		##

		rm -f \${symlinkPath}
		ln -s "\${installPath}" "\${symlinkPath}"

		##
		## Setup segments
		##

		# The greenplum_path.sh file must be sourced just after symlink modification and
		# just prior to gpseginstall invocation. gpseginstall depends on \$GPHOME pointing 
		# to a symlink to determine whether it too must set up the symlinks on the segments.
		source \${installPath}/greenplum_path.sh

		echo "Executing: gpseginstall --file hostfile -c csv 2>&1 | tee -a \${install_log}" | tee -a \${install_log}
		gpseginstall --file hostfile -c csv 2>&1 | tee -a \${install_log}

		if [ \$? -ne 0 ] ; then
		    cat <<-EOF
				======================================================================
				ERROR: gpseginstall failed
				======================================================================
			EOF
		    exit 1
		fi
		
		cat <<-EOF
			======================================================================
			Installation complete
			======================================================================
		EOF
		
		exit 0
		
		__END_HEADER__
	EOF_HEADER
}

## ======================================================================

#Tarfile to use; grab only the latest
TARFILE=`ls -t greenplum-db-*.tar.gz 2>/dev/null | head -1`

if [ "${INSTALLER_TYPE}" = "appliance" ]; then
    INSTALLER_NAME=$( echo ${TARFILE} | sed -e 's|.tar.gz||' -e 's|greenplum-db|greenplum-db-appliance|' )
else
    INSTALLER_NAME=$( echo ${TARFILE} | sed 's|.tar.gz||' )
fi

VERSION=`echo ${INSTALLER_NAME} | sed -e "s|greenplum-db-appliance-||" \
                                      -e "s|greenplum-db-||" \
                                      -e "s|-build.*||" \
                                      -e "s|-${MPP_ARCH}||"`

echo "INSTALLER_NAME: ${INSTALLER_NAME}"

TARFILE_SIZE=`ls -al ${TARFILE} | awk '{print $5}'`
BOM="installer-bom.txt"

#Perform some checks
if [ -z "$TARFILE" ] ; then
    echo "Could not find GPDB tarfile."
    exit 1
fi

echo "Creating installer"
echo "Tar file: ${TARFILE}" 
echo "Size: ${TARFILE_SIZE}b"
echo""

#Get this from a checked in file eventually...
if [ "${INSTALLER_TYPE}" = "appliance" ]; then
    generate-applicance-installer-header > ${INSTALLER_NAME}.bin
else
    generate-pulse-installer-header > ${INSTALLER_NAME}.bin
fi

cat ${TARFILE} >> ${INSTALLER_NAME}.bin
chmod 755 ${INSTALLER_NAME}.bin

if [ "$?" -ne "0" ] ; then
    echo "Error creating installer."
    exit 1
fi

echo "${INSTALLER_NAME}.bin created."
