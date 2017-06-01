package utils

type RowsWrapper interface {
	Columns() ([]string, error)
	Scan(dest ...interface{}) error
	Next() bool
}
