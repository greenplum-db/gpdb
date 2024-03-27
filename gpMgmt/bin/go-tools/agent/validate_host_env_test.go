package agent_test

import (
	"context"
	"fmt"
	"net"
	"os"
	"os/user"
	"strings"
	"syscall"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/agent"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func init() {
	exectest.RegisterMains(
		PgVersionCmd, UlimitSuccess, UlimitFail, UlimitTextOutput,
	)
}
func UlimitSuccess() {
	//os.Stdout.WriteString("%d", constants.OsOpenFiles+1)
	os.Stdout.WriteString(fmt.Sprintf("%d", constants.OsOpenFiles+1))
}
func UlimitFail() {

	os.Stdout.WriteString(fmt.Sprintf("%d", constants.OsOpenFiles-1))
}
func UlimitTextOutput() {

	os.Stdout.WriteString(fmt.Sprintf("abc:%d", constants.OsOpenFiles+1))
}
func PgVersionCmd() {
	os.Stdout.WriteString("test-version-1234")
}

func resetAgentFunctions() {
	agent.CheckDirEmpty = agent.CheckDirEmptyFn
	agent.CheckFileOwnerGroup = agent.CheckFileOwnerGroupFn
	agent.CheckExecutable = agent.CheckExecutableFn
	agent.GetAllNonEmptyDir = agent.GetAllNonEmptyDirFn
	agent.CheckFilePermissions = agent.CheckFilePermissionsFn
	agent.ValidateLocaleSettings = agent.ValidateLocaleSettingsFn
	agent.ValidatePorts = agent.ValidatePortsFn
	agent.VerifyPgVersion = agent.ValidatePgVersionFn
	agent.OsIsNotExist = os.IsNotExist
	agent.GetAllAvailableLocales = agent.GetAllAvailableLocalesFn
	utils.ResetSystemFunctions()

}
func TestCheckOpenFilesLimit(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("returns error when fails to execute ulimit command", func(t *testing.T) {
		expReply := idl.LogMessage{
			Message: "error fetching open file limit values:exit status 1",
			Level:   idl.LogLevel_WARNING,
		}
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(exectest.Failure)
		reply := agent.CheckOpenFilesLimit()
		if !strings.Contains(reply[0].Message, expReply.Message) || reply[0].Level != expReply.Level {
			t.Fatalf("Got:%s:thu, Expected:%v", reply[0].Message, expReply)
		}
	})
	t.Run("returns no warning when ulimit returns higher value", func(t *testing.T) {
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(UlimitSuccess)
		reply := agent.CheckOpenFilesLimit()
		if len(reply) > 0 {
			t.Fatalf("Got:%s, Expected no warinings", reply[0].Message)
		}
	})
	t.Run("returns warning when fails to convert ulimit output", func(t *testing.T) {
		tesstStr := "could not convert the ulimit value"
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(UlimitTextOutput)
		reply := agent.CheckOpenFilesLimit()
		if len(reply) < 1 || !strings.Contains(reply[0].Message, tesstStr) {
			t.Fatalf("Got:%s, Expected:%s", reply[0].Message, tesstStr)
		}
	})
}

func TestCheckHostAddressInHostsFile(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns warning when fails  to read hostfile", func(t *testing.T) {
		expectedReply := idl.LogMessage{
			Message: "test error reading file",
			Level:   idl.LogLevel_WARNING,
		}
		defer utils.ResetSystemFunctions()
		utils.System.ReadFile = func(name string) ([]byte, error) {
			return nil, fmt.Errorf(expectedReply.Message)
		}

		addressList := []string{"sdw1", "sdw2"}
		reply := agent.CheckHostAddressInHostsFile(addressList)
		if len(reply) < 1 {
			t.Fatalf("Got empty response Expected reponse:%s", expectedReply.Message)
		}
		if !strings.Contains(reply[0].Message, expectedReply.Message) {
			t.Fatalf("Got message '%s', expected:%s", reply[0].GetMessage(), expectedReply.Message)
		}
		if reply[0].Level != idl.LogLevel_WARNING {
			t.Fatalf("Got message log level %s, expected:%s", reply[0].GetLevel(), expectedReply.Level)
		}
	})
	t.Run("returns warning when host-address is in /etc/hosts file", func(t *testing.T) {
		expectedReply := idl.LogMessage{
			Message: "HostAddress sdw1 is assigned localhost entry in",
			Level:   idl.LogLevel_WARNING,
		}
		defer utils.ResetSystemFunctions()
		utils.System.ReadFile = func(name string) ([]byte, error) {
			return []byte("192.168.1.1 sdw1 localhost"), nil
		}
		addressList := []string{"sdw1", "sdw2"}

		reply := agent.CheckHostAddressInHostsFile(addressList)
		if len(reply) < 1 {
			t.Fatalf("Got empty response Expected reponse:%s", expectedReply.Message)
		}

		if !strings.Contains(reply[0].Message, expectedReply.Message) {
			t.Fatalf("Got message '%s', expected:%s", reply[0].GetMessage(), expectedReply.Message)
		}
		if reply[0].Level != idl.LogLevel_WARNING {
			t.Fatalf("Got message log level %s, expected:%s", reply[0].GetLevel(), expectedReply.Level)
		}
	})
	t.Run("returns no warning when host-address is not in /etc/hosts file", func(t *testing.T) {
		defer utils.ResetSystemFunctions()
		utils.System.ReadFile = func(name string) ([]byte, error) {
			return []byte("192.168.1.1 sdw1 sdw1-1 sdw1.localdomain.com"), nil
		}
		addressList := []string{"sdw1", "sdw2"}

		reply := agent.CheckHostAddressInHostsFile(addressList)
		if len(reply) > 0 {
			t.Fatalf("Got reply:%v Expected empty response", reply)
		}
	})
	t.Run("returns no warning when host-address is not in /etc/hosts file on same line with partial match", func(t *testing.T) {
		defer utils.ResetSystemFunctions()
		utils.System.ReadFile = func(name string) ([]byte, error) {
			return []byte("192.168.1.1 sdw1 sdw1-1 sdw1.localdomain.com\n" + "127.0.0.1 localhost sdw1-1"), nil
		}
		addressList := []string{"sdw1", "sdw2"}

		reply := agent.CheckHostAddressInHostsFile(addressList)
		if len(reply) > 0 {
			t.Fatalf("Got reply:%v Expected empty response", reply)
		}
	})
}

func TestValidatePgVersionFn(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns error when error executing command", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Failure)
		defer utils.ResetSystemFunctions()

		err := agent.ValidatePgVersionFn("expected-version", "gpHome")
		expectedStr := "fetching postgres gp-version: exit status 1"
		if err == nil || !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("expected error:`%s`, got error:`%s`", expectedStr, err)
		}
	})
	t.Run("returns version when no error executing command", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(exectest.Success)
		defer utils.ResetSystemFunctions()

		err := agent.ValidatePgVersionFn("", "gpHome")
		if err != nil {
			t.Fatalf("expected no error, got error:`%s`", err)
		}
	})
	t.Run("returns no error when gp-versions match", func(t *testing.T) {
		expectedVersion := "test-version-1234"
		utils.System.ExecCommand = exectest.NewCommand(PgVersionCmd)
		defer utils.ResetSystemFunctions()

		err := agent.ValidatePgVersionFn(expectedVersion, "gpHome")
		if err != nil {
			t.Fatalf("expected no error, got error:`%s`", err)
		}
	})
	t.Run("returns error when gp-version does not match", func(t *testing.T) {
		expectedVersion := "test-version-Unknown"
		utils.System.ExecCommand = exectest.NewCommand(PgVersionCmd)
		defer utils.ResetSystemFunctions()

		err := agent.ValidatePgVersionFn(expectedVersion, "gpHome")
		expectedStr := "postgres gp-version does not matches with coordinator postgres gp-version."
		if err == nil || !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("expected error: `%s`, got error:`%s`", expectedStr, err)
		}
	})
}

func TestValidatePorts(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns no error when ports are not in use", func(t *testing.T) {
		testPort := []string{"11456", "11457", "11458"}
		err := agent.ValidatePorts(testPort)
		if err != nil {
			t.Fatalf("got %v, expected no error", err)
		}
	})
	t.Run("returns error when ports are in use localhost", func(t *testing.T) {
		testPort := []string{"11456", "11457", "11458"}

		listener, err := net.Listen("tcp", net.JoinHostPort("localhost", testPort[0]))
		if err != nil {
			t.Fatalf("error while listening to test port %v Please change test port", testPort[0])
		}
		defer listener.Close()

		err = agent.ValidatePorts(testPort)

		testStr := "ports already in use:"
		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("returns error when ports are in use 0.0.0.0", func(t *testing.T) {
		testPort := []string{"11456", "11457", "11458"}

		listener, err := net.Listen("tcp", net.JoinHostPort("0.0.0.0", testPort[0]))
		if err != nil {
			t.Fatalf("error while listening to test port %v Please change test port", testPort[0])
		}
		defer listener.Close()

		err = agent.ValidatePorts(testPort)

		testStr := "ports already in use:"
		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("returns error when ports are in use default", func(t *testing.T) {
		testPort := []string{"11456", "11457", "11458"}

		listener, err := net.Listen("tcp", net.JoinHostPort("", testPort[0]))
		if err != nil {
			t.Fatalf("error while listening to test port %v Please change test port", testPort[0])
		}
		defer listener.Close()

		err = agent.ValidatePorts(testPort)

		testStr := "ports already in use:"
		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
}
func TestValidateHostEnv(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("return error when ran as a root", func(t *testing.T) {
		testStr := "is a root user, Can't create cluster under root user"
		defer resetAgentFunctions()
		utils.System.Getuid = func() int {
			return 0
		}

		req := idl.ValidateHostEnvRequest{}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return error when ran as a root and error getting current user", func(t *testing.T) {
		testStr := "test error getting current user"
		defer resetAgentFunctions()
		utils.System.Getuid = func() int {
			return 0
		}
		utils.System.CurrentUser = func() (*user.User, error) {
			return nil, fmt.Errorf(testStr)
		}

		req := idl.ValidateHostEnvRequest{}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return error when gp version is not matching", func(t *testing.T) {
		testStr := "gpversion does not match"
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return fmt.Errorf(testStr)
		}

		req := idl.ValidateHostEnvRequest{Forced: false}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return error when non-empty directories are found and force is not set", func(t *testing.T) {
		testStr := "directory not empty:"
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return nil
		}
		agent.GetAllNonEmptyDir = func(dirList []string) ([]string, error) {
			return []string{"/tmp/1", "/tmp/2"}, nil
		}

		req := idl.ValidateHostEnvRequest{Forced: false}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return error when force is set and error deleting files", func(t *testing.T) {
		testStr := "Error deleting directory"
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return nil
		}
		agent.GetAllNonEmptyDir = func(dirList []string) ([]string, error) {
			return []string{"/tmp/1", "/tmp/2"}, nil
		}
		utils.System.RemoveAll = func(path string) error {
			return fmt.Errorf(testStr)
		}

		req := idl.ValidateHostEnvRequest{Forced: true}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return error when initdb file does not have the correct permissions", func(t *testing.T) {
		testStr := "file does not have enough permission"
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return nil
		}
		agent.GetAllNonEmptyDir = func(dirList []string) ([]string, error) {
			return []string{}, nil
		}
		agent.CheckFilePermissions = func(filePath string) error {
			return fmt.Errorf(testStr)
		}

		req := idl.ValidateHostEnvRequest{Forced: false}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return error when locale validation fails", func(t *testing.T) {
		testStr := "invalid local provided"
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return nil
		}
		agent.GetAllNonEmptyDir = func(dirList []string) ([]string, error) {
			return []string{}, nil
		}
		agent.CheckFilePermissions = func(filePath string) error {
			return nil
		}
		agent.ValidateLocaleSettings = func(locale *idl.Locale) error {
			return fmt.Errorf(testStr)
		}

		req := idl.ValidateHostEnvRequest{Forced: false}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return error when port validation fails", func(t *testing.T) {
		testStr := "ports already in use"
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return nil
		}
		agent.GetAllNonEmptyDir = func(dirList []string) ([]string, error) {
			return []string{}, nil
		}
		agent.CheckFilePermissions = func(filePath string) error {
			return nil
		}
		agent.ValidateLocaleSettings = func(locale *idl.Locale) error {
			return nil
		}
		agent.ValidatePorts = func(portList []string) error {
			return fmt.Errorf(testStr)
		}

		req := idl.ValidateHostEnvRequest{Forced: false}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v, expected:%s", err, testStr)
		}
	})
	t.Run("return success when no errors, no force", func(t *testing.T) {
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return nil
		}
		agent.GetAllNonEmptyDir = func(dirList []string) ([]string, error) {
			return []string{}, nil
		}
		agent.CheckFilePermissions = func(filePath string) error {
			return nil
		}
		agent.ValidateLocaleSettings = func(locale *idl.Locale) error {
			return nil
		}
		agent.ValidatePorts = func(portList []string) error {
			return nil
		}

		req := idl.ValidateHostEnvRequest{Forced: false}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err != nil {
			t.Fatalf("got %v, expected no error", err)
		}
	})
	t.Run("return success when no errors, force is true", func(t *testing.T) {
		defer resetAgentFunctions()
		agent.VerifyPgVersion = func(expectedVersion string, gpHome string) error {
			return nil
		}
		agent.GetAllNonEmptyDir = func(dirList []string) ([]string, error) {
			return []string{}, nil
		}
		agent.CheckFilePermissions = func(filePath string) error {
			return nil
		}
		agent.ValidateLocaleSettings = func(locale *idl.Locale) error {
			return nil
		}
		agent.ValidatePorts = func(portList []string) error {
			return nil
		}

		req := idl.ValidateHostEnvRequest{Forced: true}
		server := agent.New(agent.Config{})
		ctx := context.Background()

		_, err := server.ValidateHostEnv(ctx, &req)

		if err != nil {
			t.Fatalf("got %v, expected no error", err)
		}
	})
}
func TestCheckFileOwnerGroupFn(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns no error when converting fileInfo", func(t *testing.T) {
		tmpFile, err := os.CreateTemp("/tmp/", "")
		if err != nil {
			t.Fatalf("error creating test temp file:%v", err)
		}
		defer os.Remove(tmpFile.Name())
		filePath := tmpFile.Name()
		fileInfo, err := os.Stat(filePath)
		if err != nil {
			t.Fatalf("error stating the file temp file %s:%v", filePath, err)
		}
		err = agent.CheckFileOwnerGroupFn(filePath, fileInfo)
		if err != nil {
			t.Fatalf("got %v expected none", err)
		}
	})
	t.Run("returns no error when only GID does not match", func(t *testing.T) {
		tmpFile, err := os.CreateTemp("/tmp/", "")
		if err != nil {
			t.Fatalf("error creating test temp file:%v", err)
		}
		defer os.Remove(tmpFile.Name())
		defer resetAgentFunctions()
		utils.System.Getgid = func() int {
			return -1
		}
		utils.System.Getuid = func() int {
			stat, err := tmpFile.Stat()
			if err != nil {
				t.Fatalf("error getting test temp file stat:%v", err)
			}
			return int(stat.Sys().(*syscall.Stat_t).Uid)
		}
		filePath := tmpFile.Name()
		fileInfo, err := os.Stat(filePath)
		if err != nil {
			t.Fatalf("error stating the file temp file %s:%v", filePath, err)
		}
		err = agent.CheckFileOwnerGroupFn(filePath, fileInfo)
		if err != nil {
			t.Fatalf("got %v expected no error", err)
		}
	})
	t.Run("returns no error when only UID does not match", func(t *testing.T) {
		tmpFile, err := os.CreateTemp("/tmp/", "")
		if err != nil {
			t.Fatalf("error creating test temp file:%v", err)
		}
		defer os.Remove(tmpFile.Name())
		defer resetAgentFunctions()
		utils.System.Getuid = func() int {
			return -1
		}
		utils.System.Getgid = func() int {
			stat, err := tmpFile.Stat()
			if err != nil {
				t.Fatalf("error getting test temp file stat:%v", err)
			}
			return int(stat.Sys().(*syscall.Stat_t).Gid)
		}
		filePath := tmpFile.Name()
		fileInfo, err := os.Stat(filePath)
		if err != nil {
			t.Fatalf("error stating the file temp file %s:%v", filePath, err)
		}
		err = agent.CheckFileOwnerGroupFn(filePath, fileInfo)
		if err != nil {
			t.Fatalf("got %v expected no error", err)
		}
	})
	t.Run("returns error when uid and gid both do not match", func(t *testing.T) {
		tmpFile, err := os.CreateTemp("/tmp/", "")
		if err != nil {
			t.Fatalf("error creating test temp file:%v", err)
		}
		defer os.Remove(tmpFile.Name())
		defer resetAgentFunctions()
		utils.System.Getgid = func() int {
			return -1
		}
		utils.System.Getuid = func() int {
			return -1
		}
		filePath := tmpFile.Name()
		fileInfo, err := os.Stat(filePath)
		if err != nil {
			t.Fatalf("error stating the file temp file %s:%v", filePath, err)
		}
		testStr := "is neither owned by the user nor by group"
		err = agent.CheckFileOwnerGroupFn(filePath, fileInfo)
		if err == nil || !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got %v expected %s", err, testStr)
		}
	})
}

func TestCheckFilePermissions(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns error when gets error stating the file", func(t *testing.T) {
		filePath := "/tmp/testfile"
		testString := "test error"
		defer resetAgentFunctions()
		utils.System.Stat = func(name string) (os.FileInfo, error) {
			return nil, fmt.Errorf(testString)
		}
		err := agent.CheckFilePermissions(filePath)
		if err == nil || !strings.Contains(err.Error(), testString) {
			t.Fatalf("Got error %v, expected `%s`", err, testString)
		}
	})
	t.Run("returns error when gets error getting the file owner", func(t *testing.T) {
		filePath := "/tmp/testfile"
		testString := "test error"

		defer resetAgentFunctions()
		utils.System.Stat = func(name string) (os.FileInfo, error) {
			return nil, nil
		}

		agent.CheckFileOwnerGroup = func(filePath string, fileInfo os.FileInfo) error {
			return fmt.Errorf(testString)
		}

		err := agent.CheckFilePermissions(filePath)

		if err == nil || !strings.Contains(err.Error(), testString) {
			t.Fatalf("Got error %v, expected `%s`", err, testString)
		}
	})
	t.Run("returns error when gets file isn't executable", func(t *testing.T) {

		tmpFile, err := os.CreateTemp("/tmp/", "")
		if err != nil {
			t.Fatalf("error creating test temp file:%v", err)
		}

		_ = tmpFile.Chmod(0666)
		defer os.Remove(tmpFile.Name())
		defer resetAgentFunctions()

		agent.CheckFileOwnerGroup = func(filePath string, fileInfo os.FileInfo) error {
			return nil
		}

		testString := "does not have execute permissions"
		err = agent.CheckFilePermissions(tmpFile.Name())
		if err == nil || !strings.Contains(err.Error(), testString) {
			t.Fatalf("Got error %v, expected `%s`", err, testString)
		}

	})
	t.Run("returns no error when no  error checking file permissions", func(t *testing.T) {

		tmpFile, err := os.CreateTemp("/tmp", "")
		if err != nil {
			t.Fatalf("error creating test temp file:%v", err)
		}
		defer os.Remove(tmpFile.Name())
		defer resetAgentFunctions()
		agent.CheckFileOwnerGroup = func(filePath string, fileInfo os.FileInfo) error {
			return nil
		}
		agent.CheckExecutable = func(FileMode os.FileMode) bool {
			return true
		}
		err = agent.CheckFilePermissions(tmpFile.Name())
		if err != nil {
			t.Fatalf("Got error %v, expected no error", err)
		}
	})
}
func TestCheckDirEmptyFn(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("check if returns empty dir and no error when directory is empty", func(t *testing.T) {
		dirPath := "/tmp/test"
		defer resetAgentFunctions()
		agent.OsIsNotExist = func(err error) bool {
			return true
		}
		utils.System.Open = func(name string) (*os.File, error) {
			return nil, fmt.Errorf("test error")
		}
		isEmpty, err := agent.CheckDirEmptyFn(dirPath)
		if isEmpty != true {
			t.Fatalf("got directory empty:%v expected directory empty:  true", isEmpty)
		}
		if err != nil {
			t.Fatalf("got error:%v, expected no error", err)
		}
	})
	t.Run("check if returns error if get checking if directory exists", func(t *testing.T) {
		dirPath := "/tmp/test"
		expectedErr := "error opening file"
		defer resetAgentFunctions()
		utils.System.Open = func(name string) (*os.File, error) {
			return nil, fmt.Errorf(expectedErr)
		}
		agent.OsIsNotExist = func(err error) bool {
			return false
		}
		_, err := agent.CheckDirEmptyFn(dirPath)
		if err == nil || !strings.Contains(err.Error(), expectedErr) {
			t.Fatalf("got error:%v, expected '%s' error", err, expectedErr)
		}
	})

}
func TestCheckEmptyDir(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("CheckemptyDir returns correct value if directory exists", func(t *testing.T) {
		testDir := "/tmp/test1"
		err := os.Mkdir(testDir, 0766)
		defer os.Remove(testDir)
		if err != nil {
			t.Fatalf("error creating test dir:%s. Error:%v", testDir, err)
		}
		isEmpty, err := agent.CheckDirEmpty(testDir)
		if err != nil {
			t.Fatalf("Got: %v, expected no error", err)
		}
		if isEmpty != true {
			t.Fatalf("expected directory empty, but its not")
		}
	})
	t.Run("CheckemptyDir returns correct value if directory does not exists", func(t *testing.T) {
		testDir := "/tmp/test1"
		isEmpty, err := agent.CheckDirEmpty(testDir)
		if err != nil {
			t.Fatalf("Got: %v, expecte no error", err)
		}
		if isEmpty != true {
			t.Fatalf("expected directory empty, but its not")
		}
	})
	t.Run("CheckemptyDir returns correct value if directory exists and non-empty", func(t *testing.T) {
		testDir := "/tmp/test1"
		testFile := "/tmp/test1/testfile"
		err := os.Mkdir(testDir, 0766)
		defer os.Remove(testDir)
		if err != nil {
			t.Fatalf("error creating test dir:%s. Error:%v", testDir, err)
		}

		file, err := os.Create(testFile)
		if err != nil {
			t.Fatalf("error creating test file:%s. Error:%v", testFile, err)
		}
		file.Close()
		defer os.Remove(testFile)
		isEmpty, err := agent.CheckDirEmpty(testDir)
		if err != nil {
			t.Fatalf("Got: %v, expected no error", err)
		}
		if isEmpty != false {
			t.Fatalf("expected directory non-empty, but returned empty")
		}
	})
}

func TestGetAllNonEmptyDir(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("function returns list of all non empty directories", func(t *testing.T) {
		var dirList []string
		testString := "/tmp/1"
		expectedStr := "/tmp/2"
		dirList = append(dirList, testString)
		dirList = append(dirList, expectedStr)
		defer resetAgentFunctions()
		agent.CheckDirEmpty = func(dirPath string) (bool, error) {
			if dirPath == testString {
				return true, nil
			}
			return false, nil
		}
		emptyList, err := agent.GetAllNonEmptyDir(dirList)
		if err != nil {
			t.Fatalf("got error:%v, want no error", err)
		}
		if len(emptyList) != 1 || emptyList[0] != expectedStr {
			t.Fatalf("got %q, want %q", emptyList, expectedStr)
		}
	})
	t.Run("function returns error when got error checking directory empty", func(t *testing.T) {
		strErr := "test Error checking directory"
		dirList := []string{"/tmp/1", "/tmp/2"}

		defer resetAgentFunctions()
		agent.CheckDirEmpty = func(dirPath string) (bool, error) {
			if dirPath == dirList[0] {
				return true, nil
			}
			return false, fmt.Errorf(strErr)
		}
		_, err := agent.GetAllNonEmptyDir(dirList)
		if err == nil || !strings.Contains(err.Error(), strErr) {
			t.Fatalf("got error:%v, want error:%s", err, strErr)
		}

	})
}

func TestNormalizeCodesetInLocale(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("function returns the locale with normalized codeset", func(t *testing.T) {
		expectedLocale := "en_US.utf8"
		normalizedCodesetLocale := agent.NormalizeCodesetInLocale("en_US.UTF-8")
		if normalizedCodesetLocale != expectedLocale {
			t.Fatalf("got locale %s expected %s", normalizedCodesetLocale, expectedLocale)
		}
	})
	t.Run("function returns the locale with normalized codeset when codeset contains only digits", func(t *testing.T) {
		expectedLocale := "en_US.iso1234"
		normalizedCodesetLocale := agent.NormalizeCodesetInLocale("en_US.1234")
		if normalizedCodesetLocale != expectedLocale {
			t.Fatalf("got locale %s expected %s", normalizedCodesetLocale, expectedLocale)
		}
	})
	t.Run("function returns the locale with normalized codeset with modifier", func(t *testing.T) {
		expectedLocale := "en_US.iso1234@tlo"
		normalizedCodesetLocale := agent.NormalizeCodesetInLocale("en_US.1234@tlo")
		if normalizedCodesetLocale != expectedLocale {
			t.Fatalf("got locale %s expected %s", normalizedCodesetLocale, expectedLocale)
		}
	})
	t.Run("function returns the locale when empty locale is passed", func(t *testing.T) {
		expectedLocale := ""
		normalizedCodesetLocale := agent.NormalizeCodesetInLocale("")
		if normalizedCodesetLocale != expectedLocale {
			t.Fatalf("got locale %s expected %s", normalizedCodesetLocale, expectedLocale)
		}
	})
	t.Run("function returns the locale when only locale is passed", func(t *testing.T) {
		expectedLocale := "UTF8"
		normalizedCodesetLocale := agent.NormalizeCodesetInLocale("UTF8")
		if normalizedCodesetLocale != expectedLocale {
			t.Fatalf("got locale %s expected %s", normalizedCodesetLocale, expectedLocale)
		}
	})
}

func TestIsLocaleAvailable(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("function returns true if locale is available on the system", func(t *testing.T) {
		isAvailable := agent.IsLocaleAvailable("en_US.UTF-8", "en_US.UTF-8\nfi_FI.ISO8859-15\nko_KR.CP949\nhy_AM.UTF-8")
		if !isAvailable {
			t.Fatalf("Returned false even when locale is available")
		}
	})
	t.Run("function returns false if locale is available on the system", func(t *testing.T) {
		isAvailable := agent.IsLocaleAvailable("en_US.UTF-8", "fi_FI.ISO8859-15\nko_KR.CP949\nhy_AM.UTF-8")
		if isAvailable {
			t.Fatalf("Returned true even when locale is not available")
		}
	})
}

func TestValidateLocaleSettingsFn(t *testing.T) {
	testhelper.SetupTestLogger()
	defer resetAgentFunctions()
	locale := &idl.Locale{
		LcAll:      "en_US.UTF-8",
		LcCtype:    "en_US.UTF-8",
		LcTime:     "en_US.UTF-8",
		LcNumeric:  "en_US.UTF-8",
		LcMonetory: "en_US.UTF-8",
		LcMessages: "en_US.UTF-8",
		LcCollate:  "en_US.UTF-8",
	}
	t.Run("returns error when fails to get locale", func(t *testing.T) {
		testStr := "test-error"
		agent.GetAllAvailableLocales = func() (string, error) {
			return "", fmt.Errorf(testStr)
		}
		err := agent.ValidateLocaleSettingsFn(locale)
		if err != nil && !strings.Contains(err.Error(), testStr) {
			t.Fatalf("got unexpected error %s", err)
		}
	})
	t.Run("function does not return any error if all of the locale are available on the system", func(t *testing.T) {
		agent.GetAllAvailableLocales = func() (string, error) {
			return "en_US.UTF-8\nfi_FI.ISO8859-15\nko_KR.CP949\nhy_AM.UTF-8", nil
		}
		err := agent.ValidateLocaleSettingsFn(locale)
		if err != nil {
			t.Fatalf("got unexpected error %s", err)
		}
	})
	t.Run("function returns error if any of the locale is not available on the system", func(t *testing.T) {
		expectedError := "locale value 'en_US.1234' is not a valid locale"
		locale.LcCtype = "en_US.1234"
		agent.GetAllAvailableLocales = func() (string, error) {
			return "en_US.UTF-8\nfi_FI.ISO8859-15\nko_KR.CP949\nhy_AM.UTF-8", nil
		}
		err := agent.ValidateLocaleSettingsFn(locale)
		if err == nil || !strings.Contains(err.Error(), expectedError) {
			t.Fatalf("got %s, expected: %s", err, expectedError)
		}
	})
}

func TestGetAllAvailableLocalesFn(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns error upon failure to get locale", func(t *testing.T) {
		exectest.NewCommand(exectest.Failure)

	})
}
