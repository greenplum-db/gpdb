package postgres_test

import (
	"os"
	"os/exec"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/postgres"
)

func init() {
	exectest.RegisterMains(
		PgCommandSuccess,
		PgCommandFailure,
	)
}

// Enable exectest.NewCommand mocking.
func TestMain(m *testing.M) {
	os.Exit(exectest.Run(m))
}

func TestPgCommand(t *testing.T) {
	_, _, logfile := testhelper.SetupTestLogger()

	cases := []struct {
		pgCmdOptions utils.CommandBuilder
		expected     string
	}{
		{
			pgCmdOptions: &postgres.Initdb{
				PgData:   "pgdata",
				Encoding: "encoding",
				Locale:   "locale",
			},
			expected: `gpHome/bin/initdb --pgdata pgdata --encoding encoding --locale locale`,
		},
		{
			pgCmdOptions: &postgres.PgCtlStart{
				PgData:  "pgdata",
				Timeout: 10,
				Wait:    true,
				NoWait:  false,
				Logfile: "logfile",
			},
			expected: `gpHome/bin/pg_ctl start --pgdata pgdata --timeout 10 --wait --log logfile`,
		},
		{
			pgCmdOptions: &postgres.PgCtlStop{
				PgData:  "pgdata",
				Timeout: 10,
				Wait:    true,
				NoWait:  false,
				Mode:    "smart",
			},
			expected: `gpHome/bin/pg_ctl stop --pgdata pgdata --timeout 10 --wait --mode smart`,
		},
		{
			pgCmdOptions: &postgres.Postgres{
				GpVersion: true,
			},
			expected: `gpHome/bin/postgres --gp-version`,
		},
	}

	for _, tc := range cases {
		t.Run("builds the correct command", func(t *testing.T) {
			pgCmd := utils.NewExecCommand(tc.pgCmdOptions, "gpHome")
			if pgCmd.String() != tc.expected {
				t.Fatalf("got %s, want %s", pgCmd.String(), tc.expected)
			}
		})
	}

	t.Run("returns the correct command output on success", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(PgCommandSuccess)
		defer utils.ResetSystemFunctions()

		pgCmdOptions := &postgres.Initdb{
			PgData:   "pgdata",
			Encoding: "encoding",
		}
		out, err := utils.RunExecCommand(pgCmdOptions, "gpHome")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		if out.String() != "success" {
			t.Fatalf("got %s, want success", out)
		}

		expectedLogMsg := `\[DEBUG\]:-Executing command: .* --pgdata pgdata --encoding encoding`
		testutils.AssertLogMessage(t, logfile, expectedLogMsg)
	})

	t.Run("returns the correct command output on failure", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(PgCommandFailure)
		defer utils.ResetSystemFunctions()

		pgCmdOptions := &postgres.Initdb{
			PgData:   "pgdata",
			Encoding: "encoding",
		}
		out, err := utils.RunExecCommand(pgCmdOptions, "gpHome")
		if status, ok := err.(*exec.ExitError); !ok || status.ExitCode() != 1 {
			t.Fatalf("unexpected error: %+v", err)
		}

		if out.String() != "failure" {
			t.Fatalf("got %s, want failure", out)
		}

		expectedLogMsg := `\[DEBUG\]:-Executing command: .* --pgdata pgdata --encoding encoding`
		testutils.AssertLogMessage(t, logfile, expectedLogMsg)
	})
}

func PgCommandSuccess() {
	os.Stdout.WriteString("success")
	os.Exit(0)
}

func PgCommandFailure() {
	os.Stderr.WriteString("failure")
	os.Exit(1)
}
