package utils_test

import (
	"backup_restore/backup"
	"backup_restore/testutils"
	"backup_restore/utils"
	"database/sql/driver"
	"errors"
	"io/ioutil"
	"os"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

func TestIO(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("utils/io tests", func() {
	var connection *utils.DBConn
	var mock sqlmock.Sqlmock
	BeforeEach(func() {
		connection, mock = testutils.CreateAndConnectMockDB()
		testutils.SetupTestLogger()
	})

	Describe("DirectoryMustExist", func() {
		It("does nothing if the directory exists", func() {
			utils.FPOsStat = func(name string) (os.FileInfo, error) { return nil, nil }
			defer func() { utils.FPOsStat = os.Stat }()
			utils.DirectoryMustExist("dirname")
		})
		It("panics if the directory does not exist", func() {
			utils.FPOsStat = func(name string) (os.FileInfo, error) { return nil, errors.New("No such file or directory") }
			defer func() { utils.FPOsStat = os.Stat }()
			defer testutils.ShouldPanicWithMessage("Cannot stat directory dirname: No such file or directory")
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
	Describe("GetSegmentConfiguration", func() {
		header := []string{"content", "hostname", "datadir"}
		localSegOne := []driver.Value{"0", "localhost", "/data/gpseg0"}
		localSegTwo := []driver.Value{"1", "localhost", "/data/gpseg1"}
		remoteSegOne := []driver.Value{"2", "remotehost", "/data/gpseg2"}

		It("returns a configuration for a single-host, single-segment cluster", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(localSegOne...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := utils.GetSegmentConfiguration(connection)
			Expect(len(results)).To(Equal(1))
			Expect(results[0].DataDir).To(Equal("/data/gpseg0"))
		})
		It("returns a configuration for a single-host, multi-segment cluster", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(localSegOne...).AddRow(localSegTwo...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := utils.GetSegmentConfiguration(connection)
			Expect(len(results)).To(Equal(2))
			Expect(results[0].DataDir).To(Equal("/data/gpseg0"))
			Expect(results[1].DataDir).To(Equal("/data/gpseg1"))
		})
		It("returns a configuration for a multi-host, multi-segment cluster", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(localSegOne...).AddRow(localSegTwo...).AddRow(remoteSegOne...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := utils.GetSegmentConfiguration(connection)
			Expect(len(results)).To(Equal(3))
			Expect(results[0].DataDir).To(Equal("/data/gpseg0"))
			Expect(results[1].DataDir).To(Equal("/data/gpseg1"))
			Expect(results[2].DataDir).To(Equal("/data/gpseg2"))
		})
	})
	Describe("CreateDumpDirs", func() {
		segOne := utils.QuerySegConfig{0, "localhost", "/data/gpseg0"}
		segTwo := utils.QuerySegConfig{1, "localhost", "/data/gpseg1"}
		segConfig := []utils.QuerySegConfig{segOne, segTwo}

		It("creates directories relative to the segment data directory", func() {
			checkMap := make(map[string]bool, 0)
			utils.FPOsMkdirAll = func(path string, perm os.FileMode) error {
				checkMap[path] = true
				Expect(perm).To(Equal(os.FileMode(0700)))
				return nil
			}
			defer func() { utils.FPOsMkdirAll = os.MkdirAll }()
			utils.DumpPathFmtStr = "<SEG_DATA_DIR>/backups/20170101/20170101010101"
			utils.CreateDumpDirs(segConfig)
			Expect(len(checkMap)).To(Equal(2))
			Expect(checkMap["/data/gpseg0/backups/20170101/20170101010101"]).To(BeTrue())
			Expect(checkMap["/data/gpseg1/backups/20170101/20170101010101"]).To(BeTrue())
		})
		It("creates directories relative to a user-specified directory", func() {
			checkMap := make(map[string]bool, 0)
			utils.FPOsMkdirAll = func(path string, perm os.FileMode) error {
				checkMap[path] = true
				Expect(perm).To(Equal(os.FileMode(0700)))
				return nil
			}
			defer func() { utils.FPOsMkdirAll = os.MkdirAll }()
			utils.DumpPathFmtStr = "/tmp/foo/backups/20170101/20170101010101"
			utils.CreateDumpDirs(segConfig)
			Expect(len(checkMap)).To(Equal(1))
			Expect(checkMap["/tmp/foo/backups/20170101/20170101010101"]).To(BeTrue())
		})
	})
	Describe("WriteTableMapFile", func() {
		tableOne := utils.Table{0, 1234, "public", "foo"}
		tableTwo := utils.Table{0, 2345, "public", "foo|bar"}

		It("writes a map file containing one table", func() {
			filePath := ""
			r, w, _ := os.Pipe()
			utils.FPOsCreate = func(name string) (*os.File, error) { filePath = name; return w, nil }
			defer func() { utils.FPOsCreate = os.Create }()
			tables := []utils.Table{tableOne}
			backup.WriteTableMapFile(tables)
			w.Close()
			output, _ := ioutil.ReadAll(r)
			testutils.ExpectRegex(string(output), `public.foo: 1234
`)
		})
		It("writes a map file containing multiple tables", func() {
			filePath := ""
			r, w, _ := os.Pipe()
			utils.FPOsCreate = func(name string) (*os.File, error) { filePath = name; return w, nil }
			defer func() { utils.FPOsCreate = os.Create }()
			tables := []utils.Table{tableOne, tableTwo}
			backup.WriteTableMapFile(tables)
			w.Close()
			output, _ := ioutil.ReadAll(r)
			testutils.ExpectRegex(string(output), `public.foo: 1234
public."foo|bar": 2345`)
		})
	})
})
