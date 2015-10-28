build-debian:
	cd $(CURDIR)/.. \
	&& docker build -f docker/Dockerfile.debian.build -t gpdb_debian_builder . \
	&& docker run -it --rm -v `pwd`:/work -w /work gpdb_debian_builder build

clean-debian:
	cd $(CURDIR)/.. \
        && ( test -f gpdb_*_amd64.deb && rm gpdb_*_amd64.deb || true ) \
	&& docker run -it --rm -v `pwd`:/work -w /work gpdb_debian_builder clean

build-centos5:
	cd $(CURDIR)/.. \
	&& docker build -f docker/Dockerfile.centos5.build -t gpdb_centos5_builder . \
	&& docker run -it --rm -v `pwd`:/work -w /work gpdb_centos5_builder build

clean-centos5:
	cd $(CURDIR)/.. \
        && ( test -f gpdb*.rpm && rm gpdb*.rpm || true ) \
	&& docker run -it --rm -v `pwd`:/work -w /work gpdb_centos5_builder clean

