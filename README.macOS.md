# Readme

We've confirmed that these steps work on a brand new installation of macOS Sierra or a 
brand new installation of macOS Sierra with [Pivotal's workstation-setup](https://github.com/pivotal/workstation-setup)

## Step: Disable System Integrity Protection

Note that you may need to disable System Integrity Protection in order to bring
up the gpdemo cluster. Without doing this, psql commands run in child processes
spawned by gpinitsystem may have the DYLD_* environment variables removed from
their environments.

## Step: install needed dependencies. This will install homebrew if missing
```
./README.macOS.bash
source ~/.bash_profile
```

## Step: Workaround for libreadline / libxml2 on OSX 10.11 (El Capitan)

Symptoms:
* Running `./configure`,
* You see output
  `configure: error: readline library not found`, and
* in `config.log` you see
  `ld: file not found: /usr/lib/system/libsystem_symptoms.dylib for architecture x86_64`

There is an issue with Xcode 8.1 on El Capitan. Here's a workaround:

```
brew install libxml2
brew link libxml2 --force
```

Other workarounds may include downgrading to Xcode 7.3, or installing the CLT
package from Xcode 7.3.

For more info, [this seems to be the best thread](https://github.com/Homebrew/brew/issues/972)

## Step: Allow ssh to use the version of python in path, not the system python
#
```
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

sudo tee -a /etc/ssh/sshd_config << EOF
# Allow ssh to use the version of python in path, not the system python
PermitUserEnvironment yes
EOF

```

## Step: verify that you can ssh to your machine name without a password
```
ssh <hostname of your machine>  # e.g., ssh briarwood
```
### If the hostname does not resolve, try adding your machine name to /etc/hosts
```
echo -e "127.0.0.1\t$HOSTNAME" | sudo tee -a /etc/hosts
```

### If you see 'ssh: connect to host <> port 22: Connection refused', enable remote login
System Preferences -> Sharing -> Remote Login

### If you see a password prompt, authorize your SSH key
```
mkdir -p ~/.ssh
ssh-keygen -t rsa -b 4096 -C "your_email@example.com"
cat ~/.ssh/id_rsa.pub >>  ~/.ssh/authorized_keys
```

### Verify that you can ssh without a password and that ~/.ssh/environment exists
```
ssh <hostname of your machine> 
ls ~/.ssh/environment
```
