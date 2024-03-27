package greenplum_test

import (
	"database/sql/driver"
	"errors"
	"os/user"
	"reflect"
	"regexp"
	"testing"

	sqlmock "github.com/DATA-DOG/go-sqlmock"

	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/greenplum"
)

func TestConnectDatabase(t *testing.T) {
	t.Run("connect to database successfully", func(t *testing.T) {
		expectedConn, _, err := testutils.CreateMockDBConn()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		conn, err := greenplum.ConnectDatabase("testhost", 5432)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		if !(conn.DBName == expectedConn.DBName &&
			conn.User == expectedConn.User &&
			conn.Host == expectedConn.Host &&
			conn.Port == expectedConn.Port) {
			t.Fatalf("got %+v, want %+v", conn, expectedConn)
		}
	})

	t.Run("unable to get current user", func(t *testing.T) {
		expectedErr := errors.New("unable to get Current User")
		utils.System.CurrentUser = func() (*user.User, error) {
			return nil, expectedErr
		}
		defer utils.ResetSystemFunctions()

		_, err := greenplum.ConnectDatabase("testhost", 7000)
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("connect to database failed", func(t *testing.T) {
		utils.System.CurrentUser = func() (*user.User, error) {
			return &user.User{Username: "testdb"}, nil
		}
		defer utils.ResetSystemFunctions()

		_, err := greenplum.ConnectDatabase("testhost", 5432)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
}

func TestReadGpSegmentConfig(t *testing.T) {
	t.Run("empty gp_segment_configuration table", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		header := []string{""}
		fakeRows := sqlmock.NewRows(header)
		sqlMock.ExpectQuery("SELECT dbid, content, role, preferred_role, mode, status, port, datadir, hostname, address " +
			"FROM pg_catalog.gp_segment_configuration ORDER BY content ASC, role DESC;").WillReturnRows(fakeRows)

		expectedErr := errors.New("Empty gp_segment_configuration")

		gpArray := greenplum.NewGpArray()
		err = gpArray.ReadGpSegmentConfig(mockConnection)
		if errors.Is(err, expectedErr) {
			t.Fatalf("got %#v, want %#v", err, expectedErr)
		}
	})

	t.Run("ReadGpSegmentConfig Succeeds", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		header := []string{"dbid", "contentid", "role", "pref_role", "mode", "state", "port", "data_dir", "hostname", "hostaddr"}
		rowOne := []driver.Value{"1", "-1", "p", "p", "n", "u", "7000", "/tmp/demo/0", "temp.com", "temp.com"}
		rowTwo := []driver.Value{"2", "0", "p", "p", "s", "u", "7002", "/tmp/demo/1", "temp.com", "temp.com"}
		rowThree := []driver.Value{"3", "1", "p", "p", "s", "u", "7003", "/tmp/demo/2", "temp.com", "temp.com"}
		rowFour := []driver.Value{"4", "3", "p", "p", "s", "u", "7004", "/tmp/demo/3", "temp.com", "temp.com"}
		fakeRows := sqlmock.NewRows(header).AddRow(rowOne...).AddRow(rowTwo...).AddRow(rowThree...).AddRow(rowFour...)
		sqlMock.ExpectQuery("SELECT dbid, content, role, preferred_role, mode, status, port, datadir, hostname, address " +
			"FROM pg_catalog.gp_segment_configuration ORDER BY content ASC, role DESC;").WillReturnRows(fakeRows)

		result := []greenplum.Segment{
			{1, -1, "p", "p", "n", "u", 7000, "temp.com", "temp.com", "/tmp/demo/0"},
			{2, 0, "p", "p", "s", "u", 7002, "temp.com", "temp.com", "/tmp/demo/1"},
			{3, 1, "p", "p", "s", "u", 7003, "temp.com", "temp.com", "/tmp/demo/2"},
			{4, 3, "p", "p", "s", "u", 7004, "temp.com", "temp.com", "/tmp/demo/3"},
		}
		gpArray := greenplum.NewGpArray()
		_ = gpArray.ReadGpSegmentConfig(mockConnection)
		if !reflect.DeepEqual(gpArray.Segments, result) {
			t.Fatalf("got %v, want %v", gpArray.Segments, result)
		}
	})
}

func TestGpArray_GetPrimarySegments(t *testing.T) {
	t.Run("GetPrimarySegments succeeds", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		header := []string{"dbid", "contentid", "role", "pref_role", "mode", "state", "port", "data_dir", "hostname", "hostaddr"}
		rowOne := []driver.Value{"1", "-1", "p", "p", "n", "u", "7000", "/tmp/demo/0", "temp.com", "temp.com"}
		rowTwo := []driver.Value{"2", "0", "p", "p", "s", "u", "7002", "/tmp/demo/1", "temp.com", "temp.com"}
		rowThree := []driver.Value{"3", "1", "p", "p", "s", "u", "7003", "/tmp/demo/2", "temp.com", "temp.com"}
		rowFour := []driver.Value{"4", "3", "p", "p", "s", "u", "7004", "/tmp/demo/3", "temp.com", "temp.com"}
		fakeRows := sqlmock.NewRows(header).AddRow(rowOne...).AddRow(rowTwo...).AddRow(rowThree...).AddRow(rowFour...)
		sqlMock.ExpectQuery("SELECT dbid, content, role, preferred_role, mode, status, port, datadir, hostname, address " +
			"FROM pg_catalog.gp_segment_configuration ORDER BY content ASC, role DESC;").WillReturnRows(fakeRows)

		expResult := []greenplum.Segment{
			{2, 0, "p", "p", "s", "u", 7002, "temp.com", "temp.com", "/tmp/demo/1"},
			{3, 1, "p", "p", "s", "u", 7003, "temp.com", "temp.com", "/tmp/demo/2"},
			{4, 3, "p", "p", "s", "u", 7004, "temp.com", "temp.com", "/tmp/demo/3"},
		}
		gpArray := greenplum.NewGpArray()
		_ = gpArray.ReadGpSegmentConfig(mockConnection)

		result, _ := gpArray.GetPrimarySegments()
		if !reflect.DeepEqual(expResult, result) {
			t.Fatalf("got %v, want %v", result, expResult)
		}

	})

	t.Run("Empty gp_segment_configuration table", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		header := []string{""}
		fakeRows := sqlmock.NewRows(header)
		sqlMock.ExpectQuery("SELECT dbid, content, role, preferred_role, mode, status, port, datadir, hostname, address " +
			"FROM pg_catalog.gp_segment_configuration ORDER BY content ASC, role DESC;").WillReturnRows(fakeRows)

		gpArray := greenplum.NewGpArray()
		err = gpArray.ReadGpSegmentConfig(mockConnection)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expErr := errors.New("unable to find primary segments")
		_, err = gpArray.GetPrimarySegments()
		if errors.Is(err, expErr) {
			t.Fatalf("got %#v, want %#v", err, expErr)
		}
	})
}

func Test_RegisterPrimaries(t *testing.T) {
	t.Run("Failed to add primaries to pg_catalog table", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		err = errors.New("Unexpected error")

		sqlMock.ExpectExec(regexp.QuoteMeta("SELECT pg_catalog.gp_add_segment_primary( 'temp.com', 'temp.com', 7000, '/temp/seg/0');")).
			WillReturnError(err)

		var segs = []*idl.Segment{
			{
				Port:          7000,
				HostName:      "temp.com",
				HostAddress:   "temp.com",
				DataDirectory: "/temp/seg/0",
			},
		}
		err = greenplum.RegisterPrimaries(segs, mockConnection)
		if err == nil {
			t.Fatalf("Unexpected error")
		}
	})

	t.Run("Succeeded in adding Primaries in pg_catalog table but failed to update contentid", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		err = errors.New("Unexpected error")
		sqlMock.ExpectExec(regexp.QuoteMeta("SELECT pg_catalog.gp_add_segment_primary( 'temp.com', 'temp.com', 7000, '/temp/seg/0');")).
			WillReturnResult(sqlmock.NewResult(1, 1))
		sqlMock.ExpectExec("SET allow_system_table_mods=true; UPDATE gp_segment_configuration SET content = content - 1 where content > 0;").
			WillReturnError(err)

		var segs = []*idl.Segment{
			{
				Port:          7000,
				HostName:      "temp.com",
				HostAddress:   "temp.com",
				DataDirectory: "/temp/seg/0",
			},
		}
		err = greenplum.RegisterPrimaries(segs, mockConnection)
		if err == nil {
			t.Fatalf("Unexpected error")
		}
	})

	t.Run("Updated primaries in pg_catalog and updated contentid successfully", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		sqlMock.ExpectExec(regexp.QuoteMeta("SELECT pg_catalog.gp_add_segment_primary( 'temp.com', 'temp.com', 7000, '/temp/seg/0');")).
			WillReturnResult(sqlmock.NewResult(1, 1))
		sqlMock.ExpectExec("SET allow_system_table_mods=true; UPDATE gp_segment_configuration SET content = content - 1 where content > 0;").
			WillReturnResult(sqlmock.NewResult(1, 1))

		var segs = []*idl.Segment{
			{
				Port:          7000,
				HostName:      "temp.com",
				HostAddress:   "temp.com",
				DataDirectory: "/temp/seg/0",
			},
		}
		err = greenplum.RegisterPrimaries(segs, mockConnection)
		if err != nil {
			t.Fatalf("Unexpected error")
		}
	})

}

func Test_RegisterCoordinator(t *testing.T) {
	t.Run("Failed to add coordinator to pg_catalog table", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		err = errors.New("Unexpected error")

		sqlMock.ExpectExec(regexp.QuoteMeta("SELECT pg_catalog.gp_add_segment(1::int2, -1::int2, 'p', 'p', 's', 'u', '7000', 'temp.com', 'temp.com', '/temp/seg/0')")).
			WillReturnError(err)

		var segs idl.Segment
		err = greenplum.RegisterCoordinator(&segs, mockConnection)
		if err == nil {
			t.Fatalf("Unexpected error")
		}
	})

	t.Run("Succeded in adding coordinator in pg_catalog table", func(t *testing.T) {
		mockConnection, sqlMock, err := testutils.CreateAndConnectMockDB(1)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		sqlMock.ExpectExec(regexp.QuoteMeta("SELECT pg_catalog.gp_add_segment(1::int2, -1::int2, 'p', 'p', 's', 'u', '7000', 'temp.com', 'temp.com', '/temp/seg/0')")).
			WillReturnResult(sqlmock.NewResult(1, 1))

		var segs = idl.Segment{
			Port:          7000,
			HostName:      "temp.com",
			HostAddress:   "temp.com",
			DataDirectory: "/temp/seg/0",
		}
		err = greenplum.RegisterCoordinator(&segs, mockConnection)
		if err != nil {
			t.Fatalf("Unexpected error")
		}

	})
}
