# Utilities written in golang

## Fetching dependencies

We are not using vendoring, nor a dependency management tool, currently.

Fetch dependencies for a utility with:

```
cd src/<utility_name>
go get
```

## Development

In order to have your $GOPATH and $PATH set up properly when you enter this
directory you should install `direnv`. On macOS you can install this by running
this command:

    $ brew install direnv

Then follow the instructions here: https://github.com/direnv/direnv#setup
