Summary: Extensible formatted print library.  (Printf with user-defined verbs.)
Name: libfmt
Version: 2.0
Release: 1
Group: Development/C
Copyright: BSD-like
Packager: Russ Cox <rsc@post.harvard.edu>
Source: http://pdos.lcs.mit.edu/~rsc/software/libfmt-2.0.tgz
URL: http://pdos.lcs.mit.edu/~rsc/software/#libfmt
Requires: libutf

%description
Libfmt is a port of Plan 9's formatted print library.
As a base it provides all the syntax of ANSI printf
but adds the ability for client programs to install
new print verbs.  One such print verb (installed by
default) is %r, which prints the system error string.
Instead of perror("foo"), you can write fprint(2, "foo: %r\n"). 
This is especially nice when you write verbs to format
the data structures used by your particular program.
%prep
%setup

%build
make

%install
make install

%files
/usr/local/include/fmt.h
/usr/local/lib/libfmt.a
/usr/local/man/man3/print.3
/usr/local/man/man3/fmtinstall.3
