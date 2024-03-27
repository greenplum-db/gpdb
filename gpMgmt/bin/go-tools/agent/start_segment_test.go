package agent_test

import (
	"context"
	"reflect"
	"strings"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/agent"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func init() {
	exectest.RegisterMains()
}

func TestStartSegment(t *testing.T) {
	testhelper.SetupTestLogger()

	agentServer := agent.New(agent.Config{
		GpHome: "gpHome",
	})

	request := &idl.StartSegmentRequest{
		DataDir: "gpseg",
		Wait:    true,
		Timeout: 60,
		Options: "",
	}

	t.Run("succesfully starts the segment", func(t *testing.T) {
		var pgCtlCalled bool
		utils.System.ExecCommand = exectest.NewCommandWithVerifier(exectest.Success, func(utility string, args ...string) {
			pgCtlCalled = true
			expectedUtility := "gpHome/bin/pg_ctl"
			if utility != expectedUtility {
				t.Fatalf("got %s, want %s", utility, expectedUtility)
			}

			expectedArgs := []string{"start", "--pgdata", "gpseg", "--timeout", "60", "--wait", "--log", "gpseg/log/startup.log"}
			if !reflect.DeepEqual(args, expectedArgs) {
				t.Fatalf("got %+v, want %+v", args, expectedArgs)
			}
		})
		defer utils.ResetSystemFunctions()

		_, err := agentServer.StartSegment(context.Background(), request)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		if !pgCtlCalled {
			t.Fatalf("expected pg_ctl to be called")
		}
	})

	t.Run("returns appropriate error when it fails", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Failure)
		defer utils.ResetSystemFunctions()

		expectedErrPrefix := "executing pg_ctl start:"
		_, err := agentServer.StartSegment(context.Background(), request)
		if !strings.HasPrefix(err.Error(), expectedErrPrefix) {
			t.Fatalf("got %v, want %v", err, expectedErrPrefix)
		}
	})
}
