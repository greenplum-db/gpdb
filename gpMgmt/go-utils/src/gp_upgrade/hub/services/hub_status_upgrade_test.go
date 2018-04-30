package services_test

import (
	"errors"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

	"gp_upgrade/hub/services"
	pb "gp_upgrade/idl"
	"gp_upgrade/testutils"
	"gp_upgrade/utils"

	"google.golang.org/grpc"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("status upgrade", func() {
	var (
		hub                      *services.HubClient
		fakeStatusUpgradeRequest *pb.StatusUpgradeRequest
		dir                      string
		commandExecer            *testutils.FakeCommandExecer
		errChan                  chan error
		outChan                  chan []byte
		mockAgent                *testutils.MockAgentServer
	)

	BeforeEach(func() {
		reader := testutils.SpyReader{}
		reader.Hostnames = []string{"localhost"}

		var port int
		mockAgent, port = testutils.NewMockAgentServer()
		mockAgent.StatusConversionResponse = &pb.CheckConversionStatusReply{}

		var err error
		dir, err = ioutil.TempDir("", "")
		Expect(err).ToNot(HaveOccurred())
		conf := &services.HubConfig{
			HubToAgentPort: port,
			StateDir:       dir,
		}

		errChan = make(chan error, 2)
		outChan = make(chan []byte, 2)

		commandExecer = &testutils.FakeCommandExecer{}
		commandExecer.SetOutput(&testutils.FakeCommand{
			Err: errChan,
			Out: outChan,
		})
		hub = services.NewHub(nil, &reader, grpc.DialContext, commandExecer.Exec, conf)
	})

	AfterEach(func() {
		utils.System = utils.InitializeSystemFunctions()
		os.RemoveAll(dir)
	})

	It("responds with the statuses of the steps based on files on disk", func() {
		setStateFile(dir, "seginstall", "completed")
		setStateFile(dir, "share-oids", "failed")

		f, err := os.Create(filepath.Join(dir, "cluster_config.json"))
		Expect(err).ToNot(HaveOccurred())
		f.Close()

		mockAgent.StatusConversionResponse = &pb.CheckConversionStatusReply{
			Statuses: []string{"RUNNING", "PENDING"},
		}

		resp, err := hub.StatusUpgrade(nil, &pb.StatusUpgradeRequest{})
		Expect(err).To(BeNil())

		Expect(resp.ListOfUpgradeStepStatuses).To(ConsistOf(
			[]*pb.UpgradeStepStatus{
				{
					Step:   pb.UpgradeSteps_CHECK_CONFIG,
					Status: pb.StepStatus_COMPLETE,
				}, {
					Step:   pb.UpgradeSteps_PREPARE_INIT_CLUSTER,
					Status: pb.StepStatus_PENDING,
				}, {
					Step:   pb.UpgradeSteps_SEGINSTALL,
					Status: pb.StepStatus_COMPLETE,
				}, {
					Step:   pb.UpgradeSteps_STOPPED_CLUSTER,
					Status: pb.StepStatus_PENDING,
				}, {
					Step:   pb.UpgradeSteps_MASTERUPGRADE,
					Status: pb.StepStatus_PENDING,
				}, {
					Step:   pb.UpgradeSteps_PREPARE_START_AGENTS,
					Status: pb.StepStatus_PENDING,
				}, {
					Step:   pb.UpgradeSteps_SHARE_OIDS,
					Status: pb.StepStatus_FAILED,
				}, {
					Step:   pb.UpgradeSteps_VALIDATE_START_CLUSTER,
					Status: pb.StepStatus_PENDING,
				}, {
					Step:   pb.UpgradeSteps_CONVERT_PRIMARIES,
					Status: pb.StepStatus_RUNNING,
				}, {
					Step:   pb.UpgradeSteps_RECONFIGURE_PORTS,
					Status: pb.StepStatus_PENDING,
				},
			},
		))
	})

	// TODO: Get rid of these tests once full rewritten test coverage exists
	Describe("creates a reply", func() {
		It("sends status messages under good condition", func() {
			formulatedResponse, err := hub.StatusUpgrade(nil, fakeStatusUpgradeRequest)
			Expect(err).To(BeNil())
			countOfStatuses := len(formulatedResponse.GetListOfUpgradeStepStatuses())
			Expect(countOfStatuses).ToNot(BeZero())
		})

		It("reports that prepare start-agents is pending", func() {
			utils.System.FilePathGlob = func(string) ([]string, error) {
				return []string{"somefile"}, nil
			}

			var fakeStatusUpgradeRequest *pb.StatusUpgradeRequest

			formulatedResponse, err := hub.StatusUpgrade(nil, fakeStatusUpgradeRequest)
			Expect(err).To(BeNil())

			stepStatuses := formulatedResponse.GetListOfUpgradeStepStatuses()

			var stepStatusSaved *pb.UpgradeStepStatus
			for _, stepStatus := range stepStatuses {

				if stepStatus.GetStep() == pb.UpgradeSteps_PREPARE_START_AGENTS {
					stepStatusSaved = stepStatus
				}
			}
			Expect(stepStatusSaved.GetStep()).ToNot(BeZero())
			Expect(stepStatusSaved.GetStatus()).To(Equal(pb.StepStatus_PENDING))
		})

		It("reports that prepare start-agents is running and then complete", func() {
			var numInvocations int
			utils.System.FilePathGlob = func(input string) ([]string, error) {
				numInvocations += 1
				if numInvocations == 1 {
					return []string{filepath.Join(filepath.Dir(input), "in.progress")}, nil
				}
				return []string{filepath.Join(filepath.Dir(input), "completed")}, nil
			}
			utils.System.Stat = func(name string) (os.FileInfo, error) {
				return nil, nil
			}
			pollStatusUpgrade := func() pb.StepStatus {
				response, _ := hub.StatusUpgrade(nil, &pb.StatusUpgradeRequest{})

				stepStatuses := response.GetListOfUpgradeStepStatuses()

				var stepStatusSaved *pb.UpgradeStepStatus
				for _, stepStatus := range stepStatuses {

					if stepStatus.GetStep() == pb.UpgradeSteps_PREPARE_START_AGENTS {
						stepStatusSaved = stepStatus
					}
				}
				return stepStatusSaved.GetStatus()

			}

			//Expect(stepStatusSaved.GetStep()).ToNot(BeZero())
			Eventually(pollStatusUpgrade).Should(Equal(pb.StepStatus_COMPLETE))
		})

		It("reports that master upgrade is pending when pg_upgrade dir does not exist", func() {
			utils.System.IsNotExist = func(error) bool {
				return true
			}

			formulatedResponse, err := hub.StatusUpgrade(nil, fakeStatusUpgradeRequest)
			Expect(err).To(BeNil())

			stepStatuses := formulatedResponse.GetListOfUpgradeStepStatuses()

			for _, stepStatus := range stepStatuses {
				if stepStatus.GetStep() == pb.UpgradeSteps_MASTERUPGRADE {
					Expect(stepStatus.GetStatus()).To(Equal(pb.StepStatus_PENDING))
				}
			}
		})

		It("reports that master upgrade is running when pg_upgrade/*.inprogress files exists", func() {
			outChan <- []byte("123")
			outChan <- []byte("123")

			utils.System.IsNotExist = func(error) bool {
				return false
			}
			utils.System.FilePathGlob = func(string) ([]string, error) {
				return []string{"somefile.inprogress"}, nil
			}

			formulatedResponse, err := hub.StatusUpgrade(nil, fakeStatusUpgradeRequest)
			Expect(err).To(BeNil())

			stepStatuses := formulatedResponse.GetListOfUpgradeStepStatuses()

			for _, stepStatus := range stepStatuses {
				if stepStatus.GetStep() == pb.UpgradeSteps_MASTERUPGRADE {
					Expect(stepStatus.GetStatus()).To(Equal(pb.StepStatus_RUNNING))
				}
			}
		})

		It("reports that master upgrade is done when no *.inprogress files exist in ~/.gp_upgrade/pg_upgrade", func() {
			outChan <- []byte("stdout/stderr message")
			errChan <- errors.New("bogus error")

			utils.System.IsNotExist = func(error) bool {
				return false
			}
			utils.System.FilePathGlob = func(glob string) ([]string, error) {
				if strings.Contains(glob, "inprogress") {
					return nil, errors.New("fake error")
				} else if strings.Contains(glob, "done") {
					return []string{"found something"}, nil
				}

				return nil, errors.New("test not configured for this glob")
			}

			utils.System.Stat = func(filename string) (os.FileInfo, error) {
				if strings.Contains(filename, "found something") {
					return &testutils.FakeFileInfo{}, nil
				}
				return nil, nil
			}

			utils.System.Open = func(name string) (*os.File, error) {
				// Temporarily create a file that we can read as a real file descriptor
				fd, err := ioutil.TempFile("/tmp", "hub_status_upgrade_test")
				Expect(err).To(BeNil())

				filename := fd.Name()
				fd.WriteString("12312312;Upgrade complete;\n")
				fd.Close()
				return os.Open(filename)

			}

			formulatedResponse, err := hub.StatusUpgrade(nil, fakeStatusUpgradeRequest)
			Expect(err).To(BeNil())

			stepStatuses := formulatedResponse.GetListOfUpgradeStepStatuses()

			for _, stepStatus := range stepStatuses {
				if stepStatus.GetStep() == pb.UpgradeSteps_MASTERUPGRADE {
					Expect(stepStatus.GetStatus()).To(Equal(pb.StepStatus_COMPLETE))
				}
			}
		})

		It("reports pg_upgrade has failed", func() {
			outChan <- []byte("stdout/stderr message")
			errChan <- errors.New("bogus error")

			utils.System.IsNotExist = func(error) bool {
				return false
			}
			utils.System.FilePathGlob = func(glob string) ([]string, error) {
				if strings.Contains(glob, "inprogress") {
					return nil, errors.New("fake error")
				} else if strings.Contains(glob, "done") {
					return []string{"found something"}, nil
				}

				return nil, errors.New("test not configured for this glob")
			}

			utils.System.Open = func(name string) (*os.File, error) {
				// Temporarily create a file that we can read as a real file descriptor
				fd, err := ioutil.TempFile("/tmp", "hub_status_upgrade_test")
				Expect(err).To(BeNil())

				filename := fd.Name()
				fd.WriteString("12312312;Upgrade failed;\n")
				fd.Close()
				return os.Open(filename)

			}
			formulatedResponse, err := hub.StatusUpgrade(nil, fakeStatusUpgradeRequest)
			Expect(err).To(BeNil())

			stepStatuses := formulatedResponse.GetListOfUpgradeStepStatuses()

			for _, stepStatus := range stepStatuses {
				if stepStatus.GetStep() == pb.UpgradeSteps_MASTERUPGRADE {
					Expect(stepStatus.GetStatus()).To(Equal(pb.StepStatus_FAILED))
				}
			}
		})
	})

	Describe("Status of PrepareNewClusterConfig", func() {
		It("marks this step pending if there's no new cluster config file", func() {
			utils.System.Stat = func(filename string) (os.FileInfo, error) {
				return nil, errors.New("cannot find file") /* This is normally a PathError */
			}
			stepStatus, err := services.GetPrepareNewClusterConfigStatus(dir)
			Expect(err).To(BeNil()) // convert file-not-found errors into stepStatus
			Expect(stepStatus.Step).To(Equal(pb.UpgradeSteps_PREPARE_INIT_CLUSTER))
			Expect(stepStatus.Status).To(Equal(pb.StepStatus_PENDING))
		})

		It("marks this step complete if there is a new cluster config file", func() {
			utils.System.Stat = func(filename string) (os.FileInfo, error) {
				return nil, nil
			}

			stepStatus, err := services.GetPrepareNewClusterConfigStatus(dir)
			Expect(err).To(BeNil())
			Expect(stepStatus.Step).To(Equal(pb.UpgradeSteps_PREPARE_INIT_CLUSTER))
			Expect(stepStatus.Status).To(Equal(pb.StepStatus_COMPLETE))

		})

	})

	Describe("Status of ShutdownClusters", func() {
		It("We're sending the status of shutdown clusters", func() {
			formulatedResponse, err := hub.StatusUpgrade(nil, fakeStatusUpgradeRequest)
			Expect(err).To(BeNil())
			countOfStatuses := len(formulatedResponse.GetListOfUpgradeStepStatuses())
			Expect(countOfStatuses).ToNot(BeZero())
			found := false
			for _, v := range formulatedResponse.GetListOfUpgradeStepStatuses() {
				if pb.UpgradeSteps_STOPPED_CLUSTER == v.Step {
					found = true
				}
			}
			Expect(found).To(Equal(true))
		})
	})
})

func setStateFile(dir string, step string, state string) {
	err := os.MkdirAll(filepath.Join(dir, step), os.ModePerm)
	Expect(err).ToNot(HaveOccurred())

	f, err := os.Create(filepath.Join(dir, step, state))
	Expect(err).ToNot(HaveOccurred())
	f.Close()
}
