Summary: Port of Plan 9's UTF8 support functions
Name: libutf
Version: 2.0
Release: 1
Group: Development/C
Copyright: Public Domain
Packager: Russ Cox <rsc@post.harvard.edu>
Source: http://pdos.lcs.mit.edu/~rsc/software/libutf-2.0.tgz
URL: http://pdos.lcs.mit.edu/~rsc/software/#libutf

%description
Libutf is a port of Plan 9's UTF8 support functions.
%prep
%setup

%build
make

%install
make install

%files
/usr/local/include/utf.h
/usr/local/lib/libutf.a
/usr/local/man/man3/runestrcat.3
/usr/local/man/man3/isalpharune.3
/usr/local/man/man3/rune.3
/usr/local/man/man7/utf.7
