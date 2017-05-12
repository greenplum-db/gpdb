package utils

import (
	"fmt"
	"io"
	"log"
	"os"
)

var (
	logger *Logger

	defaultLogDir   = "gpAdminLogs"
	headerFormatStr = "%s:%s:%s:%06d-[%s]:-" // PROGRAMNAME:USERNAME:HOSTNAME:PID-[LOGLEVEL]:-, to match gpcrondump

	FPSetLogger = SetLogger
	FPOsGetpid  = os.Getpid
)

const ( // Constants representing the current logging level
	LOGERROR = iota
	LOGINFO
	LOGVERBOSE
	LOGDEBUG
)

type Logger struct {
	logStdout *log.Logger
	logStderr *log.Logger
	logFile   *log.Logger
	verbosity int
	header    string
}

// stdout and stderr are passed in to enable redirection for testing
func NewLogger(stdout io.Writer, stderr io.Writer, logFile io.Writer, verbosity int, header string) *Logger {
	if verbosity < LOGERROR || verbosity > LOGDEBUG {
		Abort("Cannot create logger with an invalid logging level")
	}
	return &Logger{
		logStdout: log.New(stdout, "", 0),
		logStderr: log.New(stderr, "", 0),
		logFile:   log.New(logFile, "", 0),
		verbosity: verbosity,
		header:    header,
	}
}

func SetLogger(log *Logger) {
	logger = log
}

// Creates a logger, sets it to global so it's usable in utils/, and returns it so it can be used in backup/ and restore/
func InitializeLogging(program string, logdir string, verbosity int) *Logger {
	user, homedir, host := FPGetUserAndHostInfo()
	pid := FPOsGetpid()
	header := fmt.Sprintf(headerFormatStr, program, user, host, pid, "%s")

	if logdir == "" {
		logdir = fmt.Sprintf("%s/%s", homedir, defaultLogDir)
	}
	FPDirectoryMustExist(logdir)

	logfile := fmt.Sprintf("%s/%s_%s.log", logdir, program, CurrentDatestamp())
	logFileHandle := FPMustOpenFile(logfile)

	logger := NewLogger(os.Stdout, os.Stderr, logFileHandle, verbosity, header)
	FPSetLogger(logger)
	return logger
}

func (logger *Logger) GetLogPrefix(level string) string {
	logTimestamp := FPTimeNow().Format("20060102:15:04:05")
	return fmt.Sprintf("%s %s", logTimestamp, fmt.Sprintf(logger.header, level))
}

func (logger *Logger) Info(s string, v ...interface{}) {
	if logger.verbosity >= LOGINFO {
		message := logger.GetLogPrefix("INFO") + fmt.Sprintf(s, v...)
		logger.logFile.Output(1, message)
		logger.logStdout.Output(1, message)
	}
}

func (logger *Logger) Warn(s string, v ...interface{}) {
	message := logger.GetLogPrefix("WARNING") + fmt.Sprintf(s, v...)
	logger.logFile.Output(1, message)
	logger.logStdout.Output(1, message)
}

func (logger *Logger) Verbose(s string, v ...interface{}) {
	if logger.verbosity >= LOGVERBOSE {
		message := logger.GetLogPrefix("DEBUG") + fmt.Sprintf(s, v...)
		logger.logFile.Output(1, message)
		logger.logStdout.Output(1, message)
	}
}

func (logger *Logger) Debug(s string, v ...interface{}) {
	if logger.verbosity >= LOGDEBUG {
		message := logger.GetLogPrefix("DEBUG") + fmt.Sprintf(s, v...)
		logger.logFile.Output(1, message)
		logger.logStdout.Output(1, message)
	}
}

func (logger *Logger) Error(s string, v ...interface{}) {
	message := logger.GetLogPrefix("ERROR") + fmt.Sprintf(s, v...)
	logger.logFile.Output(1, message)
	logger.logStderr.Output(1, message)
}

func (logger *Logger) Fatal(s string, v ...interface{}) {
	message := logger.GetLogPrefix("CRITICAL") + fmt.Sprintf(s, v...)
	logger.logFile.Output(1, message)
	if logger.verbosity >= LOGVERBOSE {
		AbortWithStackTrace(message)
	} else {
		Abort(message)
	}
}

func (logger *Logger) GetVerbosity() int {
	return logger.verbosity
}

func (logger *Logger) SetVerbosity(verbosity int) {
	logger.verbosity = verbosity
}
