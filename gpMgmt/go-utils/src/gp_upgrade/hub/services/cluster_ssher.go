package services

import (
	"fmt"
	"gp_upgrade/hub/logger"
	"gp_upgrade/utils"
	"os"
	"path/filepath"
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
	err := c.checklistWriter.ResetStateDir("seginstall")
	if err != nil {
		c.logger.Error <- err.Error()
		//For MMVP, return here, but maybe should log more info
		return
	}
	err = c.checklistWriter.MarkInProgress("seginstall")
	if err != nil {
		c.logger.Error <- err.Error()
		//For MMVP, return here, but maybe should log more info
		return
	}
	//default assumption: GPDB is installed on the same path on all hosts in cluster
	//we're looking for gp_upgrade_agent as proof that the new binary is installed
	//TODO: if this finds nothing, should we err out? do a fallback check based on $GPHOME?
	hubPath, _ := os.Executable()
	agentPath := filepath.Join(filepath.Dir(hubPath), "gp_upgrade_agent")
	var anyFailed = false
	for _, hostname := range hostnames {
		output, err := utils.System.ExecCmdCombinedOutput("ssh",
			"-o",
			"StrictHostKeyChecking=no",
			hostname,
			"ls",
			agentPath,
		)
		if err != nil {
			c.logger.Error <- string(output)
			c.logger.Error <- fmt.Sprintf("didn't find %s on %s", agentPath, hostname)
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
