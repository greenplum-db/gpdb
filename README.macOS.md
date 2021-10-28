We've confirmed that these steps work on a brand new installation of macOS
Sierra.

## 1. Install needed dependencies

Execute the following, this script will start by caching the `sudo` password:

```bash
./README.macOS.bash
source ~/.bash_profile
```

Note: This will install Homebrew if missing

## 2. Allow ssh to use the version of python in path, not the system python

Execute the following in your terminal:

```bash
mkdir -p "$HOME/.ssh"

cat >> ~/.bash_profile << EOF

# Allow ssh to use the version of python in path, not the system python
# BEGIN SSH agent
# from http://stackoverflow.com/questions/18880024/start-ssh-agent-on-login/18915067#18915067
SSH_ENV="\$HOME/.ssh/environment"
# Refresh the PATH per new session
sed -i .bak '/^PATH/d' \${SSH_ENV}
echo "PATH=\$PATH" >> \${SSH_ENV}

function start_agent {
    echo "Initialising new SSH agent..."
    /usr/bin/ssh-agent | sed 's/^echo/#echo/' > "\${SSH_ENV}"
    echo succeeded
    chmod 600 "\${SSH_ENV}"
    source "\${SSH_ENV}" > /dev/null
    /usr/bin/ssh-add;
}

# Source SSH settings, if applicable
if [ -f "\${SSH_ENV}" ]; then
    . "\${SSH_ENV}" > /dev/null
    ps -ef | grep \${SSH_AGENT_PID} 2>/dev/null | grep ssh-agent$ > /dev/null || {
        start_agent;
    }
else
    start_agent;
fi

[ -f ~/.bashrc ] && source ~/.bashrc
# END SSH agent
EOF
```

Then bring `.bash_profile` into effect and allow ssh to use the version of python in path, not the system python.

```bash
source ~/.bash_profile

sudo tee -a /etc/ssh/sshd_config << EOF
# Allow ssh to use the version of python in path, not the system python
PermitUserEnvironment yes
EOF
```

## 3. Enable ssh to localhost without requiring a password

Currently, the GPDB utilities require that it be possible to `ssh` to localhost
without using a password.  To verify this, the following command should succeed
without erroring, or requiring you to enter a password. 

```bash
ssh <hostname of your machine>  # e.g., ssh briarwood (You can use `hostname` to get the hostname of your machine.)
```

However, if it is the first time that you are `ssh`'ing to localhost, you may
need to accept the trust on first use prompt.

```bash
The authenticity of host '<your hostname>' can't be established.
ECDSA key fingerprint is SHA256:<fingerprint here>.
Are you sure you want to continue connecting (yes/no)?
```

If the hostname does not resolve, try adding your machine name to `/etc/hosts`,
like so:

```bash
echo -e "127.0.0.1\t$HOSTNAME" | sudo tee -a /etc/hosts
```

If you see `ssh: connect to host <> port 22: Connection refused`, enable remote
login in:

```bash
System Preferences -> Sharing -> Remote Login
```

If you see a password prompt, add your SSH key to the `authorized_keys` file,
like so:

```bash
mkdir -p ~/.ssh
ssh-keygen -t rsa -b 4096 -C "your_email@example.com"
cat ~/.ssh/id_rsa.pub >>  ~/.ssh/authorized_keys
```

After troubleshooting,  verify one more time that you can ssh without a
password and that `~/.ssh/environment` exists, by running:

```bash
ssh <hostname of your machine> 
ls ~/.ssh/environment
```

If the ssh succeeded with out asking for a password, and that file exists, you
are good to go.  Remember to exit all of your ssh session before moving on.

## 4. Configure, compile, and install

At this point head back the main [README.md](./README.md#build-the-database),
and continue from __Build the Database__.

## Troubleshooting macOS specific issues: 

### Workaround for libreadline / libxml2 on OSX 10.11 (El Capitan)

Symptoms:
* After Running `./configure`,
* You see output
  `configure: error: readline library not found`, and
* in `config.log` you see
  `ld: file not found: /usr/lib/system/libsystem_symptoms.dylib for architecture x86_64`

There is an issue with Xcode 8.1 on El Capitan. Here's a workaround:

```bash
brew install libxml2
brew link libxml2 --force
```

Other workarounds may include downgrading to Xcode 7.3, or installing the CLT
package from Xcode 7.3.

For more info, [this seems to be the best thread](https://github.com/Homebrew/brew/issues/972)

### Disable System Integrity Protection

Note that you may need to disable System Integrity Protection in order to bring
up the gpdemo cluster. Without doing this, psql commands run in child processes
spawned by gpinitsystem may have the DYLD_* environment variables removed from
their environments.
