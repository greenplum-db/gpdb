BUILD_DIR=../../../..
GPAUX_DIR=$(BUILD_DIR)/gpAux

.PHONY: build
build: dist

dist: Dockerfile_dist $(GPAUX_DIR)/addon/README
	$(info $@ ...)
	docker build --iidfile dist --tag ubuntu_ent_build:16.04_dist \
		--file Dockerfile_dist $(BUILD_DIR)
	docker run -it --rm -v /tmp:/tmp ubuntu_ent_build:16.04_dist bash -c \
		"tar czf /tmp/bin_gpdb.tar.gz -C /usr/local/greenplum-db-devel/ ."

$(GPAUX_DIR)/addon/README:
	$(info $@ ...)
	rm -fr $(GPAUX_DIR)/addon/ && \
	mkdir -p $(GPAUX_DIR)/ && \
	cd $(GPAUX_DIR)/ && \
	git clone git@github.com:greenplum-db/gpaddon.git -b 5X_STABLE addon

.PHONY: check
check: Dockerfile_check dist
	$(info $@ ...)
	docker build --no-cache --tag ubuntu_ent_build:16.04_check --file Dockerfile_check .
	docker run -t ubuntu_ent_build:16.04_check

.PHONY: debug
debug:
	$(info $@ ...)
	docker run -it --rm $$(docker images -q | head -n 1) bash

.PHONY: clean
clean:
	$(info $@ ...)
	rm -fr ext/
	docker rmi -f ubuntu_ent_build:16.04_check || true
	for tag in \
			dist \
			; do \
		if [ -f $$tag ] ; then \
			docker rmi -f $$(cat $$tag) ; \
			rm -f $$tag ; \
		fi ; \
	done

.PHONY: docker-clean
docker-clean:
	$(info $@ ...)
	docker container prune -f || true
	docker image prune -f --filter="dangling=true" || true
