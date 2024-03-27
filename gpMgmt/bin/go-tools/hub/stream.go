package hub

import (
	"os/exec"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/idl"
)

// Common interface for all hub side streaming RPC servers
type streamSender interface {
	Send(*idl.HubReply) error
}

type hubStreamer interface {
	StreamLogMsg(msg string, level ...idl.LogLevel)
	StreamStdoutMsg(msg string)
	StreamExecCommand(cmd *exec.Cmd, gpHome string) error
	StreamProgressMsg(label string, total int)
}

type HubStream struct {
	handler streamSender
}

func NewHubStream(s streamSender) HubStream {
	return HubStream{
		handler: s,
	}
}

func (h *HubStream) GetHandler() streamSender {
	return h.handler
}

/*
StreamLogMsg streams a log message from hub to the CLI.
Default log level is set to INFO
*/
func (h *HubStream) StreamLogMsg(msg string, level ...idl.LogLevel) {
	// if log level is not provided, set it to INFO
	if len(level) == 0 {
		level = append(level, idl.LogLevel_INFO)
	}

	logMsg := &idl.LogMessage{
		Message: msg,
		Level:   level[0],
	}

	message := &idl.HubReply{
		Message: &idl.HubReply_LogMsg{
			LogMsg: logMsg,
		},
	}

	err := h.handler.Send(message)
	if err != nil {
		gplog.Error("unable to stream message %q: %s", message, err)
	}
}

/*
StreamStdoutMsg streams a message from hub to CLI
which is directly written to the stdout
*/
func (h *HubStream) StreamStdoutMsg(msg string) {
	message := &idl.HubReply{
		Message: &idl.HubReply_StdoutMsg{
			StdoutMsg: msg,
		},
	}

	err := h.handler.Send(message)
	if err != nil {
		gplog.Error("unable to stream message %q: %s", message, err)
	}
}

/*
StreamExecCommand runs the given exec.Cmd and streams its
stdout and stderr from hub to the CLI
*/
func (h *HubStream) StreamExecCommand(cmd *exec.Cmd, gpHome string) error {
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}

	stderr, err := cmd.StderrPipe()
	if err != nil {
		return err
	}

	gplog.Verbose("Executing command: %s", cmd.String())
	if err := cmd.Start(); err != nil {
		return err
	}

	// stream the stdout continuously
	go func() {
		buf := make([]byte, 1024)
		for {
			n, err := stdout.Read(buf)
			if err != nil {
				break
			}

			output := string(buf[:n])
			h.StreamStdoutMsg(output)
		}
	}()

	// stream the stderr continuously
	go func() {
		buf := make([]byte, 1024)
		for {
			n, err := stderr.Read(buf)
			if err != nil {
				break
			}

			output := string(buf[:n])
			h.StreamStdoutMsg(output)
		}
	}()

	if err := cmd.Wait(); err != nil {
		return err
	}

	return nil
}

/*
StreamProgressMsg is used to stream progress messages from hub to
the CLI. On the CLI side a progress bar will be displayed on the stdout
which will increment with each call to this function. First call to this
will just create/initialise the progress bar and subsequent calls would be
used to increment the progress
*/
func (h *HubStream) StreamProgressMsg(label string, total int) {

	message := &idl.HubReply{
		Message: &idl.HubReply_ProgressMsg{
			ProgressMsg: &idl.ProgressMessage{
				Label: label,
				Total: int32(total),
			},
		},
	}

	err := h.handler.Send(message)
	if err != nil {
		gplog.Error("unable to stream message %q: %s", message, err)
	}
}
