// Code generated by MockGen. DO NOT EDIT.
// Source: idl/cli_to_hub.pb.go

// Package mock_idl is a generated GoMock package.
package mock_idl

import (
	gomock "github.com/golang/mock/gomock"
	context "golang.org/x/net/context"
	grpc "google.golang.org/grpc"
	. "gp_upgrade/idl"
	reflect "reflect"
)

// MockCliToHubClient is a mock of CliToHubClient interface
type MockCliToHubClient struct {
	ctrl     *gomock.Controller
	recorder *MockCliToHubClientMockRecorder
}

// MockCliToHubClientMockRecorder is the mock recorder for MockCliToHubClient
type MockCliToHubClientMockRecorder struct {
	mock *MockCliToHubClient
}

// NewMockCliToHubClient creates a new mock instance
func NewMockCliToHubClient(ctrl *gomock.Controller) *MockCliToHubClient {
	mock := &MockCliToHubClient{ctrl: ctrl}
	mock.recorder = &MockCliToHubClientMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use
func (m *MockCliToHubClient) EXPECT() *MockCliToHubClientMockRecorder {
	return m.recorder
}

// StatusUpgrade mocks base method
func (m *MockCliToHubClient) StatusUpgrade(ctx context.Context, in *StatusUpgradeRequest, opts ...grpc.CallOption) (*StatusUpgradeReply, error) {
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "StatusUpgrade", varargs...)
	ret0, _ := ret[0].(*StatusUpgradeReply)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// StatusUpgrade indicates an expected call of StatusUpgrade
func (mr *MockCliToHubClientMockRecorder) StatusUpgrade(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "StatusUpgrade", reflect.TypeOf((*MockCliToHubClient)(nil).StatusUpgrade), varargs...)
}

// CheckConfig mocks base method
func (m *MockCliToHubClient) CheckConfig(ctx context.Context, in *CheckConfigRequest, opts ...grpc.CallOption) (*CheckConfigReply, error) {
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "CheckConfig", varargs...)
	ret0, _ := ret[0].(*CheckConfigReply)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// CheckConfig indicates an expected call of CheckConfig
func (mr *MockCliToHubClientMockRecorder) CheckConfig(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "CheckConfig", reflect.TypeOf((*MockCliToHubClient)(nil).CheckConfig), varargs...)
}

// CheckObjectCount mocks base method
func (m *MockCliToHubClient) CheckObjectCount(ctx context.Context, in *CheckObjectCountRequest, opts ...grpc.CallOption) (*CheckObjectCountReply, error) {
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "CheckObjectCount", varargs...)
	ret0, _ := ret[0].(*CheckObjectCountReply)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// CheckObjectCount indicates an expected call of CheckObjectCount
func (mr *MockCliToHubClientMockRecorder) CheckObjectCount(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "CheckObjectCount", reflect.TypeOf((*MockCliToHubClient)(nil).CheckObjectCount), varargs...)
}

// MockCliToHubServer is a mock of CliToHubServer interface
type MockCliToHubServer struct {
	ctrl     *gomock.Controller
	recorder *MockCliToHubServerMockRecorder
}

// MockCliToHubServerMockRecorder is the mock recorder for MockCliToHubServer
type MockCliToHubServerMockRecorder struct {
	mock *MockCliToHubServer
}

// NewMockCliToHubServer creates a new mock instance
func NewMockCliToHubServer(ctrl *gomock.Controller) *MockCliToHubServer {
	mock := &MockCliToHubServer{ctrl: ctrl}
	mock.recorder = &MockCliToHubServerMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use
func (m *MockCliToHubServer) EXPECT() *MockCliToHubServerMockRecorder {
	return m.recorder
}

// StatusUpgrade mocks base method
func (m *MockCliToHubServer) StatusUpgrade(arg0 context.Context, arg1 *StatusUpgradeRequest) (*StatusUpgradeReply, error) {
	ret := m.ctrl.Call(m, "StatusUpgrade", arg0, arg1)
	ret0, _ := ret[0].(*StatusUpgradeReply)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// StatusUpgrade indicates an expected call of StatusUpgrade
func (mr *MockCliToHubServerMockRecorder) StatusUpgrade(arg0, arg1 interface{}) *gomock.Call {
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "StatusUpgrade", reflect.TypeOf((*MockCliToHubServer)(nil).StatusUpgrade), arg0, arg1)
}

// CheckConfig mocks base method
func (m *MockCliToHubServer) CheckConfig(arg0 context.Context, arg1 *CheckConfigRequest) (*CheckConfigReply, error) {
	ret := m.ctrl.Call(m, "CheckConfig", arg0, arg1)
	ret0, _ := ret[0].(*CheckConfigReply)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// CheckConfig indicates an expected call of CheckConfig
func (mr *MockCliToHubServerMockRecorder) CheckConfig(arg0, arg1 interface{}) *gomock.Call {
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "CheckConfig", reflect.TypeOf((*MockCliToHubServer)(nil).CheckConfig), arg0, arg1)
}

// CheckObjectCount mocks base method
func (m *MockCliToHubServer) CheckObjectCount(arg0 context.Context, arg1 *CheckObjectCountRequest) (*CheckObjectCountReply, error) {
	ret := m.ctrl.Call(m, "CheckObjectCount", arg0, arg1)
	ret0, _ := ret[0].(*CheckObjectCountReply)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// CheckObjectCount indicates an expected call of CheckObjectCount
func (mr *MockCliToHubServerMockRecorder) CheckObjectCount(arg0, arg1 interface{}) *gomock.Call {
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "CheckObjectCount", reflect.TypeOf((*MockCliToHubServer)(nil).CheckObjectCount), arg0, arg1)
}
