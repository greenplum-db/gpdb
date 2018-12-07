## gpssh developer notes

This directory contains code that overrides our vendored pexpect module.

In particular, we override the way the ssh command prompt is validated 
on a remote machine. The original, vendored pexpect tries to match 2 successive prompts 
from an interactive bash shell.  However, if the target host is slow, because of
CPU loading or network loading, those prompts may returned to the ssh client with some delay.

In the case of an SSH session with delay, the overridden method in this module retries several times,
extending the timeout from the default 0.1 second expectation between characters to up to 16x that.

Experimentally, the retries and extended timeout are enough to tolerate a 1 second delay between characters, at least on a localhost connection using the "tc" tool to delay network on that loopback.

The number of retries can be configured via a configuration setting. 
 
### Testing the retry logic

`tc` itself needs permissions beyond sudo, so it does not seem to work in an
unprivileged container (i.e., via Concourse).


On a Linux VM with root, use the `tc` library to delay network traffic as follows.

First, a delay of 500ms should exercise the retry logic, but still succeed:

```
sudo tc qdisc add dev lo root netem delay 500ms
```

To reconfigure the delay, first cancel the existing delay and then set a new one:

```
sudo tc qdisc del dev lo root
sudo tc qdisc add dev lo root netem delay 1200ms
```



