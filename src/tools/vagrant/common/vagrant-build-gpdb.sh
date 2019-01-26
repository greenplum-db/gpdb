#!/bin/bash
set -ex

setup_ssh_config() {
  mkdir -p ~/.ssh
  cat <<EOF>>~/.ssh/config
Host github.com
  HostName github.com
  User git
  StrictHostKeyChecking No
EOF
  chmod -R 600 ~/.ssh/config
}

GPDB_DIR=~/gpdb
if [[ ! -d $GPDB_DIR ]]; then
  pushd ~
  git clone https://github.com/greenplum-db/gpdb
  pushd $GPDB_DIR
  git config user.name "$CURRENT_GIT_USER_NAME"
  git config user.email "$CURRENT_GIT_USER_EMAIL"
  # add all remotes found in git repo
  for path in "${!GIT_REMOTE_PATH_@}"; do
    eval git remote add "${path#GIT_REMOTE_PATH_}" "\$$path"
  done
  # 'git checkout' same remote and branch as user
  if [[ $CURRENT_GIT_REMOTE != origin ]]; then
    [[ $CURRENT_GIT_REMOTE_PATH =~ ^https?:// ]] || setup_ssh_config
    git fetch "$CURRENT_GIT_REMOTE" "$CURRENT_GIT_BRANCH"
    git checkout "$CURRENT_GIT_BRANCH"
  fi
  popd
  popd
fi

export CC='ccache cc'
export CXX='ccache c++'
export PATH=/usr/local/bin:$PATH

rm -rf /usr/local/gpdb
pushd $GPDB_DIR
./configure --prefix=/usr/local/gpdb "$@"
make clean
make -j4 && make install
popd

# make sure ssh is not stuck asking if the host is known
rm -f ~/.ssh/id_rsa
rm -f ~/.ssh/id_rsa.pub
ssh-keygen -t rsa -N '' -f "${HOME}/.ssh/id_rsa"
cat "${HOME}/.ssh/id_rsa.pub" >> "${HOME}/.ssh/authorized_keys"
{ ssh-keyscan -H localhost
  ssh-keyscan -H 127.0.0.1
  ssh-keyscan -H "$HOSTNAME"
} >> "${HOME}/.ssh/known_hosts"
chmod 600 ~/.ssh/authorized_keys

# BUG: fix the LD_LIBRARY_PATH to find installed GPOPT libraries
echo export LD_LIBRARY_PATH=/usr/local/lib:\$LD_LIBRARY_PATH \
  >>/usr/local/gpdb/greenplum_path.sh

pushd "${GPDB_DIR}/gpAux/gpdemo"
source /usr/local/gpdb/greenplum_path.sh
make
popd

#cd /vagrant/src/pl/plspython/tests
#make containers
#sudo -u postgres make
