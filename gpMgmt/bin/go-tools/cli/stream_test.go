package cli_test

import (
	"bytes"
	"errors"
	"io"
	"os"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/cli"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
)

type msgStream struct {
	msg []*idl.HubReply
	err error
}

func (m *msgStream) Recv() (*idl.HubReply, error) {
	if len(m.msg) == 0 {
		if m.err == nil {
			return nil, io.EOF
		}

		return nil, m.err
	}

	nextMsg := (m.msg)[0]
	m.msg = (m.msg)[1:]

	return nextMsg, nil
}

func TestParseStreamResponse(t *testing.T) {
	_, _, logfile := testhelper.SetupTestLogger()

	t.Run("displays the correct stream responses to the user", func(t *testing.T) {
		infoLogMsg := "info log message"
		warnLogMsg := "warning log message"
		errLogMsg := "error log message"
		dbgLogMsg := "debug log message"
		msg := []*idl.HubReply{
			{
				Message: &idl.HubReply_StdoutMsg{
					StdoutMsg: "stdout message",
				},
			},
			{
				Message: &idl.HubReply_LogMsg{
					LogMsg: &idl.LogMessage{Message: infoLogMsg, Level: idl.LogLevel_INFO},
				},
			},
			{
				Message: &idl.HubReply_LogMsg{
					LogMsg: &idl.LogMessage{Message: warnLogMsg, Level: idl.LogLevel_WARNING},
				},
			},
			{
				Message: &idl.HubReply_LogMsg{
					LogMsg: &idl.LogMessage{Message: errLogMsg, Level: idl.LogLevel_ERROR},
				},
			},
			{
				Message: &idl.HubReply_LogMsg{
					LogMsg: &idl.LogMessage{Message: dbgLogMsg, Level: idl.LogLevel_DEBUG},
				},
			},
			{
				Message: &idl.HubReply_ProgressMsg{
					ProgressMsg: &idl.ProgressMessage{
						Label: "progress message",
						Total: 1,
					},
				},
			},
			{
				Message: &idl.HubReply_ProgressMsg{
					ProgressMsg: &idl.ProgressMessage{
						Label: "progress message",
						Total: 1,
					},
				},
			},
		}

		oldStdout := os.Stdout
		reader, writer, err := os.Pipe()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		os.Stdout = writer
		defer func() {
			os.Stdout = oldStdout
		}()

		err = cli.ParseStreamResponse(&msgStream{msg: msg})
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		writer.Close()
		out, err := io.ReadAll(reader)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		testutils.AssertLogMessage(t, logfile, infoLogMsg)
		testutils.AssertLogMessage(t, logfile, warnLogMsg)
		testutils.AssertLogMessage(t, logfile, errLogMsg)
		testutils.AssertLogMessage(t, logfile, dbgLogMsg)

		expectedStdoutMsg := "stdout message"
		if !bytes.Contains(out, []byte(expectedStdoutMsg)) {
			t.Fatalf("got %v, want %v", out, expectedStdoutMsg)
		}

		expectedProgressContents := []string{"progress message", "done", "1/1"}
		for _, expected := range expectedProgressContents {
			if !bytes.Contains(out, []byte(expected)) {
				t.Fatalf("got %v, want %v", out, expected)
			}
		}
	})

	t.Run("returns non EOF errors and aborts any running progress bars", func(t *testing.T) {
		msg := []*idl.HubReply{
			{
				Message: &idl.HubReply_ProgressMsg{
					ProgressMsg: &idl.ProgressMessage{
						Label: "progress message",
						Total: 5,
					},
				},
			},
			{
				Message: &idl.HubReply_ProgressMsg{
					ProgressMsg: &idl.ProgressMessage{
						Label: "progress message",
						Total: 5,
					},
				},
			},
		}

		oldStdout := os.Stdout
		reader, writer, err := os.Pipe()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		os.Stdout = writer
		defer func() {
			os.Stdout = oldStdout
		}()

		expectedErr := errors.New("error")
		err = cli.ParseStreamResponse(&msgStream{
			msg: msg,
			err: expectedErr,
		})
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}

		writer.Close()
		out, err := io.ReadAll(reader)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expectedProgressContents := []string{"progress message", "error", "1/5"}
		for _, expected := range expectedProgressContents {
			if !bytes.Contains(out, []byte(expected)) {
				t.Fatalf("got %v, want %v", out, expected)
			}
		}
	})
}
