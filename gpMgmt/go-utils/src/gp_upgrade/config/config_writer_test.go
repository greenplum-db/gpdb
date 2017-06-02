package config_test

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"gp_upgrade/config"
	"gp_upgrade/test_utils"
	"io/ioutil"
	"os"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

const (
	temp_home_dir = "/tmp/gp_upgrade_test_temp_home_dir"
)

var (
	saved_old_home string
)

var _ = Describe("configWriter", func() {

	BeforeEach(func() {
		saved_old_home = test_utils.NukeAndSetHomeDir(temp_home_dir)
	})

	AfterEach(func() {
		os.Setenv("HOME", saved_old_home)
	})

	Describe("#NewWriter", func() {
		It("initializes a configuration", func() {
			fakeRows := test_utils.FakeRows{
				FakeColumns: []string{"colname1"},
				NumRows:     1,
				SampleRow:   []string{"value1"},
			}
			subject, err := config.NewWriter(&fakeRows)

			Expect(err).NotTo(HaveOccurred())
			Expect(len(subject.TableJsonData)).To(Equal(1))
			Expect(subject.TableJsonData[0]["colname1"]).To(Equal("value1"))
		})
		Describe("error cases", func() {
			It("is returns an error if rows are empty", func() {
				rows := sql.Rows{}

				_, err := config.NewWriter(&rows)

				Expect(err).To(HaveOccurred())
			})

			It("returns an error if the given rows do not parse via Columns()", func() {
				fakeRows := test_utils.FakeRows{
					FakeColumns: []string{"colname1", "colname2"},
					NumRows:     1,
					SampleRow:   []string{"value1"},
				}
				_, err := config.NewWriter(&fakeRows)
				Expect(err).To(HaveOccurred())
			})
		})
	})

	Describe("#Write", func() {
		const (
			// the output is pretty-printed, so match that format precisely
			expected_json = `[
  {
    "some": "json"
  }
]`
		)

		var (
			json_structure []map[string]interface{}
		)

		BeforeEach(func() {
			err := json.Unmarshal([]byte(expected_json), &json_structure)
			Expect(err).NotTo(HaveOccurred())
		})
		It("writes a configuration when given json", func() {
			subject := config.Writer{
				TableJsonData: json_structure,
				Formatter:     config.NewJsonFormatter(),
				FileWriter:    config.NewRealFileWriter(),
			}
			err := subject.Write()

			Expect(err).NotTo(HaveOccurred())

			content, err := ioutil.ReadFile(temp_home_dir + "/.gp_upgrade/cluster_config.json")
			Expect(err).NotTo(HaveOccurred())
			Expect(expected_json).To(Equal(string(content)))
		})
		Describe("error cases", func() {
			It("returns an error when home directory is not writable", func() {
				os.Chmod(temp_home_dir, 0100)
				subject := config.Writer{
					TableJsonData: json_structure,
					Formatter:     config.NewJsonFormatter(),
					FileWriter:    config.NewRealFileWriter(),
				}
				err := subject.Write()

				Expect(err).To(HaveOccurred())
				Expect(string(err.Error())).To(ContainSubstring(fmt.Sprintf("mkdir %v/.gp_upgrade: permission denied", temp_home_dir)))
			})
			It("returns an error when cluster config file cannot be opened", func() {
				// pre-create the directory with 0100 perms
				upgrade_config_dir := temp_home_dir + "/.gp_upgrade"
				err := os.RemoveAll(upgrade_config_dir)
				Expect(err).NotTo(HaveOccurred())
				os.MkdirAll(upgrade_config_dir, 0100)

				subject := config.Writer{
					TableJsonData: json_structure,
					Formatter:     config.NewJsonFormatter(),
				}
				err = subject.Write()

				Expect(err).To(HaveOccurred())
				Expect(string(err.Error())).To(ContainSubstring(fmt.Sprintf("open %v/.gp_upgrade/cluster_config.json: permission denied", temp_home_dir)))
			})
			It("returns an error when json marshalling fails", func() {
				myMap := make(map[string]interface{})
				myMap["foo"] = make(chan int) // there is no json representation for a channel
				malformed_json_structure := []map[string]interface{}{
					0: myMap,
				}
				subject := config.Writer{
					TableJsonData: malformed_json_structure,
					Formatter:     config.NewJsonFormatter(),
				}
				err := subject.Write()

				Expect(err).To(HaveOccurred())
			})

			It("returns an error when json pretty print fails", func() {
				subject := config.Writer{
					TableJsonData: json_structure,
					Formatter:     &test_utils.ErrorFormatter{},
				}
				err := subject.Write()

				Expect(err).To(HaveOccurred())
			})

			It("returns an error when file writing fails", func() {
				subject := config.Writer{
					TableJsonData: json_structure,
					Formatter:     &test_utils.NilFormatter{},
					FileWriter:    &test_utils.ErrorFileWriterDuringWrite{},
				}
				err := subject.Write()

				Expect(err).To(HaveOccurred())
			})

		})
	})
})
