#!/bin/bash
set -uo pipefail

main() {
	echo "quitting Checkman"
	pgrep Checkman | xargs kill -9
	rm -rf ~/.trash/Checkman.app
	echo "removing Checkman.app"
	mv /Applications/Checkman.app ~/.trash 2> /dev/null
	echo "archiving all existing checklists into ~/.CheckmanArchive/"
	mkdir -p ~/.CheckmanArchive
	shopt -s dotglob
	cp -f ~/Checkman/* ~/.CheckmanArchive/
	rm -rf ~/Checkman
	echo "unsetting defaults"
	defaults delete com.tomato.Checkman 2> /dev/null
}

main
