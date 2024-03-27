package testutils

import (
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"google.golang.org/grpc"
)

type MockStream struct {
	buf []*idl.HubReply
	grpc.ServerStream
	err error
}

func NewMockStream(err ...error) (*hub.HubStream, *MockStream) {
	if len(err) == 0 {
		err = append(err, nil)
	}

	mockStream := hub.NewHubStream(&MockStream{
		err: err[0],
	})

	handler := mockStream.GetHandler()
	if val, ok := handler.(*MockStream); ok {
		return &mockStream, val
	}

	return &mockStream, nil
}

func (m *MockStream) Send(reply *idl.HubReply) error {
	if m.err == nil {
		m.buf = append(m.buf, reply)
		return nil
	} else {
		return m.err
	}
}

func (m *MockStream) GetBuffer() []*idl.HubReply {
	return m.buf
}
