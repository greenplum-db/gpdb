.DEFAULT_GOAL := test

.PHONY : build

DEST = bin/gp_upgrade

GOFLAGS := -o $(DEST)

dependencies :
		go get github.com/onsi/ginkgo/ginkgo
		go get golang.org/x/tools/cmd/goimports
		go get github.com/maxbrunsfeld/counterfeiter
		go get github.com/onsi/gomega
		go get github.com/jessevdk/go-flags
		go get

format : dependencies
		goimports -w .
		go fmt .

ginkgo : dependencies
		ginkgo -r -randomizeSuites -randomizeAllSpecs -race 2>&1

test : format ginkgo

ci : ginkgo

build :
		mkdir -p build
		go build $(GOFLAGS)
