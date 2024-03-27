package hub

import (
	"context"
	"errors"
	"fmt"
	"sync"

	"golang.org/x/exp/maps"

	"github.com/greenplum-db/gp-common-go-libs/dbconn"
	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/greenplum"
	"github.com/greenplum-db/gpdb/gp/utils/postgres"
)

var execOnDatabaseFunc = ExecOnDatabase

func (s *Server) MakeCluster(request *idl.MakeClusterRequest, stream idl.Hub_MakeClusterServer) error {
	var err error
	var shutdownCoordinator bool

	hubStream := NewHubStream(stream)

	// shutdown the coordinator segment if any error occurs
	defer func() {
		if err != nil && shutdownCoordinator {
			hubStream.StreamLogMsg("Not able to create the the cluster, proceeding to shutdown the coordinator segment")
			err := s.StopCoordinator(&hubStream, request.GpArray.Coordinator.DataDirectory)
			if err != nil {
				gplog.Error(err.Error())
			}
		}
	}()

	err = s.DialAllAgents()
	if err != nil {
		return utils.LogAndReturnError(err)
	}

	hubStream.StreamLogMsg("Starting to create the cluster")
	err = s.ValidateEnvironment(&hubStream, request)
	if err != nil {
		return utils.LogAndReturnError(fmt.Errorf("validating hosts: %w", err))
	}

	hubStream.StreamLogMsg("Creating coordinator segment")
	err = s.CreateAndStartCoordinator(request.GpArray.Coordinator, request.ClusterParams)
	if err != nil {
		return utils.LogAndReturnError(err)
	}
	hubStream.StreamLogMsg("Successfully created coordinator segment")

	shutdownCoordinator = true

	hubStream.StreamLogMsg("Starting to register primary segments with the coordinator")

	conn, err := greenplum.ConnectDatabase(request.GpArray.Coordinator.HostName, int(request.GpArray.Coordinator.Port))
	if err != nil {
		return utils.LogAndReturnError(fmt.Errorf("connecting to database: %w", err))
	}

	err = conn.Connect(1, true)
	if err != nil {
		return utils.LogAndReturnError(err)
	}

	err = greenplum.RegisterCoordinator(request.GpArray.Coordinator, conn)
	if err != nil {
		return utils.LogAndReturnError(err)
	}

	err = greenplum.RegisterPrimaries(request.GetPrimarySegments(), conn)
	if err != nil {
		return utils.LogAndReturnError(err)
	}
	hubStream.StreamLogMsg("Successfully registered primary segments with the coordinator")

	gpArray := greenplum.NewGpArray()
	err = gpArray.ReadGpSegmentConfig(conn)
	if err != nil {
		return utils.LogAndReturnError(err)
	}
	conn.Close()

	primarySegs, err := gpArray.GetPrimarySegments()
	if err != nil {
		return utils.LogAndReturnError(err)
	}

	var coordinatorAddrs []string
	if request.ClusterParams.HbaHostnames {
		coordinatorAddrs = append(coordinatorAddrs, request.GpArray.Coordinator.HostAddress)
	} else {
		addrs, err := utils.GetHostAddrsNoLoopback()
		if err != nil {
			return utils.LogAndReturnError(err)
		}

		coordinatorAddrs = append(coordinatorAddrs, addrs...)
	}

	hubStream.StreamLogMsg("Creating primary segments")
	err = s.CreateSegments(&hubStream, primarySegs, request.ClusterParams, coordinatorAddrs)
	if err != nil {
		return utils.LogAndReturnError(err)
	}
	hubStream.StreamLogMsg("Successfully created primary segments")

	shutdownCoordinator = false

	hubStream.StreamLogMsg("Restarting the Greenplum cluster in production mode")
	err = s.StopCoordinator(&hubStream, request.GpArray.Coordinator.DataDirectory)
	if err != nil {
		return utils.LogAndReturnError(err)
	}

	// TODO: Replace this with the new gp start once it is complete
	gpstartOptions := &greenplum.GpStart{
		DataDirectory: request.GpArray.Coordinator.DataDirectory,
		Verbose:       request.Verbose,
	}
	cmd := utils.NewGpSourcedCommand(gpstartOptions, s.GpHome)
	err = hubStream.StreamExecCommand(cmd, s.GpHome)
	if err != nil {
		return utils.LogAndReturnError(fmt.Errorf("executing gpstart: %w", err))
	}
	hubStream.StreamLogMsg("Completed restart of Greenplum cluster in production mode")

	hubStream.StreamLogMsg("Creating core GPDB extensions")
	err = CreateGpToolkitExt(conn)
	if err != nil {
		return utils.LogAndReturnError(err)
	}
	hubStream.StreamLogMsg("Successfully created core GPDB extensions")

	hubStream.StreamLogMsg("Importing system collations")
	err = ImportCollation(conn)
	if err != nil {
		return utils.LogAndReturnError(err)
	}

	if request.ClusterParams.DbName != "" {
		hubStream.StreamLogMsg(fmt.Sprintf("Creating database %q", request.ClusterParams.DbName))
		err = CreateDatabase(conn, request.ClusterParams.DbName)
		if err != nil {
			return utils.LogAndReturnError(err)
		}
	}

	hubStream.StreamLogMsg("Setting Greenplum superuser password")
	err = SetGpUserPasswd(conn, request.ClusterParams.SuPassword)
	if err != nil {
		return utils.LogAndReturnError(err)
	}

	return nil
}

func (s *Server) ValidateEnvironment(stream hubStreamer, request *idl.MakeClusterRequest) error {
	var replies []*idl.LogMessage

	gparray := request.GpArray
	hostDirMap := make(map[string][]string)
	hostPortMap := make(map[string][]string)
	hostAddressMap := make(map[string]map[string]bool)

	// Add coordinator to the map
	hostDirMap[gparray.Coordinator.HostName] = append(hostDirMap[gparray.Coordinator.HostName], gparray.Coordinator.DataDirectory)
	hostPortMap[gparray.Coordinator.HostName] = append(hostPortMap[gparray.Coordinator.HostName], fmt.Sprintf("%d", gparray.Coordinator.Port))
	hostAddressMap[gparray.Coordinator.HostName] = make(map[string]bool)
	hostAddressMap[gparray.Coordinator.HostName][gparray.Coordinator.HostAddress] = true

	// Add primaries to the map
	for _, seg := range request.GetPrimarySegments() {
		hostDirMap[seg.HostName] = append(hostDirMap[seg.HostName], seg.DataDirectory)
		hostPortMap[seg.HostName] = append(hostPortMap[seg.HostName], fmt.Sprintf("%d", seg.Port))

		if hostAddressMap[seg.HostName] == nil {
			hostAddressMap[seg.HostName] = make(map[string]bool)
		}
		hostAddressMap[seg.HostName][seg.HostAddress] = true
	}
	gplog.Debug("Host-Address-Map:[%v]", hostAddressMap)

	// Get local gpVersion

	localPgVersion, err := greenplum.GetPostgresGpVersion(s.GpHome)
	if err != nil {
		gplog.Error("fetching postgres gp-version:%v", err)
		return err
	}

	progressLabel := "Validating Hosts:"
	progressTotal := len(hostDirMap)
	stream.StreamProgressMsg(progressLabel, progressTotal)
	validateFn := func(conn *Connection) error {
		gplog.Debug(fmt.Sprintf("Starting to validate host: %s", conn.Hostname))

		dirList := hostDirMap[conn.Hostname]
		portList := hostPortMap[conn.Hostname]
		var addressList []string
		for address := range hostAddressMap[conn.Hostname] {
			addressList = append(addressList, address)
		}
		gplog.Debug("AddressList:[%v]", addressList)

		validateReq := idl.ValidateHostEnvRequest{
			DirectoryList:   dirList,
			Locale:          request.ClusterParams.Locale,
			PortList:        portList,
			Forced:          request.ForceFlag,
			HostAddressList: addressList,
			GpVersion:       localPgVersion,
		}
		reply, err := conn.AgentClient.ValidateHostEnv(context.Background(), &validateReq)
		if err != nil {
			return utils.FormatGrpcError(err)
		}

		stream.StreamProgressMsg(progressLabel, progressTotal)
		gplog.Debug(fmt.Sprintf("Successfully completed validation for host: %s", conn.Hostname))

		// Add host-name to each reply message
		for _, msg := range reply.Messages {
			msg.Message = fmt.Sprintf("Host: %s %s", conn.Hostname, msg.Message)
			replies = append(replies, msg)
		}

		return nil
	}

	err = ExecuteRPC(s.Conns, validateFn)
	if err != nil {
		return err
	}

	for _, msg := range replies {
		stream.StreamLogMsg(msg.Message, msg.Level)
	}

	return nil
}

func CreateSingleSegment(conn *Connection, seg *idl.Segment, clusterParams *idl.ClusterParams, coordinatorAddrs []string) error {
	pgConfig := make(map[string]string)
	maps.Copy(pgConfig, clusterParams.CommonConfig)
	if seg.Contentid == -1 {
		maps.Copy(pgConfig, clusterParams.CoordinatorConfig)
	} else {
		maps.Copy(pgConfig, clusterParams.SegmentConfig)
	}

	makeSegmentReq := &idl.MakeSegmentRequest{
		Segment:          seg,
		Locale:           clusterParams.Locale,
		Encoding:         clusterParams.Encoding,
		SegConfig:        pgConfig,
		CoordinatorAddrs: coordinatorAddrs,
		HbaHostNames:     clusterParams.HbaHostnames,
		DataChecksums:    clusterParams.DataChecksums,
	}

	_, err := conn.AgentClient.MakeSegment(context.Background(), makeSegmentReq)
	if err != nil {
		return utils.FormatGrpcError(err)
	}

	return nil
}

func (s *Server) CreateAndStartCoordinator(seg *idl.Segment, clusterParams *idl.ClusterParams) error {
	coordinatorConn := getConnByHost(s.Conns, []string{seg.HostName})

	seg.Contentid = -1
	seg.Dbid = 1
	request := func(conn *Connection) error {
		err := CreateSingleSegment(conn, seg, clusterParams, []string{})
		if err != nil {
			return err
		}

		startSegReq := &idl.StartSegmentRequest{
			DataDir: seg.DataDirectory,
			Wait:    true,
			Options: "-c gp_role=utility",
		}
		_, err = conn.AgentClient.StartSegment(context.Background(), startSegReq)

		return utils.FormatGrpcError(err)
	}

	return ExecuteRPC(coordinatorConn, request)
}

func (s *Server) StopCoordinator(stream hubStreamer, pgdata string) error {
	stream.StreamLogMsg("Shutting down coordinator segment")
	pgCtlStopCmd := &postgres.PgCtlStop{
		PgData: pgdata,
	}

	out, err := utils.RunExecCommand(pgCtlStopCmd, s.GpHome)
	if err != nil {
		return fmt.Errorf("executing pg_ctl stop: %s, %w", out, err)
	}
	stream.StreamLogMsg("Successfully shut down coordinator segment")

	return nil
}

func (s *Server) CreateSegments(stream hubStreamer, segs []greenplum.Segment, clusterParams *idl.ClusterParams, coordinatorAddrs []string) error {
	hostSegmentMap := map[string][]*idl.Segment{}
	for _, seg := range segs {
		segReq := &idl.Segment{
			Port:          int32(seg.Port),
			DataDirectory: seg.DataDir,
			HostName:      seg.HostName,
			HostAddress:   seg.HostAddress,
			Contentid:     int32(seg.ContentId),
			Dbid:          int32(seg.Dbid),
		}

		if _, ok := hostSegmentMap[seg.HostName]; !ok {
			hostSegmentMap[seg.HostName] = []*idl.Segment{segReq}
		} else {
			hostSegmentMap[seg.HostName] = append(hostSegmentMap[seg.HostName], segReq)
		}
	}

	progressLabel := "Initializing primary segments:"
	progressTotal := len(segs)
	stream.StreamProgressMsg(progressLabel, progressTotal)

	request := func(conn *Connection) error {
		var wg sync.WaitGroup

		segs := hostSegmentMap[conn.Hostname]
		errs := make(chan error, len(segs))
		for _, seg := range segs {
			seg := seg
			wg.Add(1)
			go func(seg *idl.Segment) {
				defer wg.Done()

				gplog.Debug(fmt.Sprintf("Starting to create primary segment: %s", seg))
				err := CreateSingleSegment(conn, seg, clusterParams, coordinatorAddrs)
				if err != nil {
					errs <- err
				} else {
					stream.StreamProgressMsg(progressLabel, progressTotal)
					gplog.Debug(fmt.Sprintf("Successfully created primary segment: %s", seg))
				}
			}(seg)
		}

		wg.Wait()
		close(errs)

		var err error
		for e := range errs {
			err = errors.Join(err, e)
		}
		return err
	}

	return ExecuteRPC(s.Conns, request)
}

func ExecOnDatabase(conn *dbconn.DBConn, dbname string, query string) error {
	conn.DBName = dbname
	if err := conn.Connect(1); err != nil {
		return err
	}
	defer conn.Close()

	if _, err := conn.Exec(query); err != nil {
		return err
	}

	return nil
}

func CreateGpToolkitExt(conn *dbconn.DBConn) error {
	createExtensionQuery := "CREATE EXTENSION gp_toolkit"

	for _, dbname := range []string{constants.DefaultDatabase, "postgres"} {
		if err := execOnDatabaseFunc(conn, dbname, createExtensionQuery); err != nil {
			return err
		}
	}

	return nil
}

func ImportCollation(conn *dbconn.DBConn) error {
	importCollationQuery := "SELECT pg_import_system_collations('pg_catalog'); ANALYZE;"

	if err := execOnDatabaseFunc(conn, "postgres", "ALTER DATABASE template0 ALLOW_CONNECTIONS on"); err != nil {
		return err
	}

	if err := execOnDatabaseFunc(conn, "template0", importCollationQuery); err != nil {
		return err
	}
	if err := execOnDatabaseFunc(conn, "template0", "VACUUM FREEZE"); err != nil {
		return err
	}

	if err := execOnDatabaseFunc(conn, "postgres", "ALTER DATABASE template0 ALLOW_CONNECTIONS off"); err != nil {
		return err
	}

	for _, dbname := range []string{constants.DefaultDatabase, "postgres"} {
		if err := execOnDatabaseFunc(conn, dbname, importCollationQuery); err != nil {
			return err
		}

		if err := execOnDatabaseFunc(conn, dbname, "VACUUM FREEZE"); err != nil {
			return err
		}
	}

	return nil
}

func CreateDatabase(conn *dbconn.DBConn, dbname string) error {
	createDbQuery := fmt.Sprintf("CREATE DATABASE %q", dbname)
	if err := execOnDatabaseFunc(conn, constants.DefaultDatabase, createDbQuery); err != nil {
		return err
	}

	return nil
}

func SetGpUserPasswd(conn *dbconn.DBConn, passwd string) error {
	user, err := utils.System.CurrentUser()
	if err != nil {
		return err
	}

	alterPasswdQuery := fmt.Sprintf("ALTER USER %q WITH PASSWORD '%s'", user.Username, passwd)
	if err := execOnDatabaseFunc(conn, constants.DefaultDatabase, alterPasswdQuery); err != nil {
		return err
	}

	return nil
}

func SetExecOnDatabase(customFunc func(*dbconn.DBConn, string, string) error) {
	execOnDatabaseFunc = customFunc
}

func ResetExecOnDatabase() {
	execOnDatabaseFunc = ExecOnDatabase
}
