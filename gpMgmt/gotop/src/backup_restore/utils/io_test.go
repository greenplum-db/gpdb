package utils_test

import (
	"backup_restore/testutils"
	"backup_restore/utils"
	"errors"
	"os"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

func TestIO(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("utils/log tests", func() {
	Describe("DirectoryMustExist", func() {
		It("does nothing if the directory exists", func() {
			utils.FPOsStat = func(name string) (os.FileInfo, error) { return nil, nil }
			defer func() { utils.FPOsStat = os.Stat }()
			utils.DirectoryMustExist("dirname")
		})
		It("panics if the directory does not exist", func() {
			utils.FPOsStat = func(name string) (os.FileInfo, error) { return nil, errors.New("No such file or directory") }
			defer func() { utils.FPOsStat = os.Stat }()
			defer testutils.ShouldPanicWithMessage("Cannot use directory dirname as log directory: No such file or directory")
			utils.DirectoryMustExist("dirname")
		})
	})
	Describe("MustOpenFile", func() {
		It("creates or opens the file for writing", func() {
			utils.FPOsCreate = func(name string) (*os.File, error) { return os.Stdout, nil }
			defer func() { utils.FPOsCreate = os.Create }()
			fileHandle := utils.MustOpenFile("filename")
			Expect(fileHandle).To(Equal(os.Stdout))
		})
		It("panics on error", func() {
			utils.FPOsCreate = func(name string) (*os.File, error) { return nil, errors.New("Permission denied") }
			defer func() { utils.FPOsCreate = os.Create }()
			defer testutils.ShouldPanicWithMessage("Unable to create or open file filename: Permission denied")
			utils.MustOpenFile("filename")
		})
	})
})
