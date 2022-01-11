# Developer's guide for GPDB

### Credits
This guide was developed in collaboration with Navneet Potti (@navsan) and
Nabarun Nag (@nabarunnag). Many thanks to Dave Cramer (@davecramer) and
Daniel Gustafsson (@danielgustafsson) for various suggestions to improve
the original version of this document. Alexey Grishchenko (@0x0FFF) has
also participated in improvement of the document and scripts.

## Who should read this document?
Anyone who wants to develop code for GPDB. This guide targets the
freelance developer who typically has a laptop and wants to develop
GPDB code on it. In other words, such a typical developer does not necessarily
have 24x7 access to a cluster, and needs a miminal stand-alone development
environment.

The instructions here were verified on the configurations below.

| **OS**        | **Date Tested**    | **Comments**                           |
| :------------ |:-------------------| --------------------------------------:|
| OSX v.10.10.5 | 2016-03-17         | Vagrant v. 1.8.1; VirtualBox v. 5.0.16 |
| OSX v.10.11.2 | 2015-12-29         | Vagrant v. 1.8.1; VirtualBox v. 5.0.12 |

## 1: Setup VirtualBox and Vagrant
You need to setup both VirtualBox and Vagrant. If you don't have these
installed already, then head over to https://www.virtualbox.org/wiki/Downloads
and http://www.vagrantup.com/downloads to download and then install them.

## 2: Clone GPDB code from github
Go to the directory in your machine where you want to check out the GPDB code,
and clone the GPDB code by typing the following into a terminal window.

```shell
git clone https://github.com/greenplum-db/gpdb.git
```

## 3: Setup and start the virtual machine
Next go to the `gpdb/src/tools/vagrant` directory. This directory has virtual machine
configurations for different operating systems (for now there is only one).
Pick the distro of your choice, and `cd` to that directory. For this document,
we will assume that you pick `centos`. So, issue the following command:

```shell
cd gpdb/src/tools/vagrant/centos
```

Next let us start a virtual machine using the Vagrant file in that directory.
From the terminal window, issue the following command:

```shell
vagrant up gpdb
```

The last command will take a while as Vagrant works with VirtualBox to fetch
a box image for CentOS. This image is fetched only once and will be stored by Vagrant in a
directory (likely `~/.vagrant.d/boxes/`), so you won't repeatedly incur this network IO
if you repeat the steps above. A side-effect is that Vagrant has now used a
few hundred MiBs of space on your machine. You can see the list of boxes that
Vagrant has downloaded using ``vagrant box list``. If you need to drop some
box images, follow the instructions posted [here](https://docs.vagrantup.com/v2/cli/box.html "vagrant manage boxes").

If you are curious about what Vagrant is doing, then open the file
`Vagrantfile`. The `config.vm.box` parameter there specifies the
Vagrant box image that is being fetched. Essentially you are creating an
image of CentOS on your machine that will be used below to setup and run GPDB.

While you are viewing the Vagrantfile, a few more things to notice here are:
* The parameter `vb.memory` sets the memory to 8GB for the virtual machine.
  You could dial that number up or down depending on the actual memory in
  your machine.
* The parameter `vb.cpus` sets the number of cores that the virtual machine will
  use to 4. Again, feel free to change this number based on the machine that
  you have.
* Additional synced folders can be configured by adding a `vagrant-local.yml`
  configuration file on the following format:

```yaml
synced_folder:
    - local: /local/folder
      shared: /folder/in/vagrant
    - local: /another/local/folder
      shared: /another/folder/in/vagrant
```

Once the command above (`vagrant up gpdb`) returns, we are ready to login to the
virtual machine. Type in the following command into the terminal window
(make sure that you are in the directory `gpdb/vagrant/centos`):

```shell
vagrant ssh gpdb
```

Now you are in the virtual machine shell in a **guest** OS that is running in
your actual machine (the **host**). Everything that you do in the guest machine
will be isolated from the host.

That's it - GPDB is built, up and running.  Before you can open a psql connection, run the following:
```shell
# setup the environment
source /usr/local/gpdb/greenplum_path.sh
source ~/gpdb/gpAux/gpdemo/gpdemo-env.sh

# create a database to interact with (you only need to do this once)
createdb

# connect!
psql
```

To run the tests:
```shell
cd ~/gpdb
make installcheck-world
```

If you are curious how this happened, take a look at the following scripts:
* `vagrant/centos/vagrant-setup.sh` - this script installs all the packages
  required for GPDB as dependencies
* `vagrant/centos/vagrant-build.sh` - this script builds GPDB. In case you
  need to change build options you can change this file and re-create VM by
  running `vagrant destroy gpdb` followed by `vagrant up gpdb`
* `vagrant/centos/vagrant-configure-os.sh` - this script configures OS
  parameters required for running GPDB

You can easily go to `vagrant/centos/Vagrantfile` and comment out the calls for
any of these scripts at any time to prevent GPDB installation or OS-level
configurations

If you want to try out a few SQL commands, go back to the guest shell in which
you have the `psql` prompt, and issue the following SQL commands:

```sql
-- Create and populate a Users table
CREATE TABLE Users (uid INTEGER PRIMARY KEY,
                    name VARCHAR);
INSERT INTO Users
  SELECT generate_series, md5(random())
  FROM generate_series(1, 100000);

-- Create and populate a Messages table
CREATE TABLE Messages (mid INTEGER PRIMARY KEY,
                       uid INTEGER REFERENCES Users(uid),
                       ptime DATE,
                       message VARCHAR);
INSERT INTO Messages
   SELECT generate_series,
          round(random()*100000),
          date(now() - '1 hour'::INTERVAL * round(random()*24*30)),
          md5(random())::text
   FROM generate_series(1, 1000000);

-- Report the number of tuples in each table
SELECT COUNT(*) FROM Messages;
SELECT COUNT(*) FROM Users;

-- Report how many messages were posted on each day
SELECT M.ptime, COUNT(*)
FROM Users U NATURAL JOIN Messages M
GROUP BY M.ptime
ORDER BY M.ptime;
```

You just created a simple warehouse database that simulates users posting
messages on a social media network. The "fact" table (i.e. the `Messages`
table) has a million rows. The final query reports the number of messages
that were posted on each day. Pretty cool!

(Note if you want to exit the `psql` shell above, type in `\q`.)

## 4: Using GPDB
If you are doing serious development, you will likely need to use a debugger.
Here is how you do that.

First, list the Postgres processes by typing in (a guest terminal) the following
command: `ps ax | grep postgres`. You should see a list that looks something
like: ![Postgres processes](/vagrant/pictures/gpdb_processes.png)

(You may have to click on the image to see it at a higher resolution.)
Here the key processes are the ones that were started as
`/usr/local/gpdb/bin/postgres`. The coordinator is the process (pid 25486
in the picture above) that has the word "coordinator" in the `-D`parameter setting,
whereas the segment hosts have the word "gpseg" in the `-D` parameter setting.

Next, start ``gdb`` from a guest terminal. Once you get a prompt in gdb, type
in the following (the pid you specify in the `attach` command will be
different for you):
```gdb
set follow-fork-mode child
b ExecutorMain
attach 25486
```
Of course, you can change which function you want to break into, and change
whether you want to debug the coordinator or the segment processes. Happy hacking!

## 5: GPDB without GPORCA
If you want to run GPDB without the GPORCA query optimizer, run `vagrant up gpdb-without-gporca`.
