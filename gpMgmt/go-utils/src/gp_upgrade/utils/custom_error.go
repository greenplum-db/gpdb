package utils

type CustomError interface {
	ReturnCode() int
	Error() string
	Prefix() string
}

func GetExitCodeForError(err error) int {
	switch err := err.(type) {
	case CustomError:
		return err.(CustomError).ReturnCode()

	default:
		return 1
	}
}
