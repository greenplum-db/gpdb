package utils_test

import (
	"backup_restore/testutils"
	"backup_restore/utils"
	"fmt"
	"io"
	"os"
	"reflect"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
)

func TestLog(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("utils/log tests", func() {
	logger, stdout, stderr, logfile := testutils.SetupTestLogger()
	fakeFile := gbytes.NewBuffer()
	var testLogger, sampleLogger *utils.Logger
	var testLogDir string

	BeforeEach(func() {
		utils.FPDirectoryMustExist = func(dirname string) {
			if dirname != testLogDir {
				Fail(fmt.Sprintf("Wrong log directory: found %s, expected %s", dirname, testLogDir))
			}
		}
		utils.FPMustOpenFile = func(filename string) io.Writer { return fakeFile }
		utils.FPGetUserAndHostInfo = func() (string, string, string) { return "testUser", "testDir", "testHost" }
		utils.FPSetLogger = func(log *utils.Logger) { testLogger = log }
		utils.FPOsGetpid = func() int { return 0 }
	})
	AfterEach(func() {
		utils.FPDirectoryMustExist = utils.DirectoryMustExist
		utils.FPMustOpenFile = utils.MustOpenFile
		utils.FPGetUserAndHostInfo = utils.GetUserAndHostInfo
		utils.FPSetLogger = utils.SetLogger
		utils.FPOsGetpid = os.Getpid
	})

	Describe("InitializeLogging", func() {
		BeforeEach(func() {
			testLogDir = ""
			sampleLogger = utils.NewLogger(os.Stdout, os.Stderr, fakeFile, utils.LOGINFO, "testProgram:testUser:testHost:000000-[%s]:-")
		})
		Context("Logger initialized with default log directory and Info log level", func() {
			It("creates a new logger writing to gpAdminLogs and sets utils.logger to this new logger", func() {
				testLogDir = "testDir/gpAdminLogs"
				newLogger := utils.InitializeLogging("testProgram", "", utils.LOGINFO)
				if testLogger == nil || !(newLogger == testLogger) {
					Fail("Created logger was not assigned to utils.logger")
				}
				if !reflect.DeepEqual(newLogger, sampleLogger) {
					Fail(fmt.Sprintf("Created logger does not match sample logger:\n%v\n%v", newLogger, sampleLogger))
				}
			})
		})
		Context("Logger initialized with a specified log directory and Info log level", func() {
			It("creates a new logger writing to the specified log directory and sets utils.logger to this new logger", func() {
				testLogDir = "/tmp/log_dir"
				newLogger := utils.InitializeLogging("testProgram", "/tmp/log_dir", utils.LOGINFO)
				if testLogger == nil || !(newLogger == testLogger) {
					Fail("Created logger was not assigned to utils.logger")
				}
				if !reflect.DeepEqual(newLogger, sampleLogger) {
					Fail(fmt.Sprintf("Created logger does not match sample logger:\n%v\n%v", newLogger, sampleLogger))
				}
			})
		})
	})
	Describe("GetLogPrefix", func() {
		It("returns a prefix for the current time", func() {
			expected := "20170101:01:01:01 testProgram:testUser:testHost:000000-[INFO]:-"
			prefix := logger.GetLogPrefix("INFO")
			Expect(expected).To(Equal(prefix))
		})
	})
	Describe("Output function tests", func() {
		patternExpected := "20170101:01:01:01 testProgram:testUser:testHost:000000-[%s]:-"
		infoExpected := fmt.Sprintf(patternExpected, "INFO")
		warnExpected := fmt.Sprintf(patternExpected, "WARNING")
		verboseExpected := fmt.Sprintf(patternExpected, "INFO")
		debugExpected := fmt.Sprintf(patternExpected, "INFO")
		errorExpected := fmt.Sprintf(patternExpected, "ERROR")
		fatalExpected := fmt.Sprintf(patternExpected, "CRITICAL")

		Describe("Verbosity set to Error", func() {
			logger.SetVerbosity(utils.LOGERROR)
			Context("Info", func() {
				It("doesn't print", func() {
					logger.Info("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, "")
				})
			})
			Context("Warn", func() {
				It("prints to stdout and the log file", func() {
					logger.Warn("test message")
					testutils.ExpectRegexp(stdout, warnExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, warnExpected)
				})
			})
			Context("Verbose", func() {
				It("doesn't print", func() {
					logger.Verbose("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, "")
				})
			})
			Context("Debug", func() {
				It("doesn't print", func() {
					logger.Debug("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, "")
				})
			})
			Context("Error", func() {
				It("prints to stderr and the log file", func() {
					logger.Error("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, errorExpected)
					testutils.ExpectRegexp(logfile, errorExpected)
				})
			})
			Context("Fatal", func() {
				It("prints to the log file, then panics", func() {
					defer func() {
						testutils.ExpectRegexp(stdout, "")
						testutils.ExpectRegexp(stderr, "")
						testutils.ExpectRegexp(logfile, fatalExpected)
					}()
					defer testutils.ShouldPanicWithMessage("test message")
					logger.Fatal("test message")
				})
			})
		})
		Describe("Verbosity set to Info", func() {
			logger.SetVerbosity(utils.LOGINFO)
			Context("Info", func() {
				It("prints to stdout and the log file", func() {
					logger.Info("test message")
					testutils.ExpectRegexp(stdout, infoExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, infoExpected)
				})
			})
			Context("Warn", func() {
				It("prints to stdout and the log file", func() {
					logger.Warn("test message")
					testutils.ExpectRegexp(stdout, warnExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, warnExpected)
				})
			})
			Context("Verbose", func() {
				It("doesn't print", func() {
					logger.Verbose("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, "")
				})
			})
			Context("Debug", func() {
				It("doesn't print", func() {
					logger.Debug("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, "")
				})
			})
			Context("Error", func() {
				It("prints to stderr and the log file", func() {
					logger.Error("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, errorExpected)
					testutils.ExpectRegexp(logfile, errorExpected)
				})
			})
			Context("Fatal", func() {
				It("prints to the log file, then panics", func() {
					defer func() {
						testutils.ExpectRegexp(stdout, "")
						testutils.ExpectRegexp(stderr, "")
						testutils.ExpectRegexp(logfile, fatalExpected)
					}()
					defer testutils.ShouldPanicWithMessage("test message")
					logger.Fatal("test message")
				})
			})
		})
		Describe("Verbosity set to Verbose", func() {
			logger.SetVerbosity(utils.LOGVERBOSE)
			Context("Info", func() {
				It("prints to stdout and the log file", func() {
					logger.Info("test message")
					testutils.ExpectRegexp(stdout, infoExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, infoExpected)
				})
			})
			Context("Warn", func() {
				It("prints to stdout and the log file", func() {
					logger.Warn("test message")
					testutils.ExpectRegexp(stdout, warnExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, warnExpected)
				})
			})
			Context("Verbose", func() {
				It("prints to stdout and the log file", func() {
					logger.Info("test message")
					testutils.ExpectRegexp(stdout, verboseExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, verboseExpected)
				})
			})
			Context("Debug", func() {
				It("doesn't print", func() {
					logger.Debug("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, "")
				})
			})
			Context("Error", func() {
				It("prints to stderr and the log file", func() {
					logger.Error("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, errorExpected)
					testutils.ExpectRegexp(logfile, errorExpected)
				})
			})
			Context("Fatal", func() {
				It("prints to the log file, then panics", func() {
					defer func() {
						testutils.ExpectRegexp(stdout, "")
						testutils.ExpectRegexp(stderr, "")
						testutils.ExpectRegexp(logfile, fatalExpected)
					}()
					defer testutils.ShouldPanicWithMessage("test message")
					logger.Fatal("test message")
				})
			})
		})
		Describe("Verbosity set to Debug", func() {
			logger.SetVerbosity(utils.LOGDEBUG)
			Context("Info", func() {
				It("prints to stdout and the log file", func() {
					logger.Info("test message")
					testutils.ExpectRegexp(stdout, infoExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, infoExpected)
				})
			})
			Context("Warn", func() {
				It("prints to stdout and the log file", func() {
					logger.Warn("test message")
					testutils.ExpectRegexp(stdout, warnExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, warnExpected)
				})
			})
			Context("Verbose", func() {
				It("prints to stdout and the log file", func() {
					logger.Info("test message")
					testutils.ExpectRegexp(stdout, verboseExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, verboseExpected)
				})
			})
			Context("Debug", func() {
				It("prints to stdout and the log file", func() {
					logger.Info("test message")
					testutils.ExpectRegexp(stdout, debugExpected)
					testutils.ExpectRegexp(stderr, "")
					testutils.ExpectRegexp(logfile, debugExpected)
				})
			})
			Context("Error", func() {
				It("prints to stderr and the log file", func() {
					logger.Error("test message")
					testutils.ExpectRegexp(stdout, "")
					testutils.ExpectRegexp(stderr, errorExpected)
					testutils.ExpectRegexp(logfile, errorExpected)
				})
			})
			Context("Fatal", func() {
				It("prints to the log file, then panics", func() {
					defer func() {
						testutils.ExpectRegexp(stdout, "")
						testutils.ExpectRegexp(stderr, "")
						testutils.ExpectRegexp(logfile, fatalExpected)
					}()
					defer testutils.ShouldPanicWithMessage("test message")
					logger.Fatal("test message")
				})
			})
		})
	})
})
