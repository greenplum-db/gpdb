package hub_test

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"reflect"
	"regexp"
	"testing"
	"time"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
)

func init() {
	exectest.RegisterMains(DummyCommand)
}

func TestHubStream(t *testing.T) {
	_, _, logfile := testhelper.SetupTestLogger()

	t.Run("succesfully streams log messages", func(t *testing.T) {
		stream, res := testutils.NewMockStream()

		info := "info log"
		warning := "warning log"
		stream.StreamLogMsg(info)
		stream.StreamLogMsg(warning, idl.LogLevel_WARNING)

		expected := []*idl.HubReply{
			{
				Message: &idl.HubReply_LogMsg{
					LogMsg: &idl.LogMessage{
						Message: info,
						Level:   idl.LogLevel_INFO,
					},
				},
			},
			{
				Message: &idl.HubReply_LogMsg{
					LogMsg: &idl.LogMessage{
						Message: warning,
						Level:   idl.LogLevel_WARNING,
					},
				},
			},
		}

		if !reflect.DeepEqual(res.GetBuffer(), expected) {
			t.Fatalf("got %+v, want %+v", res.GetBuffer(), expected)
		}
	})

	t.Run("succesfully streams stdout messages", func(t *testing.T) {
		stream, res := testutils.NewMockStream()

		msg1 := "message 1"
		msg2 := "message 2"
		stream.StreamStdoutMsg(msg1)
		stream.StreamStdoutMsg(msg2)

		expected := []*idl.HubReply{
			{
				Message: &idl.HubReply_StdoutMsg{
					StdoutMsg: msg1,
				},
			},
			{
				Message: &idl.HubReply_StdoutMsg{
					StdoutMsg: msg2,
				},
			},
		}

		if !reflect.DeepEqual(res.GetBuffer(), expected) {
			t.Fatalf("got %+v, want %+v", res.GetBuffer(), expected)
		}
	})

	t.Run("succesfully streams progress messages", func(t *testing.T) {
		stream, res := testutils.NewMockStream()

		label := "label"
		total := 2
		stream.StreamProgressMsg(label, total)
		stream.StreamProgressMsg(label, total)

		expected := []*idl.HubReply{
			{
				Message: &idl.HubReply_ProgressMsg{
					ProgressMsg: &idl.ProgressMessage{
						Label: label,
						Total: int32(total),
					},
				},
			},
			{
				Message: &idl.HubReply_ProgressMsg{
					ProgressMsg: &idl.ProgressMessage{
						Label: label,
						Total: int32(total),
					},
				},
			},
		}

		if !reflect.DeepEqual(res.GetBuffer(), expected) {
			t.Fatalf("got %+v, want %+v", res.GetBuffer(), expected)
		}
	})

	t.Run("succesfully streams exec commands", func(t *testing.T) {
		stream, res := testutils.NewMockStream()

		dummyCmd := exectest.NewCommand(DummyCommand)
		err := stream.StreamExecCommand(dummyCmd(""), "gpHome")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}

		expected := []*idl.HubReply{
			{
				Message: &idl.HubReply_StdoutMsg{
					StdoutMsg: "stdout",
				},
			},
			{
				Message: &idl.HubReply_StdoutMsg{
					StdoutMsg: "stderr",
				},
			},
		}

		if !reflect.DeepEqual(res.GetBuffer(), expected) {
			t.Fatalf("got %+v, want %+v", res.GetBuffer(), expected)
		}
	})

	t.Run("returns error when not able to stream exec command", func(t *testing.T) {
		stream, res := testutils.NewMockStream()

		dummyCmd := exectest.NewCommand(exectest.Failure)
		err := stream.StreamExecCommand(dummyCmd(""), "gpHome")

		var expectedErr *exec.ExitError
		if !errors.As(err, &expectedErr) {
			t.Errorf("got %T, want %T", err, expectedErr)
		}

		if len(res.GetBuffer()) != 0 {
			t.Fatalf("got %d, want the buffer to be empty", len(res.GetBuffer()))
		}
	})

	t.Run("succesfully logs error if not able to send a stream", func(t *testing.T) {
		expectedErr := errors.New("error")
		stream, res := testutils.NewMockStream(expectedErr)

		logMsg := &idl.HubReply{
			Message: &idl.HubReply_LogMsg{
				LogMsg: &idl.LogMessage{
					Message: "log message",
					Level:   idl.LogLevel_INFO,
				},
			},
		}
		stdoutMsg := &idl.HubReply{
			Message: &idl.HubReply_StdoutMsg{
				StdoutMsg: "stdout message",
			},
		}
		progressMsg := &idl.HubReply{
			Message: &idl.HubReply_ProgressMsg{
				ProgressMsg: &idl.ProgressMessage{
					Label: "progress message",
					Total: 0,
				},
			},
		}

		stream.StreamLogMsg(logMsg.GetLogMsg().Message)
		stream.StreamStdoutMsg(stdoutMsg.GetStdoutMsg())
		stream.StreamProgressMsg(progressMsg.GetProgressMsg().Label, int(progressMsg.GetProgressMsg().Total))

		if len(res.GetBuffer()) != 0 {
			t.Fatalf("got %d, want the buffer to be empty", len(res.GetBuffer()))
		}

		expected := fmt.Sprintf("unable to stream message %q: %s", logMsg, expectedErr)
		testutils.AssertLogMessage(t, logfile, regexp.QuoteMeta(expected))

		expected = fmt.Sprintf("unable to stream message %q: %s", stdoutMsg, expectedErr)
		testutils.AssertLogMessage(t, logfile, regexp.QuoteMeta(expected))

		expected = fmt.Sprintf("unable to stream message %q: %s", progressMsg, expectedErr)
		testutils.AssertLogMessage(t, logfile, regexp.QuoteMeta(expected))
	})
}

func DummyCommand() {
	os.Stdout.WriteString("stdout")
	time.Sleep(1 * time.Millisecond) // add a delay so that stderr is streamed after stdout and we can make a reliable assertion
	os.Stderr.WriteString("stderr")

	os.Exit(0)
}
