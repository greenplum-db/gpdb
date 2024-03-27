package agent_test

import (
	"context"
	"errors"
	"fmt"
	"net"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/agent"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func init() {
	exectest.RegisterMains()
}

// Enable exectest.NewCommand mocking.
func TestMain(m *testing.M) {
	os.Exit(exectest.Run(m))
}

func TestMakeSegment(t *testing.T) {
	testhelper.SetupTestLogger()

	agentServer := agent.New(agent.Config{
		GpHome: "gpHome",
	})

	request := &idl.MakeSegmentRequest{
		Segment: &idl.Segment{
			Port:        1234,
			HostName:    "sdw",
			HostAddress: "sdw",
			Dbid:        1,
		},
		Locale: &idl.Locale{},
		SegConfig: map[string]string{
			"key1": "value1",
			"key2": "value2",
		},
		DataChecksums: true,
	}

	cases := []struct {
		contentId              int32
		coordinatorAddrs       []string
		hbaHostname            bool
		expectedPostgresqlConf string
		expectedPgHbaConf      string
	}{
		{ // coordinator segment
			contentId:   -1,
			hbaHostname: true,
			expectedPostgresqlConf: fmt.Sprintf(`listen_addresses = '*'
gp_contentid = -1
log_statement = 'all'
key1 = 'value1'
key2 = 'value2'
port = %d
`, request.Segment.Port),
			expectedPgHbaConf: fmt.Sprintf(`local	all	gpadmin	ident
host	all	gpadmin	localhost	trust
host	all	gpadmin	%[1]s	trust
local	replication	gpadmin	ident
host	replication	gpadmin	samehost	trust
host	replication	gpadmin	%[1]s	trust
`, request.Segment.HostName),
		},
		{ // primary segment
			contentId:        0,
			coordinatorAddrs: []string{"cdw"},
			hbaHostname:      true,
			expectedPostgresqlConf: fmt.Sprintf(`listen_addresses = '*'
gp_contentid = 0
key1 = 'value1'
key2 = 'value2'
port = %d
`, request.Segment.Port),
			expectedPgHbaConf: fmt.Sprintf(`host	all	all	cdw	trust
host	all	gpadmin	%[1]s	trust
`, request.Segment.HostName),
		},
		{ // with hbaHostname set to false
			contentId:        0,
			coordinatorAddrs: []string{"192.0.0.0/24"},
			hbaHostname:      false,
			expectedPostgresqlConf: fmt.Sprintf(`listen_addresses = '*'
gp_contentid = 0
key1 = 'value1'
key2 = 'value2'
port = %d
`, request.Segment.Port),
			expectedPgHbaConf: `host	all	all	192.0.0.0/24	trust
host	all	gpadmin	192.0.1.0/24	trust
host	all	gpadmin	2001:db8::/32	trust
`,
		},
	}

	for _, tc := range cases {
		t.Run("successfully creates a segment instance", func(t *testing.T) {
			var initdbCalled bool
			var initdbArgs []string

			utils.System.ExecCommand = exectest.NewCommandWithVerifier(exectest.Success, func(utility string, args ...string) {
				initdbCalled = true
				expectedUtility := "gpHome/bin/initdb"
				if utility != expectedUtility {
					t.Fatalf("got %s, want %s", utility, expectedUtility)
				}

				initdbArgs = args
			})

			utils.System.CurrentUser = func() (*user.User, error) {
				return &user.User{Username: "gpadmin"}, nil
			}

			utils.System.InterfaceAddrs = func() ([]net.Addr, error) {
				_, addr1, _ := net.ParseCIDR("192.0.1.0/24")
				_, addr2, _ := net.ParseCIDR("2001:db8::/32")
				_, loopbackAddrIp4, _ := net.ParseCIDR("127.0.0.1/8")
				_, loopbackAddrIp6, _ := net.ParseCIDR("::1/128")

				return []net.Addr{addr1, addr2, loopbackAddrIp4, loopbackAddrIp6}, nil
			}
			defer utils.ResetSystemFunctions()

			dname, err := os.MkdirTemp("", "gpseg")
			if err != nil {
				t.Fatalf("unexpected error: %#v", err)
			}
			defer os.RemoveAll(dname)

			postgresqlConf := creatTempConfFile(t, dname, "postgresql.conf", 0644)
			pgHbaConf := creatTempConfFile(t, dname, "pg_hba.conf", 0644)
			pgInternalConf := filepath.Join(dname, "internal.auto.conf")

			request.Segment.DataDirectory = dname
			request.Segment.Contentid = tc.contentId
			request.CoordinatorAddrs = tc.coordinatorAddrs
			request.HbaHostNames = tc.hbaHostname
			_, err = agentServer.MakeSegment(context.Background(), request)
			if err != nil {
				t.Fatalf("unexpected error: %#v", err)
			}

			if !initdbCalled {
				t.Fatalf("expected initdb to be called")
			}
			expectedArgs := []string{"--pgdata", dname, "--data-checksums"}
			if !reflect.DeepEqual(initdbArgs, expectedArgs) {
				t.Fatalf("got %+v, want %+v", initdbArgs, expectedArgs)
			}

			expectedPgInternalConf := fmt.Sprintf("gp_dbid = %d", request.Segment.Dbid)

			testutils.AssertFileContentsUnordered(t, postgresqlConf, tc.expectedPostgresqlConf)
			testutils.AssertFileContents(t, pgInternalConf, expectedPgInternalConf)
			testutils.AssertFileContents(t, pgHbaConf, tc.expectedPgHbaConf)
		})
	}

	t.Run("errors out when initdb fails to run", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Failure)
		defer utils.ResetSystemFunctions()

		expectedErrPrefix := "executing initdb:"
		_, err := agentServer.MakeSegment(context.Background(), request)
		if !strings.HasPrefix(err.Error(), expectedErrPrefix) {
			t.Fatalf("got %v, want prefix %v", err, expectedErrPrefix)
		}

		var expectedErr *exec.ExitError
		if !errors.As(err, &expectedErr) {
			t.Errorf("got %T, want %T", err, expectedErr)
		}
	})

	t.Run("errors out when it fails to update the postgresql.conf", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Success)
		defer utils.ResetSystemFunctions()

		dname, err := os.MkdirTemp("", "gpseg")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.RemoveAll(dname)

		creatTempConfFile(t, dname, "postgresql.conf", 0000)

		request.Segment.DataDirectory = dname
		_, err = agentServer.MakeSegment(context.Background(), request)

		expectedErrPrefix := "updating postgresql.conf:"
		if !strings.HasPrefix(err.Error(), expectedErrPrefix) {
			t.Fatalf("got %v, want %v", err, expectedErrPrefix)
		}

		expectedErr := os.ErrPermission
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("errors out when it fails to create internal.auto.conf", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Success)
		defer utils.ResetSystemFunctions()

		dname, err := os.MkdirTemp("", "gpseg")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.RemoveAll(dname)

		creatTempConfFile(t, dname, "postgresql.conf", 0644)
		creatTempConfFile(t, dname, "internal.auto.conf", 0000)

		request.Segment.DataDirectory = dname
		_, err = agentServer.MakeSegment(context.Background(), request)

		expectedErrPrefix := "creating internal.auto.conf:"
		if !strings.HasPrefix(err.Error(), expectedErrPrefix) {
			t.Fatalf("got %v, want %v", err, expectedErrPrefix)
		}

		expectedErr := os.ErrPermission
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("errors out when it fails to get host address", func(t *testing.T) {
		expectedErr := errors.New("error")
		utils.System.ExecCommand = exectest.NewCommand(exectest.Success)
		utils.System.InterfaceAddrs = func() ([]net.Addr, error) {
			return nil, expectedErr
		}
		defer utils.ResetSystemFunctions()

		dname, err := os.MkdirTemp("", "gpseg")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.RemoveAll(dname)

		creatTempConfFile(t, dname, "postgresql.conf", 0644)
		creatTempConfFile(t, dname, "internal.auto.conf", 0644)

		request.Segment.DataDirectory = dname
		_, err = agentServer.MakeSegment(context.Background(), request)

		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("errors out when it fails to update pg_hba.conf", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Success)
		defer utils.ResetSystemFunctions()

		dname, err := os.MkdirTemp("", "gpseg")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.RemoveAll(dname)

		creatTempConfFile(t, dname, "postgresql.conf", 0644)
		creatTempConfFile(t, dname, "pg_hba.conf", 0000)

		request.Segment.DataDirectory = dname
		_, err = agentServer.MakeSegment(context.Background(), request)

		expectedErrPrefix := "updating pg_hba.conf:"
		if !strings.HasPrefix(err.Error(), expectedErrPrefix) {
			t.Fatalf("got %v, want %v", err, expectedErrPrefix)
		}

		expectedErr := os.ErrPermission
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})
}

func creatTempConfFile(t *testing.T, dname string, filename string, perm os.FileMode) string {
	t.Helper()

	confPath := filepath.Join(dname, filename)
	err := os.WriteFile(confPath, []byte(""), perm)
	if err != nil {
		t.Fatalf("unexpected error: %#v", err)
	}

	return confPath
}
