package utils

import (
	"io"
	"os"
	"os/user"
)

var (
	FPGetUserAndHostInfo = GetUserAndHostInfo
	FPOsIsNotExist       = os.IsNotExist
	FPOsMkdir            = os.Mkdir
	FPOsCreate           = os.Create
	FPOsStat             = os.Stat

	FPDirectoryMustExist = DirectoryMustExist
	FPMustOpenFile       = MustOpenFile
)

func DirectoryMustExist(dirname string) {
	_, statErr := FPOsStat(dirname)
	if statErr != nil {
		logger.Fatal("Cannot use directory %s as log directory: %s", dirname, statErr)
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
