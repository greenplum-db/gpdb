package greenplum_test

import (
	"os"
	"os/exec"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/greenplum"
)

func init() {
	exectest.RegisterMains(
		GpCommandSuccess,
		GpCommandFailure,
	)
}

// Enable exectest.NewCommand mocking.
func TestMain(m *testing.M) {
	os.Exit(exectest.Run(m))
}

func TestGpCommand(t *testing.T) {
	_, _, logfile := testhelper.SetupTestLogger()

	cases := []struct {
		gpCmdOptions utils.CommandBuilder
		expected     string
	}{
		{
			gpCmdOptions: &greenplum.GpStart{
				DataDirectory: "gpseg",
			},
			expected: `gpHome/bin/gpstart -a -d gpseg`,
		},
	}

	for _, tc := range cases {
		t.Run("builds the correct command", func(t *testing.T) {
			gpCmd := utils.NewExecCommand(tc.gpCmdOptions, "gpHome")
			if gpCmd.String() != tc.expected {
				t.Fatalf("got %s, want %s", gpCmd.String(), tc.expected)
			}
		})
	}

	t.Run("returns the correct command output on success", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(GpCommandSuccess)
		defer utils.ResetSystemFunctions()

		gpCmdOptions := &greenplum.GpStart{
			DataDirectory: "gpseg",
		}
		out, err := utils.RunExecCommand(gpCmdOptions, "gpHome")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		if out.String() != "success" {
			t.Fatalf("got %s, want success", out)
		}

		expectedLogMsg := `\[DEBUG\]:-Executing command: .* -a -d gpseg`
		testutils.AssertLogMessage(t, logfile, expectedLogMsg)
	})

	t.Run("returns the correct command output on failure", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(GpCommandFailure)
		defer utils.ResetSystemFunctions()

		gpCmdOptions := &greenplum.GpStart{
			DataDirectory: "gpseg",
		}
		out, err := utils.RunExecCommand(gpCmdOptions, "gpHome")
		if status, ok := err.(*exec.ExitError); !ok || status.ExitCode() != 1 {
			t.Fatalf("unexpected error: %+v", err)
		}

		if out.String() != "failure" {
			t.Fatalf("got %s, want failure", out)
		}

		expectedLogMsg := `\[DEBUG\]:-Executing command: .* -a -d gpseg`
		testutils.AssertLogMessage(t, logfile, expectedLogMsg)
	})
}

func GpCommandSuccess() {
	os.Stdout.WriteString("success")
	os.Exit(0)
}

func GpCommandFailure() {
	os.Stderr.WriteString("failure")
	os.Exit(1)
}
