package greenplum

import (
	"errors"
	"fmt"

	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"

	"github.com/greenplum-db/gp-common-go-libs/dbconn"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/utils"
)

type Segment struct {
	Dbid          int
	ContentId     int
	Role          string
	PreferredRole string
	Mode          string
	Status        string
	Port          int
	HostName      string
	HostAddress   string
	DataDir       string
}

func (seg *Segment) isSegmentActingPrimary() bool {
	return seg.ContentId >= 0 && seg.Role == constants.RolePrimary
}

type GpArray struct {
	Segments []Segment
}

func NewGpArray() *GpArray {
	return &GpArray{}
}

func ConnectDatabase(host string, port int) (*dbconn.DBConn, error) {
	user, err := utils.System.CurrentUser()
	if err != nil {
		return nil, err
	}
	conn := dbconn.NewDBConn(constants.DefaultDatabase, user.Username, host, port)

	return conn, nil
}

func (gpArray *GpArray) ReadGpSegmentConfig(conn *dbconn.DBConn) error {
	query := "SELECT dbid, content, role, preferred_role, mode, status, port, datadir, hostname, address " +
		"FROM pg_catalog.gp_segment_configuration ORDER BY content ASC, role DESC;"

	rows, _ := conn.Query(query)
	defer rows.Close()

	result, _ := buildGpArray(rows)
	gpArray.Segments = result
	return nil
}

func buildGpArray(rows *sqlx.Rows) ([]Segment, error) {

	result := []Segment{}

	for rows.Next() {
		dest := Segment{}

		if rErr := rows.Scan(
			&dest.Dbid,
			&dest.ContentId,
			&dest.Role,
			&dest.PreferredRole,
			&dest.Mode,
			&dest.Status,
			&dest.Port,
			&dest.DataDir,
			&dest.HostName,
			&dest.HostAddress,
		); rErr != nil {
			return nil, rErr
		}

		result = append(result, dest)
	}
	return result, nil
}

func (gpArray *GpArray) GetPrimarySegments() ([]Segment, error) {

	var result []Segment

	for _, seg := range gpArray.Segments {
		if seg.isSegmentActingPrimary() {
			result = append(result, seg)
		}
	}
	if len(result) == 0 {
		err := errors.New("unable to find primary segments")
		return nil, err
	}
	return result, nil
}

func RegisterPrimaries(segs []*idl.Segment, conn *dbconn.DBConn) error {

	addPrimaryQuery := "SELECT pg_catalog.gp_add_segment_primary( '%s', '%s', %d, '%s');"
	for _, seg := range segs {
		addSegmentQuery := fmt.Sprintf(addPrimaryQuery, seg.HostName, seg.HostAddress, seg.Port, seg.DataDirectory)

		_, err := conn.Exec(addSegmentQuery)
		if err != nil {
			return err
		}
	}

	// FIXME: gp_add_segment_primary() starts the content ID from 1,
	// so manually update the correct values for now.
	updateContentIdQuery := "SET allow_system_table_mods=true; UPDATE gp_segment_configuration SET content = content - 1 where content > 0;"
	_, err := conn.Exec(updateContentIdQuery)
	if err != nil {
		return err
	}

	return nil
}

func RegisterCoordinator(seg *idl.Segment, conn *dbconn.DBConn) error {

	addCoordinatorQuery := "SELECT pg_catalog.gp_add_segment(1::int2, -1::int2, 'p', 'p', 's', 'u', '%d', '%s', '%s', '%s')"
	_, err := conn.Exec(fmt.Sprintf(addCoordinatorQuery, seg.Port, seg.HostName, seg.HostAddress, seg.DataDirectory))
	if err != nil {
		return err
	}
	return nil
}
