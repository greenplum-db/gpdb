#!/bin/sh

cp -r "./ccp_src_unmodified/." "./ccp_src_modified"
mv "${GPINITSYSTEM_CONFIG}" "ccp_src_modified/scripts/gpinitsystem_config"

