Summary: Simple buffered I/O library from Plan 9
Name: libbio
Version: 2.0
Release: 1
Group: Development/C
Copyright: LGPL
Packager: Russ Cox <rsc@post.harvard.edu>
Source: http://pdos.lcs.mit.edu/~rsc/software/libbio-2.0.tgz
URL: http://pdos.lcs.mit.edu/~rsc/software/#libbio
Requires: libfmt libutf

%description
Libbio is a port of Plan 9's formatted I/O library.
It provides most of the same functionality as stdio or sfio,
but with a simpler interface and smaller footprint.

http://plan9.bell-labs.com/magic/man2html/2/bio
%prep
%setup

%build
make

%install
make install

%files
/usr/local/include/bio.h
/usr/local/lib/libbio.a
/usr/local/man/man3/bio.3
