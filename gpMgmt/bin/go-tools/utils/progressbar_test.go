package utils_test

import (
	"bytes"
	"testing"

	"github.com/greenplum-db/gpdb/gp/utils"
)

func TestProgressBar(t *testing.T) {
	t.Run("creates progress bar with the correct label and total count", func(t *testing.T) {
		var buf bytes.Buffer
		instance := utils.NewProgressInstance(&buf)
		bar := utils.NewProgressBar(instance, "Test", 10)

		for i := 0; i < 10; i++ {
			bar.Increment()
		}
		instance.Wait()

		expectedLabel := "Test"
		if !bytes.Contains(buf.Bytes(), []byte(expectedLabel)) {
			t.Fatalf("expected string %q not present in progress bar", expectedLabel)
		}

		expectedTotal := "/10"
		if !bytes.Contains(buf.Bytes(), []byte(expectedTotal)) {
			t.Fatalf("expected string %q not present in progress bar", expectedTotal)
		}
	})

	t.Run("has the correct bar style", func(t *testing.T) {
		var buf bytes.Buffer
		instance := utils.NewProgressInstance(&buf)
		bar := utils.NewProgressBar(instance, "Test", 5)

		for i := 0; i < 5; i++ {
			bar.Increment()
		}
		instance.Wait()

		expectedProgress := "==="
		expectedClose := "["
		expectedStart := "]"

		if !bytes.Contains(buf.Bytes(), []byte(expectedProgress)) {
			t.Fatalf("expected string %q not present in progress bar", expectedProgress)
		}
		if !bytes.Contains(buf.Bytes(), []byte(expectedClose)) {
			t.Fatalf("expected string %q not present in progress bar", expectedProgress)
		}
		if !bytes.Contains(buf.Bytes(), []byte(expectedStart)) {
			t.Fatalf("expected string %q not present in progress bar", expectedProgress)
		}
	})

	t.Run("appends done once the bar is completed", func(t *testing.T) {
		var buf bytes.Buffer
		instance := utils.NewProgressInstance(&buf)
		bar := utils.NewProgressBar(instance, "Test", 10)

		for i := 0; i < 10; i++ {
			bar.Increment()
		}
		instance.Wait()

		expected := "\033[32mdone\033[0m"
		if !bytes.Contains(buf.Bytes(), []byte(expected)) {
			t.Fatalf("expected string %q not present in progress bar", expected)
		}
	})

	t.Run("appends error if the bar is aborted", func(t *testing.T) {
		var buf bytes.Buffer
		instance := utils.NewProgressInstance(&buf)
		bar := utils.NewProgressBar(instance, "Test", 10)

		for i := 0; i < 10; i++ {
			bar.Increment()
			if i == 5 {
				bar.Abort(false)
			}
		}
		instance.Wait()

		expected := "\033[31merror\033[0m"
		if !bytes.Contains(buf.Bytes(), []byte(expected)) {
			t.Fatalf("expected string %q not present in progress bar", expected)
		}
	})
}
