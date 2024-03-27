package greenplum_test

import (
	"os"
	"strings"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/greenplum"
)

func init() {
	exectest.RegisterMains(
		PgVersionCmd,
	)
}

func TestGetPostgresGpVersion(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns error if command execution fails", func(t *testing.T) {
		expectedStr := "fetching postgres gp-version:"
		utils.System.ExecCommand = exectest.NewCommand(exectest.Failure)
		defer utils.ResetSystemFunctions()
		_, err := greenplum.GetPostgresGpVersion("/gpHome")
		if err == nil || !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("expected errror: `%s`, got error:`%v`", expectedStr, err)
		}

	})
	t.Run("returns no error when command is successful", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Success)
		defer utils.ResetSystemFunctions()
		_, err := greenplum.GetPostgresGpVersion("/gpHome")
		if err != nil {
			t.Fatalf("expected no errror, got error:`%v`", err)
		}

	})
	t.Run("returns gp-version correctly by eliminating trailing spaces", func(t *testing.T) {
		expectedStr := "test-version-1234"
		utils.System.ExecCommand = exectest.NewCommand(PgVersionCmd)
		defer utils.ResetSystemFunctions()
		version, err := greenplum.GetPostgresGpVersion("/gpHome")
		if err != nil || !strings.Contains(version, expectedStr) {
			t.Fatalf("expected version: `%s`, got version:`%v`", expectedStr, version)
		}

	})
}

func PgVersionCmd() {
	os.Stdout.WriteString("   test-version-1234   ")
}
