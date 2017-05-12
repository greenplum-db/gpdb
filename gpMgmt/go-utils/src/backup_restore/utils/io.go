package utils

import (
	"io"
	"os"
	"os/user"
	"strings"
)

var (
	BaseDumpDir    = "<SEG_DATA_DIR>"
	DumpPathFmtStr = "" // Format string to be used in COPY ... TO SEGMENT; must be set in setup function
	SegHostMap map[int]string
	SegDirMap  map[int]string

	FPGetUserAndHostInfo = GetUserAndHostInfo
	FPOsIsNotExist       = os.IsNotExist
	FPOsMkdir            = os.Mkdir
	FPOsMkdirAll         = os.MkdirAll
	FPOsCreate           = os.Create
	FPOsStat             = os.Stat

	FPDirectoryMustExist = DirectoryMustExist
	FPMustOpenFile       = MustOpenFile
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
