#!/bin/sh

command_exists() {
  command -v "$@" > /dev/null 2>&1
}

# perform some very rudimentary platform detection
lsb_dist=''
dist_version=''
if command_exists lsb_release; then
  lsb_dist="$(lsb_release -si)"
fi
if [ -z "$lsb_dist" ] && [ -r /etc/lsb-release ]; then
  lsb_dist="$(. /etc/lsb-release && echo "$DISTRIB_ID")"
fi
if [ -z "$lsb_dist" ] && [ -r /etc/debian_version ]; then
  lsb_dist='debian'
fi
if [ -z "$lsb_dist" ] && [ -r /etc/fedora-release ]; then
  lsb_dist='fedora'
fi
if [ -z "$lsb_dist" ] && [ -r /etc/oracle-release ]; then
  lsb_dist='oracleserver'
fi
if [ -z "$lsb_dist" ]; then
  if [ -r /etc/centos-release ] || [ -r /etc/redhat-release ]; then
    lsb_dist='centos'
  fi
fi
if [ -z "$lsb_dist" ] && [ -r /etc/os-release ]; then
  lsb_dist="$(. /etc/os-release && echo "$ID")"
fi

lsb_dist="$(echo "$lsb_dist" | tr '[:upper:]' '[:lower:]')"

case "$lsb_dist" in

  ubuntu)
    if command_exists lsb_release; then
      dist_version="$(lsb_release --codename | cut -f2)"
    fi
    if [ -z "$dist_version" ] && [ -r /etc/lsb-release ]; then
      dist_version="$(. /etc/lsb-release && echo "$DISTRIB_CODENAME")"
    fi
  ;;

  debian)
    dist_version="$(cat /etc/debian_version | sed 's/\/.*//' | sed 's/\..*//')"
    case "$dist_version" in
      8)
        dist_version="jessie"
      ;;
      7)
        dist_version="wheezy"
      ;;
    esac
  ;;

  oracleserver)
    # need to switch lsb_dist to match yum repo URL
    lsb_dist="oraclelinux"
    dist_version="$(rpm -q --whatprovides redhat-release --queryformat "%{VERSION}\n" | sed 's/\/.*//' | sed 's/\..*//' | sed 's/Server*//')"
  ;;

  fedora|centos)
    dist_version="$(rpm -q --whatprovides redhat-release --queryformat "%{VERSION}\n" | sed 's/\/.*//' | sed 's/\..*//' | sed 's/Server*//')"
  ;;

  *)
    if command_exists lsb_release; then
      dist_version="$(lsb_release --codename | cut -f2)"
    fi
    if [ -z "$dist_version" ] && [ -r /etc/os-release ]; then
      dist_version="$(. /etc/os-release && echo "$VERSION_ID")"
    fi
  ;;
esac

type=
case "$lsb_dist" in
  ubuntu|debian)
  type=deb
  ;;
  fedora|centos|oraclelinux)
  type=rpm
  ;;
esac

pkgs=
for i in \
  /usr/gpdb/bin \
  /usr/gpdb/demo \
  /usr/gpdb/docs \
  /usr/gpdb/greenplum_path.sh \
  /usr/gpdb/include \
  /usr/gpdb/lib \
  /usr/gpdb/sbin \
  /usr/gpdb/share
do
  pkgs="$pkgs $(echo $i | sed -e 's@^/@@')"
done

if [ "$1" = "build" ] ; then
./configure --with-python --prefix /usr/gpdb
make -j2
make -j2 install
fpm -s dir \
  -t $type \
  -n gpdb \
  -v $(cat VERSION | tr ' ' '-') \
  -p gpdb_VERSION_ARCH.$type \
  -C / \
  $pkgs
exit
elif [ "$1" = "clean" ] ; then
make clean
exit
fi
exec $@
