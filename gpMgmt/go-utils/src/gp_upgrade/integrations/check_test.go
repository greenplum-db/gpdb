package integrations_test

import (
	"database/sql"
	"os"

	"gp_upgrade/test_utils"

	"io/ioutil"

	"encoding/json"

	"gp_upgrade/config"

	"path"
	"runtime"

	_ "github.com/mattn/go-sqlite3"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"
)

const (
	sqlite3_database_path = "/tmp/gp_upgrade_test_sqlite.db"
)

var _ = Describe("check", func() {

	var fixture_path string
	BeforeEach(func() {
		_, this_file_path, _, _ := runtime.Caller(0)
		fixture_path = path.Join(path.Dir(this_file_path), "fixtures")
	})

	AfterEach(func() {
		err := os.RemoveAll(sqlite3_database_path)
		test_utils.Check("Cannot remove sqllite database file", err)
	})
	Describe("object-count", func() {
		Describe("happy: the object count prints a count of append-optimized and heap objects", func() {
			It("prints the count of append-optimized and heap objects to stdout", func() {
				// queries the database to get the count and then prints that to the command line

				object_count_path := path.Join(fixture_path, "object_count.sql")
				setupSqlite3Database(getFileContents(object_count_path))
				session := runCommand("check", "object-count", "--master-host",
					"localhost", "--master-port", "15432", "--database_type",
					"sqlite3", "--database_config_file", sqlite3_database_path)

				Eventually(session).Should(Exit(0))

				Expect(string(session.Out.Contents())).To(ContainSubstring(`Number of AO objects - 2`))
				Expect(string(session.Out.Contents())).To(ContainSubstring(`Number of heap objects - 1`))
			})
		})
	})
	Describe("happy: the database is running, master-host is provided, and connection is successful", func() {
		Context("check", func() {
			It("writes a file to ~/.gp_upgrade/cluster_config.json with correct json", func() {
				config_path := path.Join(fixture_path, "segment_config.sql")
				setupSqlite3Database(getFileContents(config_path))

				session := runCommand("check", "--master-host", "localhost", "--database_type", "sqlite3", "--database_config_file", sqlite3_database_path)

				Eventually(session).Should(Exit(0))
				content, err := ioutil.ReadFile(config.GetConfigFilePath())
				Expect(err).NotTo(HaveOccurred())
				expectedJson, err := ioutil.ReadFile(path.Join(fixture_path, "segment_config.json"))
				Expect(err).NotTo(HaveOccurred())
				Expect(expectedJson).To(Equal(content))
				var json_structure []map[string]interface{}
				err = json.Unmarshal(content, &json_structure)
				Expect(err).NotTo(HaveOccurred())
			})
		})
	})

	Describe("error cases", func() {
		Describe("the database cannot be opened", func() {
			Context("check", func() {
				It("returns error", func() {
					session := runCommand("check", "--master-host", "localhost", "--database_type", "foo", "--database_config_file", "bar")

					Eventually(session).Should(Exit(65))
					Expect(string(session.Err.Contents())).To(ContainSubstring(`sql: unknown driver "foo" (forgotten import?)`))
				})
			})
			Context("check version", func() {
				It("returns database connection error", func() {
					session := runCommand("check", "version", "--master-host", "localhost", "--database-name", "invalidDB")

					Eventually(session).Should(Exit(65))
					Expect(string(session.Err.Contents())).To(ContainSubstring(`Database Connection Error: pq: database "invalidDB" does not exist`))
				})
			})

		})
		Describe("the database query fails", func() {
			It("returns error", func() {
				session := runCommand("check", "--master-host", "localhost", "--database_type", "sqlite3", "--database_config_file", sqlite3_database_path)

				Eventually(session).Should(Exit(1))
				Expect(string(session.Err.Contents())).To(ContainSubstring(`no such table: gp_segment_configuration`))
			})
		})
	})
})

func setupSqlite3Database(inputSql string) {
	// clean any prior db
	err := ioutil.WriteFile(sqlite3_database_path, []byte(""), 0644)
	test_utils.Check("cannot delete sqlite config", err)

	db, err := sql.Open("sqlite3", sqlite3_database_path)
	test_utils.Check("cannot open sqlite config", err)
	defer db.Close()

	_, err = db.Exec(inputSql)
	test_utils.Check("cannot run sqlite config", err)

	err = os.RemoveAll(config.GetConfigFilePath())
	test_utils.Check("cannot remove json file", err)
}

func getFileContents(path string) string {
	segment_fixture_sql, err := ioutil.ReadFile(path)
	test_utils.Check("cannot open fixture:", err)
	return string(segment_fixture_sql)
}
