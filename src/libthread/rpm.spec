Summary: Port of Plan 9's thread library
Name: libthread
Version: 2.0
Release: 1
Group: Development/C
Copyright: BSD-like
Packager: Russ Cox <rsc@post.harvard.edu>
Source: http://pdos.lcs.mit.edu/~rsc/software/libthread-2.0.tgz
URL: http://pdos.lcs.mit.edu/~rsc/software/#libthread

%description
Libthread is a port of Plan 9's thread library
%prep
%setup

%build
make

%install
make install

%files
/usr/local/include/thread.h
/usr/local/lib/libthread.a
/usr/local/man/man3/thread.3
/usr/local/man/man3/ioproc.3
