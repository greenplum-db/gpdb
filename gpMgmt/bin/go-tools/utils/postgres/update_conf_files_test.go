package postgres_test

import (
	"errors"
	"io/fs"
	"os"
	"os/user"
	"path/filepath"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/postgres"
)

func TestUpdatePostgresqlConf(t *testing.T) {
	testhelper.SetupTestLogger()

	cases := []struct {
		overwrite    bool
		configParams map[string]string
		confContent  string
		expected     string
	}{
		{
			overwrite: true,
			configParams: map[string]string{
				"guc_1": "new_value_1",
			},
			confContent: `
guc_1 = value_1
guc_2 = value_2
# comment
guc_1 = value_1`,
			expected: `
guc_1 = 'new_value_1'
guc_2 = value_2
# comment
guc_1 = 'new_value_1'`,
		},
		{
			overwrite: false,
			configParams: map[string]string{
				"guc_1": "new_value_1",
			},
			confContent: `
guc_1 = value_1
guc_2 = value_2
# comment
guc_1 = value_1`,
			expected: `
guc_1 = 'new_value_1' # guc_1 = value_1
guc_2 = value_2
# comment
guc_1 = 'new_value_1' # guc_1 = value_1`,
		},
		{
			overwrite: true,
			configParams: map[string]string{
				"guc_1": "new_value_1",
				"guc_3": "new_value_3",
			},
			confContent: `
guc_1 = value_1
guc_2 = value_2`,
			expected: `
guc_1 = 'new_value_1'
guc_2 = value_2
guc_3 = 'new_value_3'`,
		},
		{
			overwrite: true,
			configParams: map[string]string{
				"guc_1": "new_value_1",
				"guc_3": "1234",
			},
			confContent: `
guc_1 value_1
guc_2 value_2`,
			expected: `
guc_1 = 'new_value_1'
guc_2 value_2
guc_3 = 1234`,
		},
		{
			overwrite: true,
			configParams: map[string]string{
				"guc_1": "new_value_1",
			},
			confContent: `
guc_1 value_1
#guc_1 value_1
guc_1a value_1
guc_1_a value_1
guc_1a = value_1
guc_1_a=value_1
guc_2 value_2`,
			expected: `
guc_1 = 'new_value_1'
#guc_1 value_1
guc_1a value_1
guc_1_a value_1
guc_1a = value_1
guc_1_a=value_1
guc_2 value_2`,
		},
	}

	for _, tc := range cases {
		t.Run("correctly updates the postgresql.conf file", func(t *testing.T) {
			dname, confPath := createTempConfFile(t, "postgresql.conf", tc.confContent, 0644)
			defer os.RemoveAll(dname)

			err := postgres.UpdatePostgresqlConf(dname, tc.configParams, tc.overwrite)
			if err != nil {
				t.Fatalf("unexpected error: %#v", err)
			}

			testutils.AssertFileContents(t, confPath, tc.expected)
		})
	}

	t.Run("errors out when there is no file present", func(t *testing.T) {
		dname, _ := createTempConfFile(t, "", "", 0644)
		defer os.RemoveAll(dname)

		expectedErr := os.ErrNotExist
		err := postgres.UpdatePostgresqlConf(dname, map[string]string{}, true)
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("returns appropriate error when fails to update the conf file", func(t *testing.T) {
		dname, _ := createTempConfFile(t, "postgresql.conf", "", 0644)
		defer os.RemoveAll(dname)

		expectedErr := errors.New("error")
		utils.System.Create = func(name string) (*os.File, error) {
			return nil, expectedErr
		}
		defer utils.ResetSystemFunctions()

		err := postgres.UpdatePostgresqlConf(dname, map[string]string{}, true)
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})
}

func TestCreatePostgresInternalConf(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("successfully creates the internal.auto.conf", func(t *testing.T) {
		dname, _ := createTempConfFile(t, "", "", 0644)
		defer os.RemoveAll(dname)

		confPath := filepath.Join(dname, "internal.auto.conf")
		_, err := os.Stat(confPath)
		if !os.IsNotExist(err) {
			t.Fatalf("expected %s to not exist", confPath)
		}

		err = postgres.CreatePostgresInternalConf(dname, -1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := "gp_dbid = -1"
		testutils.AssertFileContents(t, confPath, expected)
	})

	t.Run("errors out when not able to create the file", func(t *testing.T) {
		dname, _ := createTempConfFile(t, "internal.auto.conf", "", 0000)
		defer os.RemoveAll(dname)

		expectedErr := os.ErrPermission
		err := postgres.CreatePostgresInternalConf(dname, -1)
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("errors out when not able to write to the file", func(t *testing.T) {
		dname, _ := createTempConfFile(t, "", "", 0644)
		defer os.RemoveAll(dname)
		utils.System.OpenFile = func(name string, flag int, perm os.FileMode) (*os.File, error) {
			_, writer, _ := os.Pipe()
			writer.Close()

			return writer, nil
		}
		defer utils.ResetSystemFunctions()

		expectedErr := os.ErrClosed
		err := postgres.CreatePostgresInternalConf(dname, -1)
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})
}

func TestBuildPgHbaConf(t *testing.T) {
	testhelper.SetupTestLogger()

	coordinatorCases := []struct {
		coordinator      bool
		coordinatorAddrs []string
		addrs            []string
		confContent      string
		expected         string
	}{
		{
			coordinator: true,
			addrs:       []string{"192.0.1.0/24", "2001:db8::/32"},
			confContent: ``,
			expected: `local	all	gpadmin	ident
host	all	gpadmin	localhost	trust
host	all	gpadmin	192.0.1.0/24	trust
host	all	gpadmin	2001:db8::/32	trust
local	replication	gpadmin	ident
host	replication	gpadmin	samehost	trust
host	replication	gpadmin	192.0.1.0/24	trust
host	replication	gpadmin	2001:db8::/32	trust`,
		},
		{
			coordinator: true,
			addrs:       []string{"cdw"},
			confContent: ``,
			expected: `local	all	gpadmin	ident
host	all	gpadmin	localhost	trust
host	all	gpadmin	cdw	trust
local	replication	gpadmin	ident
host	replication	gpadmin	samehost	trust
host	replication	gpadmin	cdw	trust`,
		},
		{
			coordinator: true,
			addrs:       []string{"cdw"},
			confContent: `# foo
foobar
# bar`,
			expected: `# foo
# bar
local	all	gpadmin	ident
host	all	gpadmin	localhost	trust
host	all	gpadmin	cdw	trust
local	replication	gpadmin	ident
host	replication	gpadmin	samehost	trust
host	replication	gpadmin	cdw	trust`,
		},
		{
			coordinator:      false,
			coordinatorAddrs: []string{"192.0.0.0/24"},
			addrs:            []string{"192.0.1.0/24", "2001:db8::/32"},
			confContent:      ``,
			expected: `host	all	all	192.0.0.0/24	trust
host	all	gpadmin	192.0.1.0/24	trust
host	all	gpadmin	2001:db8::/32	trust`,
		},
		{
			coordinator:      false,
			coordinatorAddrs: []string{"cdw"},
			addrs:            []string{"sdw"},
			confContent:      ``,
			expected: `host	all	all	cdw	trust
host	all	gpadmin	sdw	trust`,
		},
		{
			coordinator:      false,
			coordinatorAddrs: []string{"cdw"},
			addrs:            []string{"sdw"},
			confContent: `# foo
foobar
# bar`,
			expected: `# foo
foobar
# bar
host	all	all	cdw	trust
host	all	gpadmin	sdw	trust`,
		},
	}

	for _, tc := range coordinatorCases {
		t.Run("correctly builds the pg_hba.conf file", func(t *testing.T) {
			dname, confPath := createTempConfFile(t, "pg_hba.conf", tc.confContent, 0644)
			defer os.RemoveAll(dname)

			utils.System.CurrentUser = func() (*user.User, error) {
				return &user.User{Username: "gpadmin"}, nil
			}
			defer utils.ResetSystemFunctions()

			var err error
			if tc.coordinator {
				err = postgres.BuildCoordinatorPgHbaConf(dname, tc.addrs)
			} else {
				err = postgres.UpdateSegmentPgHbaConf(dname, tc.coordinatorAddrs, tc.addrs)
			}

			if err != nil {
				t.Fatalf("unexpected error: %#v", err)
			}

			testutils.AssertFileContents(t, confPath, tc.expected)
		})
	}

	failureCases := []struct {
		coordinator bool
	}{
		{
			coordinator: true,
		},
		{
			coordinator: false,
		},
	}

	for _, tc := range failureCases {
		t.Run("errors out when there is no file present", func(t *testing.T) {
			dname, _ := createTempConfFile(t, "", "", 0644)
			defer os.RemoveAll(dname)

			var err error
			expectedErr := os.ErrNotExist
			if tc.coordinator {
				err = postgres.BuildCoordinatorPgHbaConf(dname, []string{"cdw"})
			} else {
				err = postgres.UpdateSegmentPgHbaConf(dname, []string{}, []string{"sdw"})
			}

			if !errors.Is(err, expectedErr) {
				t.Fatalf("got %#v, want %#v", err, expectedErr)
			}
		})

		t.Run("errors out when not able to get the current user", func(t *testing.T) {
			dname, _ := createTempConfFile(t, "pg_hba.conf", "", 0644)
			defer os.RemoveAll(dname)

			expectedErr := errors.New("error")
			utils.System.CurrentUser = func() (*user.User, error) {
				return nil, expectedErr
			}
			defer utils.ResetSystemFunctions()

			var err error
			if tc.coordinator {
				err = postgres.BuildCoordinatorPgHbaConf(dname, []string{"cdw"})
			} else {
				err = postgres.UpdateSegmentPgHbaConf(dname, []string{}, []string{"sdw"})
			}

			if !errors.Is(err, expectedErr) {
				t.Fatalf("got %#v, want %#v", err, expectedErr)
			}
		})

		t.Run("errors out when fails to update the conf file", func(t *testing.T) {
			dname, _ := createTempConfFile(t, "pg_hba.conf", "", 0644)
			defer os.RemoveAll(dname)
			defer utils.ResetSystemFunctions()

			var err error
			expectedErr := errors.New("error")
			if tc.coordinator {
				utils.System.Create = func(name string) (*os.File, error) {
					return nil, expectedErr
				}

				err = postgres.BuildCoordinatorPgHbaConf(dname, []string{"cdw"})
			} else {
				utils.System.OpenFile = func(name string, flag int, perm os.FileMode) (*os.File, error) {
					return nil, expectedErr
				}

				err = postgres.UpdateSegmentPgHbaConf(dname, []string{}, []string{"sdw"})
			}

			if !errors.Is(err, expectedErr) {
				t.Fatalf("got %#v, want %#v", err, expectedErr)
			}
		})
	}
}

func createTempConfFile(t *testing.T, filename, content string, perm fs.FileMode) (string, string) {
	t.Helper()

	dname, err := os.MkdirTemp("", "gpseg")
	if err != nil {
		t.Fatalf("unexpected error: %#v", err)
	}

	filepath := filepath.Join(dname, filename)
	if filename != "" {
		err := os.WriteFile(filepath, []byte(content), perm)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	}

	return dname, filepath
}
