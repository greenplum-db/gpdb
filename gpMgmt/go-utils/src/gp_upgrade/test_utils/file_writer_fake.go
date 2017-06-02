package test_utils

import (
	"io"
	"os"
)

type ErrorFileWriterDuringWrite struct{}

func (formatter ErrorFileWriterDuringWrite) Write(f *os.File, data []byte) error {
	return io.ErrShortWrite
}
