package utils

import (
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"text/tabwriter"

	"github.com/greenplum-db/gpdb/gp/constants"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
)

var (
	platform             Platform
	execCommand          = exec.Command
	writeServiceFileFunc = WriteServiceFile
	GpsyncCommand        = exec.Command
	LoadServiceCommand   = exec.Command
	UnloadServiceCommand = exec.Command
)

type GpPlatform struct {
	OS         string
	ServiceCmd string // Binary for managing services
	UserArg    string // systemd always needs a "--user" flag passed, launchctl does not
	ServiceExt string // Extension for service files
	StatusArg  string // Argument passed to ServiceCmd to get status of a service
}

func NewPlatform(os string) (Platform, error) {
	switch os {
	case constants.PlatformDarwin:
		return GpPlatform{
			OS:         constants.PlatformDarwin,
			ServiceCmd: "launchctl",
			UserArg:    "",
			ServiceExt: "plist",
			StatusArg:  "list",
		}, nil

	case constants.PlatformLinux:
		return GpPlatform{
			OS:         constants.PlatformLinux,
			ServiceCmd: "systemctl",
			UserArg:    "--user",
			ServiceExt: "service",
			StatusArg:  "show",
		}, nil

	default:
		return nil, errors.New("unsupported OS")
	}
}

type Platform interface {
	CreateServiceDir(hostnames []string, serviceDir string, gphome string) error
	GenerateServiceFileContents(process string, gphome string, serviceName string) string
	GetDefaultServiceDir() string
	ReloadHubService(servicePath string) error
	ReloadAgentService(gphome string, hostList []string, servicePath string) error
	CreateAndInstallHubServiceFile(gphome string, serviceDir string, serviceName string) error
	CreateAndInstallAgentServiceFile(hostnames []string, gphome string, serviceDir string, serviceName string) error
	GetStartHubCommand(serviceName string) *exec.Cmd
	GetStartAgentCommandString(serviceName string) []string
	GetServiceStatusMessage(serviceName string) (string, error)
	ParseServiceStatusMessage(message string) idl.ServiceStatus
	DisplayServiceStatus(outfile io.Writer, serviceName string, statuses []*idl.ServiceStatus, skipHeader bool)
	EnableUserLingering(hostnames []string, gphome string, serviceUser string) error
}

func GetPlatform() Platform {
	var err error

	if platform == nil {
		platform, err = NewPlatform(runtime.GOOS)
		if err != nil {
			fmt.Printf("error: %s\n", err)
			os.Exit(1)
		}
	}

	return platform
}

func (p GpPlatform) CreateServiceDir(hostnames []string, serviceDir string, gphome string) error {
	hostList := make([]string, 0)
	for _, host := range hostnames {
		hostList = append(hostList, "-h", host)
	}

	// Create service directory if it does not exist
	args := append(hostList, "mkdir", "-p", serviceDir)
	utility := filepath.Join(gphome, "bin", constants.GpSSH)
	err := execCommand(utility, args...).Run()
	if err != nil {
		return fmt.Errorf("could not create service directory %s on hosts: %w", serviceDir, err)
	}

	gplog.Info("Created service file directory %s on all hosts", serviceDir)
	return nil
}

func WriteServiceFile(filename string, contents string) error {
	handle, err := os.OpenFile(filename, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
	if err != nil {
		return fmt.Errorf("could not create service file %s: %w\n", filename, err)
	}
	defer handle.Close()

	_, err = handle.WriteString(contents)
	if err != nil {
		return fmt.Errorf("could not write to service file %s: %w\n", filename, err)
	}

	return nil
}

func (p GpPlatform) GenerateServiceFileContents(process string, gphome string, serviceName string) string {
	if p.OS == constants.PlatformDarwin {
		return GenerateDarwinServiceFileContents(process, gphome, serviceName)
	}

	return GenerateLinuxServiceFileContents(process, gphome, serviceName)
}

func GenerateDarwinServiceFileContents(process string, gphome string, serviceName string) string {
	template := `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>%[3]s_%[1]s</string>
    <key>ProgramArguments</key>
    <array>
        <string>%[2]s/bin/gp</string>
        <string>%[1]s</string>
    </array>
    <key>StandardOutPath</key>
    <string>/tmp/grpc_%[1]s.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/grpc_%[1]s.log</string>
    <key>EnvironmentVariables</key>
    <dict>
        <key>PATH</key>
        <string>%[4]s</string>
        <key>GPHOME</key>
        <string>%[2]s</string>
    </dict>
</dict>
</plist>
`
	return fmt.Sprintf(template, process, gphome, serviceName, os.Getenv("PATH"))
}

func GenerateLinuxServiceFileContents(process string, gphome string, serviceName string) string {
	template := `[Unit]
Description=Greenplum Database management utility %[1]s

[Service]
Type=simple
Environment=GPHOME=%[2]s
ExecStart=%[2]s/bin/gp %[1]s
Restart=on-failure

[Install]
Alias=%[3]s_%[1]s.service
WantedBy=default.target
`
	return fmt.Sprintf(template, process, gphome, serviceName)
}

func (p GpPlatform) GetDefaultServiceDir() string {
	if p.OS == constants.PlatformDarwin {
		return "/Users/%s/Library/LaunchAgents"
	}

	return "/home/%s/.config/systemd/user"
}

func (p GpPlatform) CreateAndInstallHubServiceFile(gphome string, serviceDir string, serviceName string) error {
	hubServiceContents := p.GenerateServiceFileContents("hub", gphome, serviceName)
	hubServiceFilePath := filepath.Join(serviceDir, fmt.Sprintf("%s_hub.%s", serviceName, p.ServiceExt))
	err := writeServiceFileFunc(hubServiceFilePath, hubServiceContents)
	if err != nil {
		return err
	}

	err = p.ReloadHubService(hubServiceFilePath)
	if err != nil {
		return err
	}

	gplog.Info("Wrote hub service file to %s on coordinator host", hubServiceFilePath)
	return nil
}

func (p GpPlatform) ReloadHubService(servicePath string) error {
	if p.OS == constants.PlatformDarwin {
		// launchctl does not have a single reload command. Hence unload and load the file to update the configuration.
		err := UnloadServiceCommand(p.ServiceCmd, "unload", servicePath).Run()
		if err != nil {
			return fmt.Errorf("could not unload hub service file %s: %w", servicePath, err)
		}

		err = LoadServiceCommand(p.ServiceCmd, "load", servicePath).Run()
		if err != nil {
			return fmt.Errorf("could not load hub service file %s: %w", servicePath, err)
		}

		return nil
	}

	err := execCommand(p.ServiceCmd, p.UserArg, "daemon-reload").Run()
	if err != nil {
		return fmt.Errorf("could not reload hub service file %s: %w", servicePath, err)
	}

	return nil
}

func (p GpPlatform) ReloadAgentService(gphome string, hostList []string, servicePath string) error {
	args := append(hostList, p.ServiceCmd)

	if p.OS == constants.PlatformDarwin { // launchctl reloads a specific service, not all of them
		// launchctl does not have a single reload command. Hence unload and load the file to update the configuration.
		err := UnloadServiceCommand(fmt.Sprintf("%s/bin/gpssh", gphome), append(args, "unload", servicePath)...).Run()
		if err != nil {
			return fmt.Errorf("could not unload agent service file %s on segment hosts: %w", servicePath, err)
		}

		err = LoadServiceCommand(fmt.Sprintf("%s/bin/gpssh", gphome), append(args, "load", servicePath)...).Run()
		if err != nil {
			return fmt.Errorf("could not load agent service file %s on segment hosts: %w", servicePath, err)
		}

		return nil
	}

	err := execCommand(fmt.Sprintf("%s/bin/gpssh", gphome), append(args, p.UserArg, "daemon-reload")...).Run()
	if err != nil {
		return fmt.Errorf("could not reload agent service file %s on segment hosts: %w", servicePath, err)
	}

	return nil
}

func (p GpPlatform) CreateAndInstallAgentServiceFile(hostnames []string, gphome string, serviceDir string, serviceName string) error {
	agentServiceContents := p.GenerateServiceFileContents("agent", gphome, serviceName)
	localAgentServiceFilePath := fmt.Sprintf("./%s_agent.%s", serviceName, p.ServiceExt)
	err := writeServiceFileFunc(localAgentServiceFilePath, agentServiceContents)
	if err != nil {
		return err
	}
	defer os.Remove(localAgentServiceFilePath)

	remoteAgentServiceFilePath := fmt.Sprintf("%s/%s_agent.%s", serviceDir, serviceName, p.ServiceExt)
	hostList := make([]string, 0)
	for _, host := range hostnames {
		hostList = append(hostList, "-h", host)
	}

	// Copy the file to segment host service directories
	args := append(hostList, localAgentServiceFilePath, fmt.Sprintf("=:%s", remoteAgentServiceFilePath))
	err = GpsyncCommand(fmt.Sprintf("%s/bin/gpsync", gphome), args...).Run()
	if err != nil {
		return fmt.Errorf("could not copy agent service files to segment hosts: %w", err)
	}

	err = p.ReloadAgentService(gphome, hostList, remoteAgentServiceFilePath)
	if err != nil {
		return err
	}

	gplog.Info("Wrote agent service file to %s on segment hosts", remoteAgentServiceFilePath)
	return nil
}

func (p GpPlatform) GetStartHubCommand(serviceName string) *exec.Cmd {
	args := []string{p.UserArg, "start", fmt.Sprintf("%s_hub", serviceName)}

	if p.OS == constants.PlatformDarwin { // empty strings are also treated as arguments
		args = args[1:]
	}

	return exec.Command(p.ServiceCmd, args...)
}

func (p GpPlatform) GetStartAgentCommandString(serviceName string) []string {
	return []string{p.ServiceCmd, p.UserArg, "start", fmt.Sprintf("%s_agent", serviceName)}
}

func (p GpPlatform) GetServiceStatusMessage(serviceName string) (string, error) {
	args := []string{p.UserArg, p.StatusArg, serviceName}

	if p.OS == constants.PlatformDarwin { // empty strings are also treated as arguments
		args = args[1:]
	}

	output, err := execCommand(p.ServiceCmd, args...).Output()
	if err != nil {
		if err.Error() != "exit status 3" { // 3 = service is stopped
			return "", err
		}
	}

	return string(output), nil
}

/*
Example service status output

Linux:
ExecMainStartTimestamp=Sun 2023-08-20 14:43:35 UTC
ExecMainPID=83008
ExecMainCode=0
ExecMainStatus=0

Darwin:
{
	"PID" = 19909;
	"Program" = "/usr/local/gpdb/bin/gp";
	"ProgramArguments" = (
		"/usr/local/gpdb/bin/gp";
		"hub";
	);
};
*/
func (p GpPlatform) ParseServiceStatusMessage(message string) idl.ServiceStatus {
	var status, uptime string
	var pid int

	lines := strings.Split(message, "\n")
	for _, line := range lines {
		line = strings.TrimSuffix(strings.TrimSpace(line), ";")
		switch {
		case strings.HasPrefix(line, "\"PID\" ="): // for darwin
			results := strings.Split(line, " = ")
			pid, _ = strconv.Atoi(results[1])

		case strings.HasPrefix(line, "ExecMainPID="): // for linux
			results := strings.Split(line, "=")
			pid, _ = strconv.Atoi(results[1])

		case strings.HasPrefix(line, "ExecMainStartTimestamp="): // for linux
			result := strings.Split(line, "=")
			uptime = result[1]
		}
	}

	if pid > 0 {
		status = "running"
	} else {
		status = "not running"
	}

	return idl.ServiceStatus{Status: status, Uptime: uptime, Pid: uint32(pid)}
}

func (p GpPlatform) DisplayServiceStatus(outfile io.Writer, serviceName string, statuses []*idl.ServiceStatus, skipHeader bool) {
	w := new(tabwriter.Writer)
	w.Init(outfile, 0, 8, 2, '\t', 0)

	if !skipHeader {
		fmt.Fprintln(w, "ROLE\tHOST\tSTATUS\tPID\tUPTIME")
	}

	for _, s := range statuses {
		fmt.Fprintf(w, "%s\t%s\t%s\t%d\t%s\n", serviceName, s.Host, s.Status, s.Pid, s.Uptime)
	}
	w.Flush()
}

// Allow systemd services to run on startup and be started/stopped without root access
// This is a no-op on Mac, as launchctl lacks the concept of user lingering
func (p GpPlatform) EnableUserLingering(hostnames []string, gphome string, serviceUser string) error {
	if p.OS != "linux" {
		return nil
	}

	hostList := make([]string, 0)
	for _, host := range hostnames {
		hostList = append(hostList, "-h", host)
	}

	remoteCmd := append(hostList, "loginctl enable-linger ", serviceUser)
	err := execCommand(fmt.Sprintf("%s/bin/gpssh", gphome), remoteCmd...).Run()
	if err != nil {
		return fmt.Errorf("could not enable user lingering: %w", err)
	}

	return nil
}

func SetExecCommand(command exectest.Command) {
	execCommand = command
}

func ResetExecCommand() {
	execCommand = exec.Command
}

func SetWriteServiceFileFunc(writeFunc func(filename string, contents string) error) {
	writeServiceFileFunc = writeFunc
}

func ResetWriteServiceFileFunc() {
	writeServiceFileFunc = WriteServiceFile
}
