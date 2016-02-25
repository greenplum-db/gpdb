#!/bin/bash

# S3Common.h S3Downloader.h utils.h utils.cpp S3Downloader.cpp S3Common.cpp extlib/http_parser.cpp include/http_parser.h .
ln -sf library/S3Common.h S3Common.h
ln -sf library/S3Downloader.h S3Downloader.h
ln -sf library/utils.h utils.h
ln -sf library/utils.cpp utils.cpp
ln -sf library/S3Downloader.cpp S3Downloader.cpp
ln -sf library/S3Common.cpp S3Common.cpp
ln -sf library/extlib/http_parser.cpp http_parser.cpp
ln -sf library/include/http_parser.h http_parser.h


