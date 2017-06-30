package test_utils

import "fmt"

type FakeRows struct {
	FakeColumns      []string
	nexti            int
	SampleRowStrings []interface{}

	NumRows int
}

func (fake *FakeRows) Columns() ([]string, error) {
	return fake.FakeColumns, nil
}

func (fake *FakeRows) Next() bool {
	fake.nexti++
	return fake.nexti <= fake.NumRows
}

func (fake *FakeRows) Scan(dest ...interface{}) error {
	if len(dest) != len(fake.FakeColumns) {
		return fmt.Errorf("rows_wrapper_fake: expected %d destination arguments in Scan, not %d", len(fake.FakeColumns), len(dest))
	}
	if len(fake.SampleRowStrings) != len(fake.FakeColumns) {
		return fmt.Errorf("rows_wrapper_fake: column count %d needs to match length of sample row %d", len(fake.FakeColumns), len(fake.SampleRowStrings))
	}

	for i, value := range fake.SampleRowStrings {
		cast_dest := dest[i].(*interface{})
		*cast_dest = value
	}

	return nil
}
