# Utilities written in golang

## Fetching dependencies

We are not using vendoring, nor a dependency management tool, currently.

Please see the README for a particular utility for more information, at
src/<utility_name>/README.md

## Development

Ensure you have Golang installed. You can do this easily on macOS with brew:

	$ brew install go

In order to have your $GOPATH and $PATH set up properly when you enter this
directory you should install `direnv`. On macOS you can install this by running
this command:

    $ brew install direnv

Then follow the instructions here: https://github.com/direnv/direnv#setup

Once you've set up `direnv`, assuming the .envrc file is in this directory and
that you have either opened a new session or sourced your .bashrc, when you
exit and enter the go-utils directory, you will see the following:

	direnv: loading .envrc
	direnv: export ~GOPATH ~PATH

The reason for installing and setting up direnv to set your $GOPATH upon
entering this directory is that we are using a non-standard $GOPATH: the
gpMgmt/go-utils sub-directory of gpdb source. In order to avoid conflicting
with the standard $GOPATH used as a convention in most Golang projects, we are
setting this custom $GOPATH only when you enter the gpMgmt/go-utils
sub-directory.

### Support for IDE development

If you are using an IDE, depending on the IDE, you may need to provide it with
the custom $GOPATH by adding it to your .bashrc or .bash_profile (not relying
on direnv).

	cat >> ~/.bashrc <<"EOF"
	export GOPATH=$HOME/workspace/gpdb/gpMgmt/go-utils:$HOME/go
	export PATH=$HOME/workspace/gpdb/gpMgmt/go-utils/bin:$HOME/go/bin:$PATH
	EOF
