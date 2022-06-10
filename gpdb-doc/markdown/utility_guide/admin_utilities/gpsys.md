# gpsys1 

Displays information about your operating system.

## Synopsis 

```
gpsys1 [ -a | -m | -p ]

gpsys1 -? 

gpsys1 --version
```

## Description 

`gpsys1` displays the platform and installed memory \(in bytes\) of the current host. For example:

```
linux 1073741824
```

## Options 

-a \(show all\)
:   Shows both platform and memory information for the current host. This is the default.

-m \(show memory only\)
:   Shows system memory installed in bytes.

-p \(show platform only\)
:   Shows the OS platform. Platform can be `linux`, `darwin` or `sunos5`.

-? \(help\)
:   Displays the online help.

--version
:   Displays the version of this utility.

## Examples 

Show information about the current host operating system:

```
gpsys1
```

## See Also 

[gpcheckperf](gpcheckperf.html)

