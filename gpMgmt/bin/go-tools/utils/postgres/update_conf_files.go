package postgres

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strconv"
	"strings"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/utils"
)

const (
	postgresqlConfFile       = "postgresql.conf"
	postgresInternalConfFile = "internal.auto.conf"
	pgHbaConfFile            = "pg_hba.conf"
)

// UpdatePostgresqlConf updates given config params to postgresql.conf file
func UpdatePostgresqlConf(pgdata string, configParams map[string]string, overwrite bool) error {
	gplog.Debug("Updating %s for data directory %s with: %s", postgresqlConfFile, pgdata, configParams)
	err := updateConfFile(postgresqlConfFile, pgdata, configParams, overwrite)
	if err != nil {
		return err
	}

	gplog.Info("Successfully updated %s for data directory %s", postgresqlConfFile, pgdata)
	return nil
}

func CreatePostgresInternalConf(pgdata string, dbid int) error {
	postgresInternalConfFilePath := filepath.Join(pgdata, postgresInternalConfFile)
	file, err := utils.System.OpenFile(postgresInternalConfFilePath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		return err
	}
	defer file.Close()

	contents := fmt.Sprintf("gp_dbid = %d", dbid)
	_, err = file.WriteString(contents)
	if err != nil {
		return err
	}

	gplog.Info("Successfully created %s for data directory %s", postgresInternalConfFile, pgdata)
	return nil
}

func BuildCoordinatorPgHbaConf(pgdata string, addrs []string) error {
	pgHbaFilePath := filepath.Join(pgdata, pgHbaConfFile)

	file, err := utils.System.Open(pgHbaFilePath)
	if err != nil {
		return err
	}
	defer file.Close()

	updatedLines := []string{}
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()

		if strings.HasPrefix(line, "#") {
			updatedLines = append(updatedLines, line)
		}
	}

	user, err := utils.System.CurrentUser()
	if err != nil {
		return err
	}

	localAccessString := "local\t%s\t%s\tident"

	// Add access entries
	line := fmt.Sprintf(localAccessString, "all", user.Username)
	updatedLines = append(updatedLines, line)
	addPgHbaEntries(&updatedLines, []string{"localhost"}, "all", user.Username)
	addPgHbaEntries(&updatedLines, addrs, "all", user.Username)

	// Add replication entries
	line = fmt.Sprintf(localAccessString, "replication", user.Username)
	updatedLines = append(updatedLines, line)
	addPgHbaEntries(&updatedLines, []string{"samehost"}, "replication", user.Username)
	addPgHbaEntries(&updatedLines, addrs, "replication", user.Username)

	err = utils.WriteLinesToFile(pgHbaFilePath, updatedLines)
	if err != nil {
		return err
	}

	gplog.Info("Successfully updated %s for data directory %s", pgHbaConfFile, pgdata)
	return nil
}

/*
UpdateSegmentPgHbaConf updates pg_hba.conf file with the given addresses.
For coordinator entry adds all users access and for other addresses, adds user level access.
*/
func UpdateSegmentPgHbaConf(pgdata string, coordinatorAddrs []string, addrs []string) error {
	pgHbaFilePath := filepath.Join(pgdata, pgHbaConfFile)

	user, err := utils.System.CurrentUser()
	if err != nil {
		return err
	}

	updatedLines := []string{}
	addPgHbaEntries(&updatedLines, coordinatorAddrs, "all", "all") // add coordinator entries
	addPgHbaEntries(&updatedLines, addrs, "all", user.Username)    // add access entries

	err = utils.AppendLinesToFile(pgHbaFilePath, updatedLines)
	if err != nil {
		return err
	}

	gplog.Info("Successfully updated %s for data directory %s", pgHbaConfFile, pgdata)
	return nil
}

/*
updateConfFile updates postresql.conf file with the provided config params.
If the config exists, comments the existing line with a # and adds a new line.
*/
func updateConfFile(filename, pgdata string, configParams map[string]string, overwrite bool) error {
	var line string
	confFilePath := filepath.Join(pgdata, filename)

	file, err := utils.System.Open(confFilePath)
	if err != nil {
		return err
	}
	defer file.Close()

	updatedLines := []string{}
	updatedKeys := []string{}
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line = scanner.Text()

		if strings.HasPrefix(line, "#") {
			// Skip the commented line
			updatedLines = append(updatedLines, line)
			continue
		}

		for key, value := range configParams {
			pattern, err := regexp.Compile(fmt.Sprintf("^%s[\\s=]+", key))
			if err != nil {
				return err
			}

			if pattern.MatchString(line) {
				if !overwrite {
					line = fmt.Sprintf("%s = %s # %s", key, quoteIfString(value), line) // comment the previous entry
				} else {
					line = fmt.Sprintf("%s = %s", key, quoteIfString(value))
				}

				// Mark the key as updated to delete later from configParams
				updatedKeys = append(updatedKeys, key)
				break
			}
		}

		updatedLines = append(updatedLines, line)
	}

	// Delete the updated keys to avoid duplicated entries
	for _, key := range updatedKeys {
		delete(configParams, key)
	}

	// Add the remaining entries
	for key, value := range configParams {
		line := fmt.Sprintf("%s = %s", key, quoteIfString(value))
		updatedLines = append(updatedLines, line)
	}

	err = utils.WriteLinesToFile(confFilePath, updatedLines)
	if err != nil {
		return err
	}

	return nil
}

/*
addPgHbaEntries adds an entry to ph_hba.conf entries.
Entries are added with specified address, access-type and the user
*/
func addPgHbaEntries(existingEntries *[]string, addrs []string, accessType string, user string) {
	var hostAccessString = "host\t%s\t%s\t%s\ttrust"

	for _, addr := range addrs {
		line := fmt.Sprintf(hostAccessString, accessType, user, addr)
		if !slices.Contains(*existingEntries, line) {
			*existingEntries = append(*existingEntries, line)
		}
	}
}

/*
quoteIfString encloses string values inside quotes.
Checks if string is a number, then skips adding quotes to the string.
*/
func quoteIfString(value string) string {
	if _, err := strconv.ParseFloat(value, 64); err == nil {
		return value
	}

	return fmt.Sprintf("'%s'", value)
}
