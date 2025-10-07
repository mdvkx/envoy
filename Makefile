
SHELL = /bin/sh

.PHONY:  default compdb docker docker-build docker-build-test

DOCKER = env ENVOY_DOCKER_BUILD_DIR="${HOME}"/envoy-docker-build ./ci/run_envoy_docker.sh

default:
	@echo "$(DOCKER)"

compdb:
	ENVOY_GEN_COMPDB_OPTIONS="--vscode --exclude_contrib" ./ci/do_ci.sh refresh_compdb

docker:
	$(DOCKER) 'bash'
docker-build:
	$(DOCKER) 'env BAZELRC_FILE=/build/clang.bazelrc ci/do_ci.sh dev.server_only'
docker-build-test:
	$(DOCKER) 'env BAZELRC_FILE=/build/clang.bazelrc ci/do_ci.sh dev //test/extensions/http/cache/ring_buffer_http_cache:ring_buffer_http_cache_test'
