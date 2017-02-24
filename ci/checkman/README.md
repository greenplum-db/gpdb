# Checkman quick installation

[Checkman](https://github.com/cppforlife/checkman) is a macOS application that
provides both a tray applet to display the number of builds and their status
with quicklinks to view those builds in addition to a local web interface
available at
[http://localhost:1234/index.html](http://localhost:1234/index.html)

## Dependencies

Checkman requires Ruby 1.9 or higher, and only runs on macOS

## First time install

Run `install.bash`

A new application will be installed in /Applications called Checkman.app.

## Subsequent installs or updates

Run:

`~/workspace/ci-infrastructure/checkman/install.bash`

This will do the following:

1. Install Checkman.app in /Applications ([using Checkman's own installer](https://github.com/cppforlife/checkman#installation)) if there's a new version
1. Link in default checkfiles for GPDB
1. Launch Checkman.app
1. Set the default check interval to 30 seconds if not greater.

If you want to show only some of the checkfiles:

1. `cd ~/Checkman`
2. hide any files you don't want to show by adding a '.' to the beginning of the filename. This will not perturb this repo's operations and you will get updates

## FAQ & Troubleshooting

**How do I change the default frequency of the checks?**

`defaults write com.tomato.Checkman checkRunInterval -int <INTERVAL_IN_SECONDS>`

**I get a permission problem installing /Applications/Checkman.app**

Most likely this means that you are running `install.bash` as a user who only has standard access and is not an admin user on macOS. See [this answer on StackOverflow](http://superuser.com/questions/411577/do-all-mac-os-x-applications-require-admin-permissions-to-install) for more information on this topic.
