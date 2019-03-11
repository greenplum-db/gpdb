#!/bin/bash
set -e

setup_ssh_config() {
  grep 'Host github.com' ~/.ssh/config && return
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
  [[ $GIT_ORIGIN_PATH =~ ^(https?|git):// ]] || setup_ssh_config
  git clone "$GIT_ORIGIN_PATH" "$GPDB_DIR"
  pushd "$GPDB_DIR"
  git config user.name "$CURRENT_GIT_USER_NAME"
  git config user.email "$CURRENT_GIT_USER_EMAIL"
  # add all remotes found in git repo
  for remote_path in "${!GIT_REMOTE_PATH_@}"; do
    remote_name=${remote_path/PATH/NAME}
    eval git remote add "\$$remote_name" "\$$remote_path"
  done
  # 'git checkout' same remote and branch as user
  CURRENT_GIT_REMOTE_PATH=$(git remote get-url "$CURRENT_GIT_REMOTE")
  [[ $CURRENT_GIT_REMOTE_PATH =~ ^(https?|git):// ]] || setup_ssh_config
  if git fetch "$CURRENT_GIT_REMOTE" "$CURRENT_GIT_BRANCH"; then
    git checkout "$CURRENT_GIT_BRANCH"
  else
    echo "${BASH_SOURCE[0]}: couldn't checkout user's current remote/branch." >&2
    echo "${BASH_SOURCE[0]}: staying on origin/master" >&2
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

# source greenplum_path in ~/.bashrc
GPDEMO_ENV=${GPDB_DIR}/gpAux/gpdemo/gpdemo-env.sh
cat <<EOF >>~/.bashrc
source /usr/local/gpdb/greenplum_path.sh
if [[ -r $GPDEMO_ENV ]]; then
  source "$GPDEMO_ENV"
fi
EOF

pushd "${GPDB_DIR}/gpAux/gpdemo"
source ~/.bashrc
make
popd
