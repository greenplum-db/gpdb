package agent

import (
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
	"syscall"

	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/utils/greenplum"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/utils"
)

var (
	CheckDirEmpty          = CheckDirEmptyFn
	CheckFileOwnerGroup    = CheckFileOwnerGroupFn
	CheckExecutable        = CheckExecutableFn
	OsIsNotExist           = os.IsNotExist
	GetAllNonEmptyDir      = GetAllNonEmptyDirFn
	CheckFilePermissions   = CheckFilePermissionsFn
	GetAllAvailableLocales = GetAllAvailableLocalesFn
	ValidateLocaleSettings = ValidateLocaleSettingsFn
	ValidatePorts          = ValidatePortsFn
	VerifyPgVersion        = ValidatePgVersionFn
)

/*
ValidateHostEnv implements agent RPC to validate local host environment
Performs various checks on host like gpdb version, permissions to initdb, data directory exists, ports in use etc
*/

func (s *Server) ValidateHostEnv(ctx context.Context, request *idl.ValidateHostEnvRequest) (*idl.ValidateHostEnvReply, error) {
	gplog.Debug("Starting ValidateHostEnvFn for request:%v", request)
	dirList := request.DirectoryList
	locale := request.Locale
	portList := request.PortList
	forced := request.Forced

	// Check if user is non-root
	if utils.System.Getuid() == 0 {
		userInfo, err := utils.System.CurrentUser()
		if err != nil {
			return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(fmt.Errorf(
				"failed to get user name Error:%v. Current user is a root user. Can't create cluster under root", err))
		}
		return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(fmt.Errorf(
			"user:%s is a root user, Can't create cluster under root user", userInfo.Name))
	}

	//Check for GP Version
	gpVersionErr := VerifyPgVersion(request.GpVersion, s.GpHome)
	if gpVersionErr != nil {
		return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(fmt.Errorf("Postgres gp-version validation failed:%v", gpVersionErr))
	}

	// Check for each directory if is empty
	nonEmptyDirList, err := GetAllNonEmptyDir(dirList)
	if err != nil {
		return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(fmt.Errorf("error checking directory empty:%v", err))
	}

	if len(nonEmptyDirList) > 0 && !forced {
		return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(fmt.Errorf("directory not empty:%v", nonEmptyDirList))
	}
	if forced && len(nonEmptyDirList) > 0 {

		gplog.Debug("Forced init. Deleting non-empty directories:%s", nonEmptyDirList)
		for _, dir := range nonEmptyDirList {
			err := utils.System.RemoveAll(dir)
			if err != nil {
				return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(fmt.Errorf("delete not empty dir:%s, error:%v", dir, err))
			}
		}
	}

	// Validate permission to initdb? Error will be returned upon running
	initdbPath := filepath.Join(s.GpHome, "bin", "initdb")
	err = CheckFilePermissions(initdbPath)
	if err != nil {
		return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(err)
	}

	// Validate that the different locale settings are available on the system
	err = ValidateLocaleSettings(locale)
	if err != nil {
		return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(err)
	}

	// Check if port in use
	err = ValidatePorts(portList)
	if err != nil {
		return &idl.ValidateHostEnvReply{}, utils.LogAndReturnError(err)
	}

	// Any checks to raise warnings
	var warnings []*idl.LogMessage

	// check coordinator open file values
	warnings = CheckOpenFilesLimit()
	addressWarnings := CheckHostAddressInHostsFile(request.HostAddressList)
	warnings = append(warnings, addressWarnings...)
	return &idl.ValidateHostEnvReply{Messages: warnings}, nil
}

/*
ValidatePgVersionFn gets current version of gpdb and compares with version from coordinator
returns error if version do not match.
*/
func ValidatePgVersionFn(expectedVersion string, gpHome string) error {
	localPgVersion, err := greenplum.GetPostgresGpVersion(gpHome)
	if err != nil {
		return err
	}

	if expectedVersion != localPgVersion {
		return fmt.Errorf("postgres gp-version does not matches with coordinator postgres gp-version."+
			"Coordinator version:'%s', Current version:'%s'", expectedVersion, localPgVersion)
	}
	return nil

}

/*
CheckOpenFilesLimit sends a warning to CLI if the open files limit is not unlimited
*/

func CheckOpenFilesLimit() []*idl.LogMessage {
	var warnings []*idl.LogMessage
	ulimitVal, err := utils.ExecuteAndGetUlimit()
	if err != nil {
		warnMsg := fmt.Sprintf("error getting open files limit:%s", err.Error())
		warnings = append(warnings, &idl.LogMessage{Message: warnMsg, Level: idl.LogLevel_WARNING})
		gplog.Warn(warnMsg)
		return warnings
	}

	if ulimitVal < constants.OsOpenFiles {
		warnMsg := fmt.Sprintf("Host open file limit is %d should be >= %d. Set open files limit for user and systemd and start gp services again.", ulimitVal, constants.OsOpenFiles)
		warnings = append(warnings, &idl.LogMessage{Message: warnMsg, Level: idl.LogLevel_WARNING})
		gplog.Warn(warnMsg)
		return warnings
	}
	return warnings
}

/*
CheckHostAddressInHostsFile checks if given address present with a localhost entry.
Returns a warning message to CLI if entry is detected
*/
func CheckHostAddressInHostsFile(hostAddressList []string) []*idl.LogMessage {
	var warnings []*idl.LogMessage
	gplog.Debug("CheckHostAddressInHostsFile checking for address:%v", hostAddressList)
	content, err := utils.System.ReadFile(constants.EtcHostsFilepath)
	if err != nil {
		warnMsg := fmt.Sprintf("error reading file %s error:%v", constants.EtcHostsFilepath, err)
		gplog.Warn(warnMsg)
		warnings = append(warnings, &idl.LogMessage{Message: warnMsg, Level: idl.LogLevel_WARNING})
		return warnings
	}

	lines := strings.Split(string(content), "\n")
	for _, hostAddress := range hostAddressList {
		for _, line := range lines {
			hosts := strings.Split(line, " ")
			if slices.Contains(hosts, hostAddress) && slices.Contains(hosts, "localhost") {
				warnMsg := fmt.Sprintf("HostAddress %s is assigned localhost entry in %s."+
					"This will cause segment->coordinator communication failures."+
					"Remove %s from localhost line in %s",
					hostAddress, constants.EtcHostsFilepath, hostAddress, constants.EtcHostsFilepath)

				warnings = append(warnings, &idl.LogMessage{Message: warnMsg, Level: idl.LogLevel_WARNING})
				gplog.Warn(warnMsg)
				break
			}
		}
	}

	return warnings
}

/*
ValidatePortsFn checks if port is already in use
*/
func ValidatePortsFn(portList []string) error {
	gplog.Debug("Started with ValidatePorts")
	usedPortList := make(map[string]bool)

	// Get a list of all interfaces addresses
	ipList, err := utils.GetAllAddresses()
	if err != nil {
		return err
	}

	// now check each port on portlist
	for _, port := range portList {
		for _, ip := range ipList {
			_, err := utils.CheckIfPortFree(ip, port)
			if err != nil {
				usedPortList[port] = true
				gplog.Error("ports already in use: %s, address:%s check if cluster already running. Error:%v", port, ip, err)

			}
		}
	}

	if len(usedPortList) > 0 {
		err := fmt.Errorf("ports already in use: %v, check if cluster already running", usedPortList)
		return err
	}
	return nil
}

/*
GetAllNonEmptyDirFn returns list of all non-empty directories
*/
func GetAllNonEmptyDirFn(dirList []string) ([]string, error) {
	var nonEmptyDir []string
	for _, dir := range dirList {
		isEmpty, err := CheckDirEmpty(dir)
		if err != nil {
			return nonEmptyDir, fmt.Errorf("directory:%s Error checking if empty:%v", dir, err)
		} else if !isEmpty {
			// Directory is not empty
			nonEmptyDir = append(nonEmptyDir, dir)
		}
	}
	return nonEmptyDir, nil
}

/*
CheckDirEmptyFn checks if given directory is empty or not
returns true if directory is empty
*/
func CheckDirEmptyFn(dirPath string) (bool, error) {
	// check if dir exists
	file, err := utils.System.Open(dirPath)
	if OsIsNotExist(err) {
		return true, nil
	}
	if err != nil {
		return false, fmt.Errorf("opening directory: %s, error:%v", dirPath, err)
	}
	defer file.Close()
	_, err = file.Readdirnames(1)
	if err == io.EOF {
		return true, nil
	}
	return false, nil
}

/*
CheckFilePermissionsFn checks if the file has the right permissions.
Verifies if execute permission is available.
Also check if the file is owned by group or user.
*/
func CheckFilePermissionsFn(filePath string) error {
	fileInfo, err := utils.System.Stat(filePath)
	if err != nil {
		return fmt.Errorf("failed for file:%s getting file info:%v", filePath, err)
	}
	// Get current user-id, group-id and checks against initdb file
	err = CheckFileOwnerGroup(filePath, fileInfo)
	if err != nil {
		return err
	}

	// Check if the file has execute permission
	if !CheckExecutable(fileInfo.Mode()) {
		return fmt.Errorf("file %s does not have execute permissions", filePath)
	}
	return nil
}

/*
CheckFileOwnerGroupFn checks if file is owned by user or the group
returns error if not owned by both
*/
func CheckFileOwnerGroupFn(filePath string, fileInfo os.FileInfo) error {
	systemUid := utils.System.Getuid()
	systemGid := utils.System.Getgid()
	// Fetch file info: file owner, group ID
	stat, ok := fileInfo.Sys().(*syscall.Stat_t)
	if !ok {
		return fmt.Errorf("error converting fileinfo:%v", ok)
	}

	if int(stat.Uid) != systemUid && int(stat.Gid) != systemGid {
		fmt.Printf("StatUID:%d, StatGID:%d\nSysUID:%d SysGID:%d\n", stat.Uid, stat.Gid, systemUid, systemGid)
		return fmt.Errorf("file %s is neither owned by the user nor by group", filePath)
	}
	return nil
}

func CheckExecutableFn(FileMode os.FileMode) bool {
	return FileMode&0111 != 0
}

func GetAllAvailableLocalesFn() (string, error) {
	cmd := utils.System.ExecCommand("/usr/bin/locale", "-a")
	availableLocales, err := cmd.Output()

	if err != nil {
		return "", fmt.Errorf("failed to get the available locales on this system: %w", err)
	}
	return string(availableLocales), nil
}

// Simplified version of _nl_normalize_codeset from glibc
// https://sourceware.org/git/?p=glibc.git;a=blob;f=intl/l10nflist.c;h=078a450dfec21faf2d26dc5d0cb02158c1f23229;hb=1305edd42c44fee6f8660734d2dfa4911ec755d6#l294
// Input parameter - string with locale define as [language[_territory][.codeset][@modifier]]
func NormalizeCodesetInLocale(locale string) string {
	localeSplit := strings.Split(locale, ".")
	languageAndTerritory := ""
	codesetAndModifier := []string{}
	codeset := ""
	modifier := ""
	if len(localeSplit) > 0 {
		languageAndTerritory = localeSplit[0]
	}

	if len(localeSplit) > 1 {

		codesetAndModifier = strings.Split(localeSplit[1], "@")
		codeset = codesetAndModifier[0]
	}

	if len(codesetAndModifier) > 1 {
		modifier = codesetAndModifier[1]
	}

	digitPattern := regexp.MustCompile(`^[0-9]+$`)
	if digitPattern.MatchString(codeset) {
		codeset = "iso" + codeset
	} else {
		codeset = strings.Map(func(r rune) rune {
			if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') {
				return r
			}
			return -1
		}, codeset)
		codeset = strings.ToLower(codeset)
	}

	result := fmt.Sprintf("%s%s%s", languageAndTerritory, dotIfNotEmpty(codeset), atIfNotEmpty(modifier))
	return result
}

func dotIfNotEmpty(s string) string {
	return AddIfNotEmpty(s, ".")
}

func atIfNotEmpty(s string) string {
	return AddIfNotEmpty(s, "@")
}

func AddIfNotEmpty(s string, prepend string) string {
	if s != "" {
		return prepend + s
	}
	return s
}

func IsLocaleAvailable(locale_type string, allAvailableLocales string) bool {
	locales := strings.Split(allAvailableLocales, "\n")
	normalizedLocale := NormalizeCodesetInLocale(locale_type)

	for _, v := range locales {
		if locale_type == v || normalizedLocale == v {
			return true
		}
	}
	return false
}

func ValidateLocaleSettingsFn(locale *idl.Locale) error {
	systemLocales, err := GetAllAvailableLocales()
	if err != nil {
		return err
	}
	localeMap := make(map[string]bool)
	localeMap[locale.LcMonetory] = true
	localeMap[locale.LcAll] = true
	localeMap[locale.LcNumeric] = true
	localeMap[locale.LcTime] = true
	localeMap[locale.LcCollate] = true
	localeMap[locale.LcMessages] = true
	localeMap[locale.LcCtype] = true

	for lc := range localeMap {
		// TODO normalize codeset in locale and the check for the availability
		if !IsLocaleAvailable(lc, systemLocales) {
			return fmt.Errorf("locale value '%s' is not a valid locale", lc)
		}
	}

	return nil
}
