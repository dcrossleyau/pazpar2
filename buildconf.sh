#!/bin/sh
# $Id: buildconf.sh,v 1.1 2007-01-08 12:43:41 adam Exp $

automake=automake
aclocal=aclocal
autoconf=autoconf
libtoolize=libtoolize
autoheader=autoheader

if [ "`uname -s`" = FreeBSD ]; then
    # FreeBSD intalls the various auto* tools with version numbers
    echo "Using special configuration for FreeBSD ..."
    automake=automake19
    aclocal="aclocal19 -I /usr/local/share/aclocal"
    autoconf=autoconf259
    libtoolize=libtoolize15
    autoheader=autoheader259
fi
if $automake --version|head -1 |grep '1\.[4-7]'; then
    echo "automake 1.4-1.7 is active. You should use automake 1.8 or later"
    if test -f /etc/debian_version; then
	echo " sudo apt-get install automake1.9"
	echo " sudo update-alternatives --config automake"
    fi
    exit 1
fi

set -x

# I am tired of underquoted warnings for Tcl macros
$aclocal -I m4 2>&1 | grep -v aclocal/tcl.m4
$autoheader
$automake --add-missing 
$autoconf
set -
if [ -f config.cache ]; then
    rm config.cache
fi

enable_configure=false
enable_help=true
sh_flags=""
conf_flags=""
case $1 in
    -d)
    sh_flags="-g -Wall"
    enable_configure=true
    enable_help=false
    shift
    ;;
    -c)
    sh_flags=""
    enable_configure=true
    enable_help=false
    shift
    ;;
esac

if $enable_configure; then
    if test -n "$sh_flags"; then
	CFLAGS="$sh_flags" ./configure $*
    else
	./configure $*
    fi
fi
if $enable_help; then
    cat <<EOF
    
Build the Makefiles with the configure command.
  ./configure [--someoption=somevalue ...]
  
For help on options or configuring run
  ./configure --help

Build and install binaries with the usual
  make
  make check
  make install

Build distribution tarball with
  make dist

Verify distribution tarball with
  make distcheck

Or just build the Debian packages without configuring
  dpkg-buildpackage -rfakeroot

When building from a CVS checkout, you need these Debian tools:
  libyaz-dev

EOF
fi
# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
