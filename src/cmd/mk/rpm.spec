Summary: Streamlined replacement for make
Name: mk
Version: 2.0
Release: 1
Group: Development/Utils
Copyright: Public Domain
Packager: Russ Cox <rsc@post.harvard.edu>
Source: http://pdos.lcs.mit.edu/~rsc/software/mk-2.0.tgz
URL: http://pdos.lcs.mit.edu/~rsc/software/#mk
Requires: libfmt libbio libregexp9 libutf

%description
Mk is a streamlined replacement for make, written for 
Tenth Edition Research Unix by Andrew Hume.

http://plan9.bell-labs.com/sys/doc/mk.pdf
%prep
%setup

%build
make

%install
make install

%files
/usr/local/doc/mk.pdf
/usr/local/man/man1/mk.1
/usr/local/bin/mk
