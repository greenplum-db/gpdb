package services

import (
	"gp_upgrade/hub/logger"
	"gp_upgrade/utils"
)

type ClusterSsher struct {
	checklistWriter ChecklistWriter
	logger          logger.LogEntry
}

type ChecklistWriter interface {
	MarkInProgress(string) error
	ResetStateDir(string) error
	MarkFailed(string) error
	MarkComplete(string) error
}

func NewClusterSsher(cw ChecklistWriter, logger logger.LogEntry) *ClusterSsher {
	return &ClusterSsher{checklistWriter: cw, logger: logger}
}

func (c *ClusterSsher) VerifySoftware(hostnames []string) {
	c.checklistWriter.ResetStateDir("seginstall")
	err := c.checklistWriter.MarkInProgress("seginstall")
	if err != nil {
		c.logger.Error <- err.Error()
	}
	var anyFailed = false
	for _, hostname := range hostnames {
		_, err := utils.System.ExecCmdOutput("ssh", hostname)
		if err != nil {
			c.logger.Error <- err.Error()
			anyFailed = true
		}
	}
	if anyFailed {
		err = c.checklistWriter.MarkFailed("seginstall")
		if err != nil {
			c.logger.Error <- err.Error()
		}
		return
	}
	err = c.checklistWriter.MarkComplete("seginstall")
	if err != nil {
		c.logger.Error <- err.Error()
	}
}
