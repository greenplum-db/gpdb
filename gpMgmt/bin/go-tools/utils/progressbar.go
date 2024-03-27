package utils

import (
	"fmt"
	"io"

	"github.com/vbauerster/mpb/v8"
	"github.com/vbauerster/mpb/v8/decor"
)

const ( // ASCII color codes
	red   = "\033[31m"
	green = "\033[32m"
	reset = "\033[0m"
)

func NewProgressInstance(output io.Writer) *mpb.Progress {
	return mpb.New(
		mpb.WithWidth(64),
		mpb.PopCompletedMode(),
		mpb.WithOutput(output),
		mpb.WithAutoRefresh(),
	)
}

func NewProgressBar(instance *mpb.Progress, label string, size int) *mpb.Bar {
	bar := instance.AddBar(int64(size),
		mpb.PrependDecorators(
			decor.Name(label, decor.WC{W: len(label) + 1, C: decor.DidentRight}),
			decor.CountersNoUnit("%d/%d"),
			decor.Elapsed(decor.ET_STYLE_GO, decor.WC{W: 6}),
		),
		mpb.AppendDecorators(
			decor.OnAbort(
				decor.OnComplete(
					decor.Percentage(decor.WC{W: 4}), fmt.Sprintf("%sdone%s", green, reset),
				),
				fmt.Sprintf("%serror%s", red, reset),
			),
		),
	)

	return bar
}
