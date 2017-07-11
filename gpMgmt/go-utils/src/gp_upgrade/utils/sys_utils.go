package utils

import (
	//"github.com/jmoiron/sqlx"
	"os"
	"os/user"
	"time"
)

var (
	system = InitializeSystemFunctions()
)

/*
 * SystemFunctions holds function pointers for built-in functions that will need
 * to be mocked out for unit testing.  All built-in functions manipulating the
 * filesystem, shell, or environment should ideally be called through a function
 * pointer in system (the global SystemFunctions variable) instead of being called
 * directly.
 */

type SystemFunctions struct {
	CurrentUser func() (*user.User, error)
	Getenv      func(key string) string
	Getpid      func() int
	Hostname    func() (string, error)
	IsNotExist  func(err error) bool
	MkdirAll    func(path string, perm os.FileMode) error
	Now         func() time.Time
	OpenFile    func(name string, flag int, perm os.FileMode) (*os.File, error)
	Stat        func(name string) (os.FileInfo, error)
}

func InitializeSystemFunctions() *SystemFunctions {
	return &SystemFunctions{
		CurrentUser: user.Current,
		Getenv:      os.Getenv,
		Getpid:      os.Getpid,
		Hostname:    os.Hostname,
		IsNotExist:  os.IsNotExist,
		MkdirAll:    os.MkdirAll,
		Now:         time.Now,
		OpenFile:    os.OpenFile,
		Stat:        os.Stat,
	}
}

func TryEnv(varname string, defval string) string {
	val := system.Getenv(varname)
	if val == "" {
		return defval
	}
	return val
}

func GetUser() (string, string, error) {
	currentUser, err := system.CurrentUser()
	if err != nil {
		return "", "", err
	}
	return currentUser.Username, currentUser.HomeDir, err
}

func GetHost() (string, error) {
	hostname, err := system.Hostname()
	return hostname, err
}
