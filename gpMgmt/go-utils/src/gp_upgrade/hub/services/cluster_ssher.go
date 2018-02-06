package services

type ClusterSsher struct {
	checklistWriter ChecklistWriter
}

type ChecklistWriter interface {
	MarkInProgress(string)
}

type ChecklistWriterImpl struct{}

func (c *ChecklistWriterImpl) MarkInProgress(step string) {
	//writes file?
}

func NewChecklistWriterImpl() *ChecklistWriterImpl {
	return &ChecklistWriterImpl{}
}

func NewClusterSsher(cw ChecklistWriter) *ClusterSsher {
	return &ClusterSsher{checklistWriter: cw}
}

func (c *ClusterSsher) VerifySoftware([]string) {
	c.checklistWriter.MarkInProgress("seginstall")
}
