#!/bin/bash
set -uxo pipefail

CHECKMAN_INTERVAL=30 # Seconds between running checks

check_prereqs() {
  local ruby_version
  ruby_version=$(ruby -v | cut -d ' ' -f2)
  local major_version minor_version
  major_version=${ruby_version:0:1}
  minor_version=${ruby_version:2:1}
  if [ "${major_version}" -lt 2 ] && [ "${minor_version}" -lt 9 ]; then
    echo "Checkman does not support your version of Ruby (${ruby_version}). The minimum supported Ruby version is 1.9."
    exit 1
  fi
}

install_checkman() {
  curl https://raw.githubusercontent.com/cppforlife/checkman/master/bin/install > /tmp/checkman_install.sh
  bash  /tmp/checkman_install.sh
  if [ $? != 0 ]; then
    echo "Error installing checkman"
    exit 1
  fi
  rm -f ~/Checkman/example
}

configure_checkman_defaults() {
  local current_interval
  current_interval=$(defaults read com.tomato.Checkman checkRunInterval 2> /dev/null || echo -1)
  if [ "${current_interval}" -lt 30 ]; then
    defaults write com.tomato.Checkman checkRunInterval -int $CHECKMAN_INTERVAL
    echo "No previous default for check interval was detected; setting interval to 30 seconds."
    echo "To modify this interval, run 'defaults write com.tomato.Checkman checkRunInterval -int <interval in seconds>' in bash."
  fi
  local stickies_setting
  stickies_setting=$(defaults read com.tomato.Checkman stickies.disabled &> /dev/null || echo "no previous stickies default")
  if [ "${stickies_setting}" = "no previous stickies default" ]; then
    defaults write com.tomato.Checkman stickies.disabled -bool YES
    echo "No previous default for stickies was detected; disabling stickies."
    echo "To enable them, run 'defaults write com.tomato.Checkman stickies.disabled -bool NO' in bash."
  fi
}

link_checkfiles() {
  mkdir -p ~/Checkman/
  shopt -s dotglob
  local checkfile_dir
  checkfile_dir="$(dirname $0)/checklists"
  for filepath in $checkfile_dir/*; do
    [[ -e $filepath ]] || break
    local filename
    filename=$(basename "$filepath")
    if [ -f "$HOME/Checkman/.$filename" ] || [ -h "$HOME/Checkman/.$filename" ] ; then
      if [ -f "$HOME/Checkman/$filename" ] || [ -h "$HOME/Checkman/$filename" ] ; then
        echo "ALERT: you have similar files in ~/Checkman named $filename and .$filename: one is hidden. Links neither created nor updated for these"
      else
        ln -nf "$filepath" "$HOME/Checkman/.$filename"
        echo "Checkfile $filename already linked and hidden. Keeping hidden"
      fi
    else
      local shasum1
      shasum1=$(shasum "$filepath" | cut -f 1 -d ' ')
      local shasum2
      shasum2=$(shasum "$HOME/Checkman/$filename" | cut -f 1 -d ' ')
      if [ "${shasum1}" = "${shasum2}" ]; then
        echo "Checkfile $filename is the same as $HOME/Checkman/$filename, skipping re-link"
      else
        ln -nf "$filepath" "$HOME/Checkman/$filename"
        echo "Checkfile $filename linked from CI Infrastructure to Checkman/"
      fi
    fi
  done
}

_main() {
  check_prereqs
  configure_checkman_defaults
  link_checkfiles
  install_checkman
}

_main
