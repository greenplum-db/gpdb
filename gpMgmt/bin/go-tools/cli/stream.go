package cli

import (
	"fmt"
	"io"
	"os"
	"time"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/vbauerster/mpb/v8"
)

type StreamReceiver interface {
	Recv() (*idl.HubReply, error)
}

func ParseStreamResponseFn(stream StreamReceiver) error {
	progressBarMap := make(map[string]*mpb.Bar)
	progressInstance := utils.NewProgressInstance(os.Stdout)

	for {
		resp, err := stream.Recv()
		if err == io.EOF {
			break
		} else if err != nil {
			for _, bar := range progressBarMap {
				bar.Abort(false)
			}
			progressInstance.Wait()

			return utils.FormatGrpcError(err)
		}

		msg := resp.Message
		switch msg.(type) {
		case *idl.HubReply_LogMsg:
			logMsg := resp.GetLogMsg()
			switch logMsg.Level {
			case idl.LogLevel_DEBUG:
				gplog.Verbose(logMsg.Message)
			case idl.LogLevel_WARNING:
				gplog.Warn(logMsg.Message)
			case idl.LogLevel_ERROR:
				gplog.Error(logMsg.Message)
			case idl.LogLevel_FATAL:
				gplog.Fatal(nil, logMsg.Message)
			default:
				gplog.Info(logMsg.Message)
			}

		case *idl.HubReply_StdoutMsg:
			fmt.Print(resp.GetStdoutMsg())

		case *idl.HubReply_ProgressMsg:
			progressMsg := resp.GetProgressMsg()
			if _, ok := progressBarMap[progressMsg.Label]; !ok {
				progressBarMap[progressMsg.Label] = utils.NewProgressBar(progressInstance, progressMsg.Label, int(progressMsg.Total))
			} else {
				bar := progressBarMap[progressMsg.Label]
				bar.Increment()

				if bar.Current() == int64(progressMsg.Total) {
					bar.Wait()
					time.Sleep(500 * time.Millisecond)
				}
			}
		}
	}

	progressInstance.Wait()

	return nil
}
