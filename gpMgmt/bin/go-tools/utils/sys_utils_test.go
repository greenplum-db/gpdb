package utils_test

import (
	"errors"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"

	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func TestCheckIfPortFree(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns no error when port not in use", func(t *testing.T) {
		_, err := utils.CheckIfPortFree("localhost", "9946")
		if err != nil {
			t.Fatalf("Got error %v, expected no error", err)
		}
	})
	t.Run("returns error when port not in use", func(t *testing.T) {
		portNum := "9946"
		hostName := "localhost"
		strErr := "bind: address already in use"
		listener, err := net.Listen("tcp", fmt.Sprintf("%s:%s", hostName, portNum))
		if err != nil {
			t.Fatalf("Failed to create test listener on port %s:%s", hostName, portNum)
		}
		defer listener.Close()

		_, err = utils.CheckIfPortFree(hostName, portNum)
		if err == nil || !strings.Contains(err.Error(), strErr) {
			t.Fatalf("Got error %v, expected:%sr", err, strErr)
		}
	})
}

func TestGetListDifference(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns correct diff when both lists are same", func(t *testing.T) {
		list1 := []string{"s1", "s2", "s3"}
		list2 := []string{"s1", "s2", "s3"}
		var expResult []string
		result := utils.GetListDifference(list1, list2)
		if !reflect.DeepEqual(result, expResult) {
			t.Fatalf("got:%v, expected:%v", result, list1)
		}
	})
	t.Run("returns correct diff when list1 and list2 has some common elements", func(t *testing.T) {
		list1 := []string{"s1", "s2", "s3"}
		list2 := []string{"s1", "s2"}
		expResult := []string{"s3"}
		result := utils.GetListDifference(list1, list2)
		if !reflect.DeepEqual(result, expResult) {
			t.Fatalf("got:%v, expected:%v", result, list1)
		}
	})
	t.Run("returns correct diff when list2 has some extra elements", func(t *testing.T) {
		list1 := []string{"s1", "s2", "s3"}
		list2 := []string{"s2", "s1", "s4", "s5"}
		expResult := []string{"s3"}
		result := utils.GetListDifference(list1, list2)
		if !reflect.DeepEqual(result, expResult) {
			t.Fatalf("got:%v, expected:%v", result, list1)
		}
	})
	t.Run("returns correct diff when list2 has no elements", func(t *testing.T) {
		list1 := []string{"s1", "s2", "s3"}
		list2 := []string{}
		expResult := list1
		result := utils.GetListDifference(list1, list2)
		if !reflect.DeepEqual(result, expResult) {
			t.Fatalf("got:%v, expected:%v", result, list1)
		}
	})
	t.Run("returns correct diff when list2 has some extra elements", func(t *testing.T) {
		list1 := []string{}
		list2 := []string{"s1", "s2", "s4", "s5"}
		expResult := list1
		result := utils.GetListDifference(list1, list2)
		if len(expResult) != 0 {
			t.Fatalf("got:%v, expected:%v", result, list1)
		}
	})
}

func TestWriteLinesToFile(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("succesfully writes to the file", func(t *testing.T) {
		filename := filepath.Join(os.TempDir(), "test")
		defer os.Remove(filename)

		lines := []string{"line1", "line2", "line3"}
		err := utils.WriteLinesToFile(filename, lines)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := strings.Join(lines, "\n")
		testutils.AssertFileContents(t, filename, expected)
	})

	t.Run("errors out when not able to create the file", func(t *testing.T) {
		file, err := os.CreateTemp("", "")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		err = os.Chmod(file.Name(), 0000)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		err = utils.WriteLinesToFile(file.Name(), []string{})

		expectedErr := os.ErrPermission
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("errors out when not able to write to the file", func(t *testing.T) {
		utils.System.Create = func(name string) (*os.File, error) {
			_, writer, _ := os.Pipe()
			writer.Close()

			return writer, nil
		}
		defer utils.ResetSystemFunctions()

		err := utils.WriteLinesToFile("", []string{})

		expectedErr := os.ErrClosed
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})
}

func TestAppendLinesToFile(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("succesfully appends to the file", func(t *testing.T) {
		file, err := os.CreateTemp("", "")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		defer os.Remove(file.Name())

		existingLines := []string{"line1", "line2", "line3"}
		_, err = file.WriteString(strings.Join(existingLines, "\n"))
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}

		newLines := []string{"line4", "line5", "line6"}
		err = utils.AppendLinesToFile(file.Name(), newLines)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := strings.Join(append(existingLines, newLines...), "\n")
		testutils.AssertFileContents(t, file.Name(), expected)
	})

	t.Run("errors out when the file does not exist", func(t *testing.T) {
		err := utils.AppendLinesToFile("non_exisitng_file", []string{})

		expectedErr := os.ErrNotExist
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("errors out when not able to write to the file", func(t *testing.T) {
		utils.System.OpenFile = func(name string, flag int, perm os.FileMode) (*os.File, error) {
			_, writer, _ := os.Pipe()
			writer.Close()

			return writer, nil
		}
		defer utils.ResetSystemFunctions()

		err := utils.AppendLinesToFile("", []string{})

		expectedErr := os.ErrClosed
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})
}

func TestGetHostAddrsNoLoopback(t *testing.T) {
	testhelper.SetupTestLogger()
	t.Run("returns the correct address without loopback", func(t *testing.T) {
		utils.System.InterfaceAddrs = func() ([]net.Addr, error) {
			_, addr1, _ := net.ParseCIDR("192.0.1.0/24")
			_, addr2, _ := net.ParseCIDR("2001:db8::/32")
			_, loopbackAddrIp4, _ := net.ParseCIDR("127.0.0.1/8")
			_, loopbackAddrIp6, _ := net.ParseCIDR("::1/128")

			return []net.Addr{addr1, addr2, loopbackAddrIp4, loopbackAddrIp6}, nil
		}
		defer utils.ResetSystemFunctions()

		result, err := utils.GetHostAddrsNoLoopback()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := []string{"192.0.1.0/24", "2001:db8::/32"}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %v, want %v", result, expected)
		}
	})

	t.Run("errors out when not able to get the host address", func(t *testing.T) {
		expectedErr := errors.New("error")
		utils.System.InterfaceAddrs = func() ([]net.Addr, error) {
			return nil, expectedErr
		}
		defer utils.ResetSystemFunctions()

		_, err := utils.GetHostAddrsNoLoopback()
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})
}
