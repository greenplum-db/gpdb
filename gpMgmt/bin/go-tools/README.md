## Setup environment

### Build and Install
The basic steps required are to
- Install dependencies and set env (one time)
- Generate SSL certificates (optional, one time)
- Compile the code
- Install GP utility

#### Checkout the code (if not already done)
```
mkdir ~/workspace & cd ~/workspace
git clone git@github.com:greenplum-db/gpdb.git
```

#### Get dependency and set env
```
cd ~/workspace/gpdb/gpMgmt/bin/go-tools
make depend-dev    # set GOBIN path and fetch the protoc and mock dependency
```

#### Generate Certificates
This step generates self-signed certificates. Skip this step if you already have
self-signed or CA issued certificates
```
make cert    # generate certificates for given host
```

#### Install GP utility
```
make install
```

## Developers options
Advanced options are required for development purposes.

#### Generate RPC bindings and protobuf
```
make proto   # compile protobuf files to generate grpc code for hub and agents
```

#### Cross-compile for other platforms
```
make build_linux   # build gp binary for Linux platform
make build_mac     # build gp binary for Mac platform
```
## Running Tests

#### Unit tests
```
make test     # run unit tests in verbose mode
```

#### Check test coverage
```
make test-coverage
```

#### Run Linter
```
make lint       # run linter on the code
```

#### Acceptance tests
Creates a concourse pipeline on dev instance against a branch (default: current branch).
The pipeline runs various multi-host unit/functional tests.
```
make pipeline
```
Examples:
```
# Runs against specific branch
make pipeline GIT_REMOTE=<Custom Remote> GIT_BRANCH=<Custom Branch>

# Creates pipeline with given name
make pipeline PIPELINE_NAME=<Custom-Pipeline-Name> 
```

## Running gp utility
Following are the basic steps to run gp utility:
- Configure gp services
- Controlling Agent and Hub services
- Monitoring service status

#### Configure gp services:
This is one-time activity required to generate the required configuration
for the hub and agents. Also, this command copies generated config file to all
the hosts using gpsync followed by service registration.

```
gp configure       # to generate config file with given conf setting
gp configure --help  # to view the config options

example:
gp configure --host <host> --server-certificate <path/to/server-cert.pem> --server-key < path/to/server-key.pem> --ca-certificate <path/to/ca-cert.pem> --ca-key <path/to/ca-key.pem>
```

#### Control and monitoring services:
Agent and Hub Services can be controlled and monitored using the following command:
```
gp [start/stop/status] [agents/hub/services]
```
e.g.
- `gp start hub` starts the hub service
- `gp start services` starts both hub and agent services
- `gp stop services` stops both hub and agent services

##### Monitoring Service Status:
To check the status of the services you can use the following command:
- `gp status agents` reports status of all agents service
- `gp status hub` reports the status of the hub service
- `gp status services` reports the status of the hub and agent services

#### Log Locations
Logs are located in the path provided in the configuration file.
By default, it will be generated in `/tmp` directory.
Logs file gets created on the local machine when the service is running. 
