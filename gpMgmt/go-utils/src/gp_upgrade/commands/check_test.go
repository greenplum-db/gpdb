package commands_test

import (
	"database/sql"
	"os"

	"gp_upgrade/utils"

	"io/ioutil"

	"encoding/json"

	_ "github.com/mattn/go-sqlite3"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"
)

const (
	sqlite3_database_path = "/tmp/gp_upgrade_test_sqlite.db"
)

var _ = Describe("check", func() {

	BeforeEach(func() {
		// todo this setup just necessary once; put in before suite?
		// clean any prior db
		err := ioutil.WriteFile(sqlite3_database_path, []byte(""), 0644)
		utils.Check("cannot delete sqlite config", err)

		db, err := sql.Open("sqlite3", sqlite3_database_path)
		utils.Check("cannot open sqlite config", err)
		defer db.Close()

		segment_fixture_sql, err := ioutil.ReadFile(os.Getenv("GOPATH") + "/src/gp_upgrade/commands/fixtures/segment_config.sql")
		utils.Check("cannot open fixture:", err)

		_, err = db.Exec(string(segment_fixture_sql))
		utils.Check("cannot run sqlite config", err)

		err = os.RemoveAll(jsonFilePath())
		utils.Check("cannot remove json file", err)
	})
	AfterEach(func() {
		err := os.RemoveAll(sqlite3_database_path)
		utils.Check("Cannot remove sqllite database file", err)
	})
	Describe("the database is running, master_host is provided, and connection is successful", func() {
		It("writes a file to ~/.gp_upgrade/cluster_config.json with correct json", func() {

			session := runCommand("check", "--master_host", "localhost", "--database_type", "sqlite3", "--database_config_file", sqlite3_database_path)

			Eventually(session).Should(Exit(0))

			content, err := ioutil.ReadFile(jsonFilePath())
			Expect(err).NotTo(HaveOccurred())
			expectedJson, err := ioutil.ReadFile(os.Getenv("GOPATH") + "/src/gp_upgrade/commands/fixtures/segment_config.json")
			Expect(expectedJson).To(Equal(content))

			var json_structure []map[string]interface{}
			err = json.Unmarshal(content, &json_structure)
			Expect(err).NotTo(HaveOccurred())
		})

	})
})

func jsonFilePath() string {
	return os.Getenv("HOME") + "/.gp_upgrade/cluster_config.json"
}
