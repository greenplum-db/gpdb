package upgradestatus_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"

	"errors"
	"gp_upgrade/hub/upgradestatus"
	"gp_upgrade/utils"
	"io/ioutil"
	"os"
	"path/filepath"
)

var _ = Describe("upgradestatus/ChecklistManager", func() {
	AfterEach(func() {
		utils.System = utils.InitializeSystemFunctions()
	})
	Describe("MarkInProgress", func() {
		It("Leaves an in-progress file in the state dir", func() {
			tempdir, _ := ioutil.TempDir("", "")

			cm := upgradestatus.NewChecklistManager(filepath.Join(tempdir, ".gp_upgrade"))
			cm.ResetStateDir("fancy_step")
			err := cm.MarkInProgress("fancy_step")
			Expect(err).ToNot(HaveOccurred())
			expectedFile := filepath.Join(tempdir, ".gp_upgrade", "fancy_step", "in.progress")
			_, err = os.Stat(expectedFile)
			Expect(err).ToNot(HaveOccurred())
		})

		It("still succeeds if file already exists", func() {
			tempdir, _ := ioutil.TempDir("", "")

			cm := upgradestatus.NewChecklistManager(filepath.Join(tempdir, ".gp_upgrade"))
			cm.ResetStateDir("fancy_step")
			cm.MarkInProgress("fancy_step") // lay the file down once
			err := cm.MarkInProgress("fancy_step")
			Expect(err).ToNot(HaveOccurred())
			expectedFile := filepath.Join(tempdir, ".gp_upgrade", "fancy_step", "in.progress")
			_, err = os.Stat(expectedFile)
			Expect(err).ToNot(HaveOccurred())
		})

		It("errors if file opening fails, e.g. disk full", func() {
			utils.System.OpenFile = func(_ string, _ int, _ os.FileMode) (*os.File, error) {
				return nil, errors.New("Disk full or something")
			}

			tempdir, _ := ioutil.TempDir("", "")

			cm := upgradestatus.NewChecklistManager(filepath.Join(tempdir, ".gp_upgrade"))
			cm.ResetStateDir("fancy_step")
			err := cm.MarkInProgress("fancy_step")
			Expect(err).To(HaveOccurred())
		})
	})

	Describe("ResetStateDir", func() {
		It("errors if existing files cant be deleted", func() {
			utils.System.RemoveAll = func(name string) error {
				return errors.New("cant remove all")
			}
			cm := upgradestatus.NewChecklistManager("/some/random/dir")
			err := cm.ResetStateDir("fancy_step")
			Expect(err).To(HaveOccurred())
		})

		It("errors if making the directory fails", func() {
			utils.System.RemoveAll = func(name string) error {
				return nil
			}
			utils.System.MkdirAll = func(string, os.FileMode) error {
				return errors.New("cant make dir")
			}
			cm := upgradestatus.NewChecklistManager("/some/random/dir")
			err := cm.ResetStateDir("fancy_step")
			Expect(err).To(HaveOccurred())
		})
		It("succeeds as long as we assume the file system calls do their job", func() {
			utils.System.RemoveAll = func(name string) error {
				return nil
			}
			utils.System.MkdirAll = func(string, os.FileMode) error {
				return nil
			}
			cm := upgradestatus.NewChecklistManager("/some/random/dir")
			err := cm.ResetStateDir("fancy_step")
			Expect(err).ToNot(HaveOccurred())
		})
	})
})
