package greenplum

import (
	"fmt"
	"path/filepath"
	"strings"

	"github.com/greenplum-db/gp-common-go-libs/operating"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/postgres"
)

func GetPostgresGpVersion(gpHome string) (string, error) {
	pgGpVersionCmd := &postgres.Postgres{GpVersion: true}
	out, err := utils.RunExecCommand(pgGpVersionCmd, gpHome)
	if err != nil {
		return "", fmt.Errorf("fetching postgres gp-version: %w", err)
	}

	return strings.TrimSpace(out.String()), nil
}

func GetDefaultHubLogDir() string {
	currentUser, _ := operating.System.CurrentUser()

	return filepath.Join(currentUser.HomeDir, "gpAdminLogs")
}
