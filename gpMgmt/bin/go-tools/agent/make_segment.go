package agent

import (
	"context"
	"fmt"
	"strconv"

	"golang.org/x/exp/maps"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/postgres"
)

// MakeSegment is an RPC which creates a new segment instance with the specified
// configuration from the MakeSegmentRequest. It calls initdb and updates the
// necessary configuration files.
func (s *Server) MakeSegment(ctx context.Context, request *idl.MakeSegmentRequest) (*idl.MakeSegmentReply, error) {
	dataDirectory := request.Segment.DataDirectory
	locale := request.Locale

	gplog.Debug("Creating segment with data directory %q", dataDirectory)

	initdbOptions := postgres.Initdb{
		PgData:        dataDirectory,
		Encoding:      request.Encoding,
		Locale:        locale.LcAll,
		LcCollate:     locale.LcCollate,
		LcCtype:       locale.LcCtype,
		LcMessages:    locale.LcMessages,
		LcMonetory:    locale.LcMonetory,
		LcNumeric:     locale.LcNumeric,
		LcTime:        locale.LcTime,
		DataChecksums: request.DataChecksums,
	}
	out, err := utils.RunExecCommand(&initdbOptions, s.GpHome)
	if err != nil {
		return &idl.MakeSegmentReply{}, utils.LogAndReturnError(fmt.Errorf("executing initdb: %s, %w", out, err))
	}

	configParams := make(map[string]string)
	maps.Copy(configParams, request.SegConfig)
	configParams["port"] = strconv.Itoa(int(request.Segment.Port))
	configParams["listen_addresses"] = "*"
	configParams["gp_contentid"] = strconv.Itoa(int(request.Segment.Contentid))
	if request.Segment.Contentid == -1 {
		configParams["log_statement"] = "all"
	}

	err = postgres.UpdatePostgresqlConf(dataDirectory, configParams, false)
	if err != nil {
		return &idl.MakeSegmentReply{}, utils.LogAndReturnError(fmt.Errorf("updating postgresql.conf: %w", err))
	}

	err = postgres.CreatePostgresInternalConf(dataDirectory, int(request.Segment.Dbid))
	if err != nil {
		return &idl.MakeSegmentReply{}, utils.LogAndReturnError(fmt.Errorf("creating internal.auto.conf: %w", err))
	}

	var addrs []string
	if request.HbaHostNames {
		addrs = append(addrs, request.Segment.HostAddress)
	} else {
		hostAddrs, err := utils.GetHostAddrsNoLoopback()
		if err != nil {
			return &idl.MakeSegmentReply{}, utils.LogAndReturnError(err)
		}

		addrs = append(addrs, hostAddrs...)
	}

	if request.Segment.Contentid == -1 {
		err = postgres.BuildCoordinatorPgHbaConf(dataDirectory, addrs)
	} else {
		err = postgres.UpdateSegmentPgHbaConf(dataDirectory, request.CoordinatorAddrs, addrs)
	}
	if err != nil {
		return &idl.MakeSegmentReply{}, utils.LogAndReturnError(fmt.Errorf("updating pg_hba.conf: %w", err))
	}

	gplog.Debug("Successfully created segment with data directory %q", dataDirectory)

	return &idl.MakeSegmentReply{}, nil
}
