package configutils

import (
	"fmt"
	pb "gp_upgrade/idl"

	"google.golang.org/grpc"
)

const (
	// todo generalize to any host
	// todo de-duplicate the use of this port in monitor.go
	port = "6416"
)

type ClientAndHostname struct {
	Client   pb.CommandListenerClient
	Hostname string
}

type RPCClients struct{}

func (helper RPCClients) GetRPCClients() []ClientAndHostname {
	reader := Reader{}
	hostnames, _ := reader.GetHostnames()
	var clients []ClientAndHostname
	for i := 0; i < len(hostnames); i++ {
		conn, err := grpc.Dial(hostnames[i]+":"+port, grpc.WithInsecure())
		if err == nil {
			clientAndHost := ClientAndHostname{
				Client:   pb.NewCommandListenerClient(conn),
				Hostname: hostnames[i],
			}
			clients = append(clients, clientAndHost)
		} else {
			fmt.Println("ERROR: couldn't get gRPC conn to " + hostnames[i])
		}
	}
	return clients
}
