package utils

import (
	//"github.com/jmoiron/sqlx"
	"io/ioutil"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"time"
)

var (
	System = InitializeSystemFunctions()
)

/*
 * SystemFunctions holds function pointers for built-in functions that will need
 * to be mocked out for unit testing.  All built-in functions manipulating the
 * filesystem, shell, or environment should ideally be called through a function
 * pointer in System (the global SystemFunctions variable) instead of being called
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
	Open        func(name string) (*os.File, error)
	OpenFile    func(name string, flag int, perm os.FileMode) (*os.File, error)
	Remove      func(name string) error
	RemoveAll   func(name string) error
	ReadFile    func(filename string) ([]byte, error)
	Stat        func(name string) (os.FileInfo, error)
	//TODO consider other patterns here?
	//first attempt at enabling us to mock exec.Command().Output()
	ExecCmdOutput         func(name string, args ...string) ([]byte, error)
	ExecCmdCombinedOutput func(name string, args ...string) ([]byte, error)
	ExecCommand           func(name string, arg ...string) *exec.Cmd
	FilePathGlob          func(pattern string) ([]string, error)
}

func InitializeSystemFunctions() *SystemFunctions {
	return &SystemFunctions{
		CurrentUser:           user.Current,
		Getenv:                os.Getenv,
		Getpid:                os.Getpid,
		Hostname:              os.Hostname,
		IsNotExist:            os.IsNotExist,
		MkdirAll:              os.MkdirAll,
		Now:                   time.Now,
		Open:                  os.Open,
		OpenFile:              os.OpenFile,
		Remove:                os.Remove,
		RemoveAll:             os.RemoveAll,
		Stat:                  os.Stat,
		ExecCmdOutput:         CommandOutput,
		ExecCmdCombinedOutput: CommandCombinedOutput,
		ExecCommand:           exec.Command,
		FilePathGlob:          filepath.Glob,
		ReadFile:              ioutil.ReadFile,
	}
}

func TryEnv(varname string, defval string) string {
	val := System.Getenv(varname)
	if val == "" {
		return defval
	}
	return val
}

func GetUser() (string, string, error) {
	currentUser, err := System.CurrentUser()
	if err != nil {
		return "", "", err
	}
	return currentUser.Username, currentUser.HomeDir, err
}

func GetHost() (string, error) {
	hostname, err := System.Hostname()
	return hostname, err
}

func CommandOutput(name string, args ...string) ([]byte, error) {
	return exec.Command(name, args...).Output()
}

func CommandCombinedOutput(name string, args ...string) ([]byte, error) {
	return exec.Command(name, args...).CombinedOutput()
}
