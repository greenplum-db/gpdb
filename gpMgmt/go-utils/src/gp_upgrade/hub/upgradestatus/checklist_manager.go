package upgradestatus

import (
	"gp_upgrade/utils"
	"path"

	"os"
	"path/filepath"
)

type ChecklistManager struct {
	pathToStateDir string
}

func (c *ChecklistManager) MarkFailed(step string) error {
	err := utils.System.Remove(filepath.Join(c.pathToStateDir, step, "in.progress"))
	if err != nil {
		return err
	}
	_, err = utils.System.OpenFile(path.Join(c.pathToStateDir, step, "failed"), os.O_CREATE, 0700)
	if err != nil {
		return err
	}

	return nil
}

func (c *ChecklistManager) MarkComplete(step string) error {
	err := utils.System.Remove(filepath.Join(c.pathToStateDir, step, "in.progress"))
	if err != nil {
		return err
	}
	_, err = utils.System.OpenFile(path.Join(c.pathToStateDir, step, "completed"), os.O_CREATE, 0700)
	if err != nil {
		return err
	}

	return nil
}

func (c *ChecklistManager) MarkInProgress(step string) error {
	_, err := utils.System.OpenFile(path.Join(c.pathToStateDir, step, "in.progress"), os.O_CREATE, 0700)
	if err != nil {
		return err
	}

	return nil
}

func (c *ChecklistManager) ResetStateDir(step string) error {
	stepSpecificStateDir := path.Join(c.pathToStateDir, step)
	err := utils.System.RemoveAll(stepSpecificStateDir)
	if err != nil {
		return err
	}
	err = utils.System.MkdirAll(stepSpecificStateDir, 0700)
	if err != nil {
		return err
	}
	return nil

}

func NewChecklistManager(stateDirPath string) *ChecklistManager {
	return &ChecklistManager{pathToStateDir: stateDirPath}
}
