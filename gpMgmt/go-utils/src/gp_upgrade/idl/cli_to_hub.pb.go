// Code generated by protoc-gen-go. DO NOT EDIT.
// source: cli_to_hub.proto

/*
Package idl is a generated protocol buffer package.

It is generated from these files:
	cli_to_hub.proto
	hub_to_agent.proto

It has these top-level messages:
	PingRequest
	PingReply
	StatusUpgradeRequest
	StatusUpgradeReply
	UpgradeStepStatus
	CheckConfigRequest
	CheckConfigReply
	CountPerDb
	CheckObjectCountRequest
	CheckObjectCountReply
	CheckVersionRequest
	CheckVersionReply
	CheckDiskUsageRequest
	CheckDiskUsageReply
	CheckUpgradeStatusRequest
	CheckUpgradeStatusReply
	FileSysUsage
	CheckDiskUsageRequestToAgent
	CheckDiskUsageReplyFromAgent
*/
package idl

import proto "github.com/golang/protobuf/proto"
import fmt "fmt"
import math "math"

import (
	context "golang.org/x/net/context"
	grpc "google.golang.org/grpc"
)

// Reference imports to suppress errors if they are not otherwise used.
var _ = proto.Marshal
var _ = fmt.Errorf
var _ = math.Inf

// This is a compile-time assertion to ensure that this generated file
// is compatible with the proto package it is being compiled against.
// A compilation error at this line likely means your copy of the
// proto package needs to be updated.
const _ = proto.ProtoPackageIsVersion2 // please upgrade the proto package

type UpgradeSteps int32

const (
	UpgradeSteps_UNKNOWN_STEP UpgradeSteps = 0
	UpgradeSteps_CHECK_CONFIG UpgradeSteps = 1
	UpgradeSteps_SEGINSTALL   UpgradeSteps = 2
)

var UpgradeSteps_name = map[int32]string{
	0: "UNKNOWN_STEP",
	1: "CHECK_CONFIG",
	2: "SEGINSTALL",
}
var UpgradeSteps_value = map[string]int32{
	"UNKNOWN_STEP": 0,
	"CHECK_CONFIG": 1,
	"SEGINSTALL":   2,
}

func (x UpgradeSteps) String() string {
	return proto.EnumName(UpgradeSteps_name, int32(x))
}
func (UpgradeSteps) EnumDescriptor() ([]byte, []int) { return fileDescriptor0, []int{0} }

type StepStatus int32

const (
	StepStatus_UNKNOWN_STATUS StepStatus = 0
	StepStatus_PENDING        StepStatus = 1
	StepStatus_RUNNING        StepStatus = 2
	StepStatus_COMPLETE       StepStatus = 3
	StepStatus_FAILED         StepStatus = 4
)

var StepStatus_name = map[int32]string{
	0: "UNKNOWN_STATUS",
	1: "PENDING",
	2: "RUNNING",
	3: "COMPLETE",
	4: "FAILED",
}
var StepStatus_value = map[string]int32{
	"UNKNOWN_STATUS": 0,
	"PENDING":        1,
	"RUNNING":        2,
	"COMPLETE":       3,
	"FAILED":         4,
}

func (x StepStatus) String() string {
	return proto.EnumName(StepStatus_name, int32(x))
}
func (StepStatus) EnumDescriptor() ([]byte, []int) { return fileDescriptor0, []int{1} }

type PingRequest struct {
}

func (m *PingRequest) Reset()                    { *m = PingRequest{} }
func (m *PingRequest) String() string            { return proto.CompactTextString(m) }
func (*PingRequest) ProtoMessage()               {}
func (*PingRequest) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{0} }

type PingReply struct {
}

func (m *PingReply) Reset()                    { *m = PingReply{} }
func (m *PingReply) String() string            { return proto.CompactTextString(m) }
func (*PingReply) ProtoMessage()               {}
func (*PingReply) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{1} }

type StatusUpgradeRequest struct {
}

func (m *StatusUpgradeRequest) Reset()                    { *m = StatusUpgradeRequest{} }
func (m *StatusUpgradeRequest) String() string            { return proto.CompactTextString(m) }
func (*StatusUpgradeRequest) ProtoMessage()               {}
func (*StatusUpgradeRequest) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{2} }

type StatusUpgradeReply struct {
	ListOfUpgradeStepStatuses []*UpgradeStepStatus `protobuf:"bytes,1,rep,name=listOfUpgradeStepStatuses" json:"listOfUpgradeStepStatuses,omitempty"`
}

func (m *StatusUpgradeReply) Reset()                    { *m = StatusUpgradeReply{} }
func (m *StatusUpgradeReply) String() string            { return proto.CompactTextString(m) }
func (*StatusUpgradeReply) ProtoMessage()               {}
func (*StatusUpgradeReply) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{3} }

func (m *StatusUpgradeReply) GetListOfUpgradeStepStatuses() []*UpgradeStepStatus {
	if m != nil {
		return m.ListOfUpgradeStepStatuses
	}
	return nil
}

type UpgradeStepStatus struct {
	Step   UpgradeSteps `protobuf:"varint,1,opt,name=step,enum=idl.UpgradeSteps" json:"step,omitempty"`
	Status StepStatus   `protobuf:"varint,2,opt,name=status,enum=idl.StepStatus" json:"status,omitempty"`
}

func (m *UpgradeStepStatus) Reset()                    { *m = UpgradeStepStatus{} }
func (m *UpgradeStepStatus) String() string            { return proto.CompactTextString(m) }
func (*UpgradeStepStatus) ProtoMessage()               {}
func (*UpgradeStepStatus) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{4} }

func (m *UpgradeStepStatus) GetStep() UpgradeSteps {
	if m != nil {
		return m.Step
	}
	return UpgradeSteps_UNKNOWN_STEP
}

func (m *UpgradeStepStatus) GetStatus() StepStatus {
	if m != nil {
		return m.Status
	}
	return StepStatus_UNKNOWN_STATUS
}

type CheckConfigRequest struct {
	DbPort int32 `protobuf:"varint,1,opt,name=dbPort" json:"dbPort,omitempty"`
}

func (m *CheckConfigRequest) Reset()                    { *m = CheckConfigRequest{} }
func (m *CheckConfigRequest) String() string            { return proto.CompactTextString(m) }
func (*CheckConfigRequest) ProtoMessage()               {}
func (*CheckConfigRequest) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{5} }

func (m *CheckConfigRequest) GetDbPort() int32 {
	if m != nil {
		return m.DbPort
	}
	return 0
}

// Consider removing the status as errors are/should be put on the error field.
type CheckConfigReply struct {
	ConfigStatus string `protobuf:"bytes,1,opt,name=configStatus" json:"configStatus,omitempty"`
}

func (m *CheckConfigReply) Reset()                    { *m = CheckConfigReply{} }
func (m *CheckConfigReply) String() string            { return proto.CompactTextString(m) }
func (*CheckConfigReply) ProtoMessage()               {}
func (*CheckConfigReply) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{6} }

func (m *CheckConfigReply) GetConfigStatus() string {
	if m != nil {
		return m.ConfigStatus
	}
	return ""
}

type CountPerDb struct {
	DbName    string `protobuf:"bytes,1,opt,name=DbName" json:"DbName,omitempty"`
	AoCount   int32  `protobuf:"varint,2,opt,name=AoCount" json:"AoCount,omitempty"`
	HeapCount int32  `protobuf:"varint,3,opt,name=HeapCount" json:"HeapCount,omitempty"`
}

func (m *CountPerDb) Reset()                    { *m = CountPerDb{} }
func (m *CountPerDb) String() string            { return proto.CompactTextString(m) }
func (*CountPerDb) ProtoMessage()               {}
func (*CountPerDb) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{7} }

func (m *CountPerDb) GetDbName() string {
	if m != nil {
		return m.DbName
	}
	return ""
}

func (m *CountPerDb) GetAoCount() int32 {
	if m != nil {
		return m.AoCount
	}
	return 0
}

func (m *CountPerDb) GetHeapCount() int32 {
	if m != nil {
		return m.HeapCount
	}
	return 0
}

type CheckObjectCountRequest struct {
	DbPort int32 `protobuf:"varint,1,opt,name=DbPort" json:"DbPort,omitempty"`
}

func (m *CheckObjectCountRequest) Reset()                    { *m = CheckObjectCountRequest{} }
func (m *CheckObjectCountRequest) String() string            { return proto.CompactTextString(m) }
func (*CheckObjectCountRequest) ProtoMessage()               {}
func (*CheckObjectCountRequest) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{8} }

func (m *CheckObjectCountRequest) GetDbPort() int32 {
	if m != nil {
		return m.DbPort
	}
	return 0
}

type CheckObjectCountReply struct {
	ListOfCounts []*CountPerDb `protobuf:"bytes,1,rep,name=list_of_counts,json=listOfCounts" json:"list_of_counts,omitempty"`
}

func (m *CheckObjectCountReply) Reset()                    { *m = CheckObjectCountReply{} }
func (m *CheckObjectCountReply) String() string            { return proto.CompactTextString(m) }
func (*CheckObjectCountReply) ProtoMessage()               {}
func (*CheckObjectCountReply) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{9} }

func (m *CheckObjectCountReply) GetListOfCounts() []*CountPerDb {
	if m != nil {
		return m.ListOfCounts
	}
	return nil
}

type CheckVersionRequest struct {
	DbPort int32  `protobuf:"varint,1,opt,name=DbPort" json:"DbPort,omitempty"`
	Host   string `protobuf:"bytes,2,opt,name=Host" json:"Host,omitempty"`
}

func (m *CheckVersionRequest) Reset()                    { *m = CheckVersionRequest{} }
func (m *CheckVersionRequest) String() string            { return proto.CompactTextString(m) }
func (*CheckVersionRequest) ProtoMessage()               {}
func (*CheckVersionRequest) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{10} }

func (m *CheckVersionRequest) GetDbPort() int32 {
	if m != nil {
		return m.DbPort
	}
	return 0
}

func (m *CheckVersionRequest) GetHost() string {
	if m != nil {
		return m.Host
	}
	return ""
}

type CheckVersionReply struct {
	IsVersionCompatible bool `protobuf:"varint,1,opt,name=IsVersionCompatible" json:"IsVersionCompatible,omitempty"`
}

func (m *CheckVersionReply) Reset()                    { *m = CheckVersionReply{} }
func (m *CheckVersionReply) String() string            { return proto.CompactTextString(m) }
func (*CheckVersionReply) ProtoMessage()               {}
func (*CheckVersionReply) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{11} }

func (m *CheckVersionReply) GetIsVersionCompatible() bool {
	if m != nil {
		return m.IsVersionCompatible
	}
	return false
}

type CheckDiskUsageRequest struct {
}

func (m *CheckDiskUsageRequest) Reset()                    { *m = CheckDiskUsageRequest{} }
func (m *CheckDiskUsageRequest) String() string            { return proto.CompactTextString(m) }
func (*CheckDiskUsageRequest) ProtoMessage()               {}
func (*CheckDiskUsageRequest) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{12} }

type CheckDiskUsageReply struct {
	SegmentFileSysUsage []string `protobuf:"bytes,1,rep,name=SegmentFileSysUsage" json:"SegmentFileSysUsage,omitempty"`
}

func (m *CheckDiskUsageReply) Reset()                    { *m = CheckDiskUsageReply{} }
func (m *CheckDiskUsageReply) String() string            { return proto.CompactTextString(m) }
func (*CheckDiskUsageReply) ProtoMessage()               {}
func (*CheckDiskUsageReply) Descriptor() ([]byte, []int) { return fileDescriptor0, []int{13} }

func (m *CheckDiskUsageReply) GetSegmentFileSysUsage() []string {
	if m != nil {
		return m.SegmentFileSysUsage
	}
	return nil
}

func init() {
	proto.RegisterType((*PingRequest)(nil), "idl.PingRequest")
	proto.RegisterType((*PingReply)(nil), "idl.PingReply")
	proto.RegisterType((*StatusUpgradeRequest)(nil), "idl.StatusUpgradeRequest")
	proto.RegisterType((*StatusUpgradeReply)(nil), "idl.StatusUpgradeReply")
	proto.RegisterType((*UpgradeStepStatus)(nil), "idl.UpgradeStepStatus")
	proto.RegisterType((*CheckConfigRequest)(nil), "idl.CheckConfigRequest")
	proto.RegisterType((*CheckConfigReply)(nil), "idl.CheckConfigReply")
	proto.RegisterType((*CountPerDb)(nil), "idl.CountPerDb")
	proto.RegisterType((*CheckObjectCountRequest)(nil), "idl.CheckObjectCountRequest")
	proto.RegisterType((*CheckObjectCountReply)(nil), "idl.CheckObjectCountReply")
	proto.RegisterType((*CheckVersionRequest)(nil), "idl.CheckVersionRequest")
	proto.RegisterType((*CheckVersionReply)(nil), "idl.CheckVersionReply")
	proto.RegisterType((*CheckDiskUsageRequest)(nil), "idl.CheckDiskUsageRequest")
	proto.RegisterType((*CheckDiskUsageReply)(nil), "idl.CheckDiskUsageReply")
	proto.RegisterEnum("idl.UpgradeSteps", UpgradeSteps_name, UpgradeSteps_value)
	proto.RegisterEnum("idl.StepStatus", StepStatus_name, StepStatus_value)
}

// Reference imports to suppress errors if they are not otherwise used.
var _ context.Context
var _ grpc.ClientConn

// This is a compile-time assertion to ensure that this generated file
// is compatible with the grpc package it is being compiled against.
const _ = grpc.SupportPackageIsVersion4

// Client API for CliToHub service

type CliToHubClient interface {
	Ping(ctx context.Context, in *PingRequest, opts ...grpc.CallOption) (*PingReply, error)
	StatusUpgrade(ctx context.Context, in *StatusUpgradeRequest, opts ...grpc.CallOption) (*StatusUpgradeReply, error)
	CheckConfig(ctx context.Context, in *CheckConfigRequest, opts ...grpc.CallOption) (*CheckConfigReply, error)
	CheckObjectCount(ctx context.Context, in *CheckObjectCountRequest, opts ...grpc.CallOption) (*CheckObjectCountReply, error)
	CheckVersion(ctx context.Context, in *CheckVersionRequest, opts ...grpc.CallOption) (*CheckVersionReply, error)
	CheckDiskUsage(ctx context.Context, in *CheckDiskUsageRequest, opts ...grpc.CallOption) (*CheckDiskUsageReply, error)
}

type cliToHubClient struct {
	cc *grpc.ClientConn
}

func NewCliToHubClient(cc *grpc.ClientConn) CliToHubClient {
	return &cliToHubClient{cc}
}

func (c *cliToHubClient) Ping(ctx context.Context, in *PingRequest, opts ...grpc.CallOption) (*PingReply, error) {
	out := new(PingReply)
	err := grpc.Invoke(ctx, "/idl.CliToHub/Ping", in, out, c.cc, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *cliToHubClient) StatusUpgrade(ctx context.Context, in *StatusUpgradeRequest, opts ...grpc.CallOption) (*StatusUpgradeReply, error) {
	out := new(StatusUpgradeReply)
	err := grpc.Invoke(ctx, "/idl.CliToHub/StatusUpgrade", in, out, c.cc, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *cliToHubClient) CheckConfig(ctx context.Context, in *CheckConfigRequest, opts ...grpc.CallOption) (*CheckConfigReply, error) {
	out := new(CheckConfigReply)
	err := grpc.Invoke(ctx, "/idl.CliToHub/CheckConfig", in, out, c.cc, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *cliToHubClient) CheckObjectCount(ctx context.Context, in *CheckObjectCountRequest, opts ...grpc.CallOption) (*CheckObjectCountReply, error) {
	out := new(CheckObjectCountReply)
	err := grpc.Invoke(ctx, "/idl.CliToHub/CheckObjectCount", in, out, c.cc, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *cliToHubClient) CheckVersion(ctx context.Context, in *CheckVersionRequest, opts ...grpc.CallOption) (*CheckVersionReply, error) {
	out := new(CheckVersionReply)
	err := grpc.Invoke(ctx, "/idl.CliToHub/CheckVersion", in, out, c.cc, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *cliToHubClient) CheckDiskUsage(ctx context.Context, in *CheckDiskUsageRequest, opts ...grpc.CallOption) (*CheckDiskUsageReply, error) {
	out := new(CheckDiskUsageReply)
	err := grpc.Invoke(ctx, "/idl.CliToHub/CheckDiskUsage", in, out, c.cc, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

// Server API for CliToHub service

type CliToHubServer interface {
	Ping(context.Context, *PingRequest) (*PingReply, error)
	StatusUpgrade(context.Context, *StatusUpgradeRequest) (*StatusUpgradeReply, error)
	CheckConfig(context.Context, *CheckConfigRequest) (*CheckConfigReply, error)
	CheckObjectCount(context.Context, *CheckObjectCountRequest) (*CheckObjectCountReply, error)
	CheckVersion(context.Context, *CheckVersionRequest) (*CheckVersionReply, error)
	CheckDiskUsage(context.Context, *CheckDiskUsageRequest) (*CheckDiskUsageReply, error)
}

func RegisterCliToHubServer(s *grpc.Server, srv CliToHubServer) {
	s.RegisterService(&_CliToHub_serviceDesc, srv)
}

func _CliToHub_Ping_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(PingRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(CliToHubServer).Ping(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: "/idl.CliToHub/Ping",
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(CliToHubServer).Ping(ctx, req.(*PingRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _CliToHub_StatusUpgrade_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(StatusUpgradeRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(CliToHubServer).StatusUpgrade(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: "/idl.CliToHub/StatusUpgrade",
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(CliToHubServer).StatusUpgrade(ctx, req.(*StatusUpgradeRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _CliToHub_CheckConfig_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(CheckConfigRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(CliToHubServer).CheckConfig(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: "/idl.CliToHub/CheckConfig",
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(CliToHubServer).CheckConfig(ctx, req.(*CheckConfigRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _CliToHub_CheckObjectCount_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(CheckObjectCountRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(CliToHubServer).CheckObjectCount(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: "/idl.CliToHub/CheckObjectCount",
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(CliToHubServer).CheckObjectCount(ctx, req.(*CheckObjectCountRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _CliToHub_CheckVersion_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(CheckVersionRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(CliToHubServer).CheckVersion(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: "/idl.CliToHub/CheckVersion",
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(CliToHubServer).CheckVersion(ctx, req.(*CheckVersionRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _CliToHub_CheckDiskUsage_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(CheckDiskUsageRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(CliToHubServer).CheckDiskUsage(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: "/idl.CliToHub/CheckDiskUsage",
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(CliToHubServer).CheckDiskUsage(ctx, req.(*CheckDiskUsageRequest))
	}
	return interceptor(ctx, in, info, handler)
}

var _CliToHub_serviceDesc = grpc.ServiceDesc{
	ServiceName: "idl.CliToHub",
	HandlerType: (*CliToHubServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "Ping",
			Handler:    _CliToHub_Ping_Handler,
		},
		{
			MethodName: "StatusUpgrade",
			Handler:    _CliToHub_StatusUpgrade_Handler,
		},
		{
			MethodName: "CheckConfig",
			Handler:    _CliToHub_CheckConfig_Handler,
		},
		{
			MethodName: "CheckObjectCount",
			Handler:    _CliToHub_CheckObjectCount_Handler,
		},
		{
			MethodName: "CheckVersion",
			Handler:    _CliToHub_CheckVersion_Handler,
		},
		{
			MethodName: "CheckDiskUsage",
			Handler:    _CliToHub_CheckDiskUsage_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "cli_to_hub.proto",
}

func init() { proto.RegisterFile("cli_to_hub.proto", fileDescriptor0) }

var fileDescriptor0 = []byte{
	// 640 bytes of a gzipped FileDescriptorProto
	0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x84, 0x54, 0xc1, 0x6e, 0xda, 0x40,
	0x10, 0x0d, 0x81, 0x90, 0x30, 0x10, 0xea, 0x4c, 0x1a, 0x42, 0xac, 0x1c, 0xa2, 0x95, 0xaa, 0x46,
	0x51, 0x15, 0xb5, 0xa9, 0xda, 0x6b, 0x45, 0x8c, 0x13, 0x50, 0xa8, 0x41, 0xb6, 0x69, 0x2f, 0x95,
	0x10, 0x36, 0x1b, 0xe2, 0xc4, 0x61, 0x5d, 0x76, 0x39, 0xe4, 0x5f, 0xfa, 0xb1, 0x95, 0x77, 0x4d,
	0xb1, 0x31, 0x51, 0x6f, 0x9e, 0xf7, 0xde, 0xbc, 0xd9, 0xd9, 0x9d, 0x31, 0x68, 0x7e, 0x18, 0x8c,
	0x04, 0x1b, 0x3d, 0x2c, 0xbc, 0xcb, 0x68, 0xce, 0x04, 0xc3, 0x62, 0x30, 0x09, 0xc9, 0x3e, 0x54,
	0x07, 0xc1, 0x6c, 0x6a, 0xd3, 0xdf, 0x0b, 0xca, 0x05, 0xa9, 0x42, 0x45, 0x85, 0x51, 0xf8, 0x42,
	0x1a, 0xf0, 0xd6, 0x11, 0x63, 0xb1, 0xe0, 0xc3, 0x68, 0x3a, 0x1f, 0x4f, 0xe8, 0x52, 0xf4, 0x08,
	0xb8, 0x86, 0x47, 0xe1, 0x0b, 0xba, 0x70, 0x12, 0x06, 0x5c, 0xf4, 0xef, 0x13, 0xd4, 0x11, 0x34,
	0x52, 0x32, 0xca, 0x9b, 0x85, 0xb3, 0xe2, 0x79, 0xf5, 0xaa, 0x71, 0x19, 0x4c, 0xc2, 0xcb, 0x1c,
	0x6f, 0xbf, 0x9e, 0x48, 0x7c, 0x38, 0xc8, 0xc1, 0xf8, 0x0e, 0x4a, 0x5c, 0xd0, 0xa8, 0x59, 0x38,
	0x2b, 0x9c, 0xd7, 0xaf, 0x0e, 0xd6, 0x5d, 0xb9, 0x2d, 0x69, 0x7c, 0x0f, 0x65, 0x2e, 0x13, 0x9a,
	0xdb, 0x52, 0xf8, 0x46, 0x0a, 0x53, 0x75, 0x13, 0x9a, 0x7c, 0x00, 0x34, 0x1e, 0xa8, 0xff, 0x64,
	0xb0, 0xd9, 0x7d, 0xb0, 0xbc, 0x0b, 0x6c, 0x40, 0x79, 0xe2, 0x0d, 0xd8, 0x5c, 0xc8, 0x3a, 0x3b,
	0x76, 0x12, 0x91, 0xaf, 0xa0, 0x65, 0xd4, 0x71, 0xf3, 0x04, 0x6a, 0xbe, 0x0c, 0x95, 0xb3, 0xcc,
	0xa8, 0xd8, 0x19, 0x8c, 0xfc, 0x02, 0x30, 0xd8, 0x62, 0x26, 0x06, 0x74, 0xde, 0xf6, 0x62, 0xf7,
	0xb6, 0x67, 0x8d, 0x9f, 0x69, 0xa2, 0x4d, 0x22, 0x6c, 0xc2, 0x6e, 0x8b, 0x49, 0x9d, 0x3c, 0xf5,
	0x8e, 0xbd, 0x0c, 0xf1, 0x14, 0x2a, 0x1d, 0x3a, 0x8e, 0x14, 0x57, 0x94, 0xdc, 0x0a, 0x20, 0x9f,
	0xe0, 0x58, 0x9e, 0xaa, 0xef, 0x3d, 0x52, 0x5f, 0x48, 0x2c, 0xd5, 0x48, 0x3b, 0xd3, 0x88, 0x8a,
	0x88, 0x05, 0x47, 0xf9, 0x94, 0xb8, 0x9b, 0x2f, 0x50, 0x8f, 0x5f, 0x64, 0xc4, 0xee, 0x47, 0x7e,
	0x8c, 0x2e, 0xdf, 0x4f, 0x5d, 0xe0, 0xaa, 0x09, 0xbb, 0xa6, 0x1e, 0x4e, 0x22, 0x9c, 0xb4, 0xe0,
	0x50, 0xfa, 0xfd, 0xa0, 0x73, 0x1e, 0xb0, 0xd9, 0x7f, 0xca, 0x23, 0x42, 0xa9, 0xc3, 0xb8, 0x6a,
	0xb3, 0x62, 0xcb, 0x6f, 0x62, 0xc2, 0x41, 0xd6, 0x22, 0x3e, 0xce, 0x47, 0x38, 0xec, 0xf2, 0x04,
	0x31, 0xd8, 0x73, 0x34, 0x16, 0x81, 0x17, 0xaa, 0x7b, 0xdb, 0xb3, 0x37, 0x51, 0xe4, 0x38, 0xe9,
	0xac, 0x1d, 0xf0, 0xa7, 0x21, 0x1f, 0x4f, 0xff, 0x8d, 0xee, 0x6d, 0x72, 0xc4, 0x14, 0x91, 0x54,
	0x70, 0xe8, 0xf4, 0x99, 0xce, 0xc4, 0x4d, 0x10, 0x52, 0xe7, 0x85, 0x4b, 0x4e, 0x76, 0x5d, 0xb1,
	0x37, 0x51, 0x17, 0xd7, 0x50, 0x4b, 0x4f, 0x1c, 0x6a, 0x50, 0x1b, 0x5a, 0x77, 0x56, 0xff, 0xa7,
	0x35, 0x72, 0x5c, 0x73, 0xa0, 0x6d, 0xc5, 0x88, 0xd1, 0x31, 0x8d, 0xbb, 0x91, 0xd1, 0xb7, 0x6e,
	0xba, 0xb7, 0x5a, 0x01, 0xeb, 0x00, 0x8e, 0x79, 0xdb, 0xb5, 0x1c, 0xb7, 0xd5, 0xeb, 0x69, 0xdb,
	0x17, 0x2e, 0x40, 0x6a, 0xa8, 0x11, 0xea, 0x2b, 0x87, 0x96, 0x3b, 0x74, 0xb4, 0x2d, 0xac, 0xc2,
	0xee, 0xc0, 0xb4, 0xda, 0x5d, 0x2b, 0x4e, 0xaf, 0xc2, 0xae, 0x3d, 0xb4, 0xac, 0x38, 0xd8, 0xc6,
	0x1a, 0xec, 0x19, 0xfd, 0xef, 0x83, 0x9e, 0xe9, 0x9a, 0x5a, 0x11, 0x01, 0xca, 0x37, 0xad, 0x6e,
	0xcf, 0x6c, 0x6b, 0xa5, 0xab, 0x3f, 0x45, 0xd8, 0x33, 0xc2, 0xc0, 0x65, 0x9d, 0x85, 0x87, 0x17,
	0x50, 0x8a, 0xf7, 0x19, 0x35, 0xf9, 0x72, 0xa9, 0x4d, 0xd7, 0xeb, 0x29, 0x24, 0x5e, 0xf6, 0x2d,
	0x34, 0x61, 0x3f, 0xb3, 0xd6, 0x78, 0x92, 0xec, 0x4b, 0xfe, 0x17, 0xa0, 0x1f, 0x6f, 0xa2, 0x94,
	0xcd, 0x37, 0xa8, 0xa6, 0xd6, 0x03, 0x95, 0x32, 0xbf, 0x5e, 0xfa, 0x51, 0x9e, 0x50, 0x06, 0x56,
	0xb2, 0x5f, 0xa9, 0xb1, 0xc4, 0xd3, 0x95, 0x38, 0x3f, 0xe0, 0xba, 0xfe, 0x0a, 0xab, 0xfc, 0xae,
	0xa1, 0x96, 0x9e, 0x29, 0x6c, 0xae, 0xd4, 0xd9, 0x49, 0xd5, 0x1b, 0x1b, 0x18, 0xe5, 0xd1, 0x81,
	0x7a, 0x76, 0x6e, 0x30, 0x55, 0x73, 0x7d, 0xca, 0xf4, 0xe6, 0x46, 0x4e, 0x3a, 0x79, 0x65, 0xf9,
	0xf3, 0xfd, 0xfc, 0x37, 0x00, 0x00, 0xff, 0xff, 0xdc, 0x37, 0x65, 0x1e, 0x90, 0x05, 0x00, 0x00,
}
