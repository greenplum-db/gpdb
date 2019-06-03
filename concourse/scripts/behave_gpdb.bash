#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function install_python_hacks() {
    # Our Python installation doesn't run standalone; it requires
    # LD_LIBRARY_PATH which causes virtualenv to fail (because the system and
    # vendored libpythons collide). We'll try our best to install patchelf to
    # fix this later, but it's not available on all platforms.
    if which yum > /dev/null; then
        yum install -y patchelf
    else
        set +x
        echo "ERROR: install_python_hacks() doesn't support the current platform and should be modified"
        exit 1
    fi
}

function gen_env(){
    cat > /opt/run_test.sh <<-EOF
		set -ex

		# virtualenv 16.0 and greater does not support python2.6, which is
		# used on centos6
		pip install --user virtualenv~=15.0
		export PATH=\$PATH:~/.local/bin

		# create virtualenv before sourcing greenplum_path since greenplum_path
		# modifies PYTHONHOME and PYTHONPATH
		#
		# XXX Patch up the vendored Python's RPATH so we can successfully run
		# virtualenv. If we instead set LD_LIBRARY_PATH (as greenplum_path.sh
		# does), the system Python and the vendored Python will collide and
		# virtualenv will fail. This step requires patchelf.
		if which patchelf > /dev/null; then
			patchelf \
				--set-rpath /usr/local/greenplum-db-devel/ext/python/lib \
				/usr/local/greenplum-db-devel/ext/python/bin/python

			virtualenv \
				--python /usr/local/greenplum-db-devel/ext/python/bin/python /tmp/venv
		else
			# We don't have patchelf on this environment. The only workaround we
			# currently have is to set both PYTHONHOME and LD_LIBRARY_PATH and
			# pray that the resulting libpython collision doesn't break
			# something too badly.
			echo 'WARNING: about to execute a cross-linked virtualenv; here there be dragons'
			LD_LIBRARY_PATH=/usr/local/greenplum-db-devel/ext/python/lib \
			PYTHONHOME=/usr/local/greenplum-db-devel/ext/python \
			virtualenv \
				--python /usr/local/greenplum-db-devel/ext/python/bin/python /tmp/venv
		fi

		# activate virtualenv after sourcing greenplum_path, so that virtualenv
		# takes precedence
		source /usr/local/greenplum-db-devel/greenplum_path.sh
		source /tmp/venv/bin/activate

		cd "\${1}/gpdb_src/gpAux"
		source gpdemo/gpdemo-env.sh

		cd "\${1}/gpdb_src/gpMgmt/"
		pip install -r requirements-dev.txt

		BEHAVE_TAGS="${BEHAVE_TAGS}"
		BEHAVE_FLAGS="${BEHAVE_FLAGS}"
		if [ ! -z "\${BEHAVE_TAGS}" ]; then
		    make -f Makefile.behave behave tags=\${BEHAVE_TAGS}
		else
		    flags="\${BEHAVE_FLAGS}" make -f Makefile.behave behave
		fi
	EOF

    chmod a+x /opt/run_test.sh
}

function _main() {

    if [ -z "${BEHAVE_TAGS}" ] && [ -z "${BEHAVE_FLAGS}" ]; then
        echo "FATAL: BEHAVE_TAGS or BEHAVE_FLAGS not set"
        exit 1
    fi

    time install_gpdb
    time ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash

    # Run inside a subshell so it does not pollute the environment after
    # sourcing greenplum_path
    time (make_cluster)

    time install_python_hacks
    time gen_env

    time run_test
}

_main "$@"
