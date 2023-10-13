package idl

// Creates the .pb.go protobuf definitions.
//go:generate protoc --plugin=../dev-bin/protoc-gen-go --go_out=plugins=grpc:. hub.proto agent.proto

// Generates mocks for the above definitions.
//go:generate ../dev-bin/mockgen -destination mock_idl/mock_hub.pb.go github.com/greenplum-db/gpdb/gp/idl HubClient,HubServer
//go:generate ../dev-bin/mockgen -source agent.pb.go -destination mock_idl/mock_agent.pb.go
