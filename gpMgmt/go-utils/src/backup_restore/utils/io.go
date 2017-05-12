package utils

/*
 * This file contains structs and functions related to interacting with files
 * and directories, both locally and remotely over SSH.
 */

import (
	"io"
	"os"
	"os/user"
	"strings"
)

var (
	/*
	 * The following two variables are used to construct the dump path for all
	 * backup files for the duration of the dump and must be set in DoSetup()
	 * function in backup.go.  They're used in the data dump COPY ... TO SEGMENT;
	 * query, and so can use <SEG_DATA_DIR> and <SEGID> instead of explicitly
	 * constructing paths for each segment.
	 */
	BaseDumpDir    = "<SEG_DATA_DIR>"
	DumpPathFmtStr = ""

	/*
	 * The following two maps map a segment's content id to its host and segment
	 * data directory, respectively.  They're set in the CreateDumpDirectories
	 * function for use throughout the rest of the dump.
	 */
	SegHostMap map[int]string
	SegDirMap  map[int]string

	/*
	 * The following variables, and any others named "FP[package][function name]",
	 * are function pointers used to enable unit testing.
	 */
	FPDirectoryMustExist = DirectoryMustExist
	FPGetUserAndHostInfo = GetUserAndHostInfo
	FPMustOpenFile       = MustOpenFile
	FPOsIsNotExist       = os.IsNotExist
	FPOsMkdir            = os.Mkdir
	FPOsMkdirAll         = os.MkdirAll
	FPOsCreate           = os.Create
	FPOsStat             = os.Stat
)

/*
 * Generic file/directory manipulation functions
 */

func DirectoryMustExist(dirname string) {
	_, statErr := FPOsStat(dirname)
	if statErr != nil {
		logger.Fatal("Cannot stat directory %s: %s", dirname, statErr)
	}
}

func MustOpenFile(filename string) io.Writer {
	logFileHandle, err := FPOsCreate(filename)
	if err != nil {
		logger.Fatal("Unable to create or open file %s: %s", filename, err)
	}
	return logFileHandle
}

func GetUserAndHostInfo() (string, string, string) {
	currentUser, _ := user.Current()
	userName := currentUser.Username
	userDir := currentUser.HomeDir
	hostname, _ := os.Hostname()
	return userName, userDir, hostname
}

/*
 * Backup-specific file/directory manipulation functions
 */

// TODO: Handle multi-node clusters
func CreateDumpDirs(segConfig []QuerySegConfig) {
	SegHostMap = make(map[int]string, 0)
	SegDirMap = make(map[int]string, 0)
	for _, seg := range segConfig {
		dumpPath := strings.Replace(DumpPathFmtStr, "<SEG_DATA_DIR>", seg.DataDir, -1)
		logger.Verbose("Creating directory %s", dumpPath)
		err := FPOsMkdirAll(dumpPath, 0700)
		if err != nil {
			logger.Fatal("Cannot create directory %s on host %s: %s", seg.DataDir, seg.Hostname, err.Error())
		}
		CheckError(err)
		SegHostMap[seg.Content] = seg.Hostname
		SegDirMap[seg.Content] = dumpPath
	}
}

/*
 * TODO: Move the segment configuration code into a shared directory after the master merge
 */
type QuerySegConfig struct {
	Content    int
	Hostname   string
	DataDir string
}

func GetSegmentConfiguration(connection *DBConn) []QuerySegConfig {
	query := `SELECT
	content,
	hostname,
	fselocation as datadir
FROM pg_catalog.gp_segment_configuration
JOIN pg_catalog.pg_filespace_entry
	ON (dbid = fsedbid)
WHERE role = 'p'
ORDER BY content;`

	results := make([]QuerySegConfig, 0)
	err := connection.Select(&results, query)
	CheckError(err)
	return results
}
