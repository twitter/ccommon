# CCommon

*CCommon* is a common C library for the various cache backends developed by Twitter's cache team. For a list of projects using ccommon, visit: .

## Origins
The Twitter Cache team started working on a fork of Memcached in 2010, and over time has written various cache backends such as *fatcache*, *slimcache* and cache middle layer *twemproxy*. These projects have a lot in common, especially when you examine the project structure and the underlying mechanism that drives the runtime. Instead of stretching our effort thin by maintaining several individual code bases, we started building a library that captures the commonality of these projects. It is also our belief that the commonality extends beyond just caching, and can be used as the skeleton of writing many more high-throughput, low-latency services used intended for a distributed environment.

## Dependencies
The unit tests rely on Check.

## Build
autoreconf -fvi -I <M4-DIRECTORY>
# on Linux, the default location for installed check m4 script is /usr/local/share/aclocal
# on Mac OS X, if you use brew, the directory will be something like /../../Cellar/check/0.9.10/share/aclocal/
./configure
make
make test
