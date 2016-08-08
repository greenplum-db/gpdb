#!/bin/sh

SO_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SO_PATH=$SO_DIR/oom_malloc.so

case `uname` in
	Linux)
		export LD_PRELOAD=$SO_PATH
		;;
	Darwin)
		export DYLD_INSERT_LIBRARIES=$SO_PATH
		export DYLD_FORCE_FLAT_NAMESPACE=
		;;
esac

`$@`
