package services

import (
	"context"
	"fmt"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	"gp_upgrade/helpers"
	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/upgradestatus"
	pb "gp_upgrade/idl"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"google.golang.org/grpc"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/reflection"
)

var DialTimeout = 3 * time.Second

type dialer func(ctx context.Context, target string, opts ...grpc.DialOption) (*grpc.ClientConn, error)

type reader interface {
	GetHostnames() ([]string, error)
	GetSegmentConfiguration() configutils.SegmentConfiguration
	OfOldClusterConfig(baseDir string)
	OfNewClusterConfig(baseDir string)
	GetMasterDataDir() string
	GetPortForSegment(segmentDbid int) int
}
type pairOperator interface {
	Init(string, string, string, helpers.CommandExecer) error
	StopEverything(string)
	GetPortsAndDataDirForReconfiguration() (int, int, string)
}

type HubClient struct {
	Bootstrapper
	conf *HubConfig

	agentConns    []*Connection
	clusterPair   pairOperator
	configreader  reader
	grpcDialer    dialer
	commandExecer helpers.CommandExecer

	mu      sync.Mutex
	server  *grpc.Server
	lis     net.Listener
	stopped chan struct{}
}

type Connection struct {
	Conn          *grpc.ClientConn
	Hostname      string
	CancelContext func()
}

type HubConfig struct {
	CliToHubPort   int
	HubToAgentPort int
	StateDir       string
	LogDir         string
}

func NewHub(pair pairOperator, configReader reader, grpcDialer dialer, execer helpers.CommandExecer, conf *HubConfig) *HubClient {
	// refactor opportunity -- don't use this pattern,
	// use different types or separate functions for old/new or set the config path at reader initialization time
	configReader.OfOldClusterConfig(conf.StateDir)

	h := &HubClient{
		stopped:       make(chan struct{}, 1),
		conf:          conf,
		clusterPair:   pair,
		configreader:  configReader,
		grpcDialer:    grpcDialer,
		commandExecer: execer,
		Bootstrapper: Bootstrapper{
			hostnameGetter: configReader,
			remoteExecutor: NewClusterSsher(
				upgradestatus.NewChecklistManager(conf.StateDir),
				NewPingerManager(conf.StateDir, 500*time.Millisecond),
				execer,
			),
		},
	}

	return h
}

func (h *HubClient) Start() {
	lis, err := net.Listen("tcp", ":"+strconv.Itoa(h.conf.CliToHubPort))
	if err != nil {
		gplog.Fatal(err, "failed to listen")
	}

	server := grpc.NewServer()
	h.mu.Lock()
	h.server = server
	h.lis = lis
	h.mu.Unlock()

	pb.RegisterCliToHubServer(server, h)
	reflection.Register(server)

	err = server.Serve(lis)
	if err != nil {
		gplog.Fatal(err, "failed to serve", err)
	}

	h.stopped <- struct{}{}
}

func (h *HubClient) Stop() {
	h.mu.Lock()
	defer h.mu.Unlock()

	if h.server != nil {
		h.closeConns()
		h.server.Stop()
		<-h.stopped
	}
}

func (h *HubClient) AgentConns() ([]*Connection, error) {
	if h.agentConns != nil {
		err := h.ensureConnsAreReady()
		if err != nil {
			gplog.Error("ensureConnsAreReady failed: ", err)
			return nil, err
		}

		return h.agentConns, nil
	}

	hostnames, err := h.configreader.GetHostnames()
	if err != nil {
		gplog.Error("GetHostnames failed: ", err)
		return nil, err
	}

	for _, host := range hostnames {
		ctx, cancelFunc := context.WithTimeout(context.Background(), DialTimeout)
		conn, err := h.grpcDialer(ctx, host+":"+strconv.Itoa(h.conf.HubToAgentPort), grpc.WithInsecure(), grpc.WithBlock())
		if err != nil {
			gplog.Error("grpcDialer failed: ", err)
			cancelFunc()
			return nil, err
		}
		h.agentConns = append(h.agentConns, &Connection{
			Conn:          conn,
			Hostname:      host,
			CancelContext: cancelFunc,
		})
	}

	return h.agentConns, nil
}

func (h *HubClient) ensureConnsAreReady() error {
	var hostnames []string
	for i := 0; i < 3; i++ {
		ready := 0
		for _, conn := range h.agentConns {
			if conn.Conn.GetState() == connectivity.Ready {
				ready++
			} else {
				hostnames = append(hostnames, conn.Hostname)
			}
		}

		if ready == len(h.agentConns) {
			return nil
		}

		time.Sleep(500 * time.Millisecond)
	}

	return fmt.Errorf("the connections to the following hosts were not ready: %s", strings.Join(hostnames, ","))
}

func (h *HubClient) closeConns() {
	for _, conn := range h.agentConns {
		defer conn.CancelContext()
		err := conn.Conn.Close()
		if err != nil {
			gplog.Info(fmt.Sprintf("Error closing hub to agent connection. host: %s, err: %s", conn.Hostname, err.Error()))
		}
	}
}

func (h *HubClient) segmentsByHost() map[string]configutils.SegmentConfiguration {
	segments := h.configreader.GetSegmentConfiguration()

	segmentsByHost := make(map[string]configutils.SegmentConfiguration)
	for _, segment := range segments {
		host := segment.Hostname
		if len(segmentsByHost[host]) == 0 {
			segmentsByHost[host] = []configutils.Segment{segment}
		} else {
			segmentsByHost[host] = append(segmentsByHost[host], segment)
		}
	}

	return segmentsByHost
}
