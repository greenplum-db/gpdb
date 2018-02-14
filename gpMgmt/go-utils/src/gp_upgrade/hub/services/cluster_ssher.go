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
	hubPath, _ := os.Executable()
	agentPath := filepath.Join(filepath.Dir(hubPath), "gp_upgrade_agent")
	c.remoteExec(hostnames, "seginstall", []string{"ls", agentPath})
}

func (c *ClusterSsher) Start(hostnames []string) {
	// ssh -o "StrictHostKeyChecking=no" hostname /path/to/gp_upgrade_agent
	hubPath, _ := os.Executable()
	agentPath := filepath.Join(filepath.Dir(hubPath), "gp_upgrade_agent")
	//ssh -n -f user@host "sh -c 'cd /whereever; nohup ./whatever > /dev/null 2>&1 &'"
	completeCommandString := fmt.Sprintf("sh -c 'nohup %s > /dev/null 2>&1 & '", agentPath)
	c.remoteExec(hostnames, "start-agents", []string{completeCommandString})
}

func (c *ClusterSsher) remoteExec(hostnames []string, statedir string, command []string) {
	err := c.checklistWriter.ResetStateDir(statedir)
	if err != nil {
		c.logger.Error <- err.Error()
		//For MMVP, return here, but maybe should log more info
		return
	}
	err = c.checklistWriter.MarkInProgress(statedir)
	if err != nil {
		c.logger.Error <- err.Error()
		//For MMVP, return here, but maybe should log more info
		return
	}
	//default assumption: GPDB is installed on the same path on all hosts in cluster
	//we're looking for gp_upgrade_agent as proof that the new binary is installed
	//TODO: if this finds nothing, should we err out? do a fallback check based on $GPHOME?
	var anyFailed = false
	for _, hostname := range hostnames {
		sshArgs := []string{"-o", "StrictHostKeyChecking=no", hostname}
		sshArgs = append(sshArgs, command...)
		output, err := utils.System.ExecCmdCombinedOutput("ssh", sshArgs...)
		if err != nil {
			c.logger.Error <- string(output)
			c.logger.Error <- fmt.Sprintf("Couldn't run %s on %s", command, hostname)
			anyFailed = true
		}
	}
	if anyFailed {
		err = c.checklistWriter.MarkFailed(statedir)
		if err != nil {
			c.logger.Error <- err.Error()
		}
		return
	}
	err = c.checklistWriter.MarkComplete(statedir)
	if err != nil {
		c.logger.Error <- err.Error()
	}
}
