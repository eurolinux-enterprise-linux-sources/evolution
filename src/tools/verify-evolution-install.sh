#!/bin/sh
#
# Verifies that Evolution and all its supporting components
# are installed correctly. A tool to weed out common
# build problems.
#
# (C)2000 Helix Code, Inc.
# Author: Peter Williams <peterw@helixcode.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

problem() {
    echo "I detected the following problem: $problem"
    if test x"$rpmsystem" = xyes ; then
	echo "Suggested solution via RPM: $rpmsolution"
    fi
    if test x"$debsystem" = xyes ; then
	echo "Suggested solution via DEB: $debsolution"
    fi
    echo "Suggested solution via sources: $srcsolution"
    if test x"$comment" != x ; then
	echo ""
	echo "$comment"
    fi
    exit 1
}

check_config() {
    #bigname=$1
    #cfgname=$2
    #pkgname=$3
    eval $1=\${$1-$2}

    eval val=\$$1
    if type $val </dev/null 1>/dev/null 2>&1 ; then
	:
    else
	problem="Cannot find $2 or it ($val) is not executable"
	rpmsolution="Install or reinstall the '$3-devel' package"
	debsolution="Install or reinstall the $3 development libraries." #FIXME
	srcsolution="Get the latest release of $3 and install it."
	comment="If you know that $3 is installed, try setting the
environment variable $1 to its location"
	problem
    fi
}

check_prefix() {
    #othercfg=$1
    #othername=$2
    #strict=$3

    eval otherpfx=\`\$$1 --prefix\`
    if test x"$3" = xstrict ; then
	if test x"$otherpfx" != x"$gl_prefix" ; then
	    problem="gnome-libs and $2 do not share the same prefix"
	    rpmsolution="This problem shouldn't happen with RPM installations. Verify your installation of Helix Gnome."
	    debsolution="This problem shouldn't happen with DEB installations. Verify your installation of Helix Gnome."
	    srcsolution="Re-run 'configure' in $2's source directory with the flag '--prefix=$gl_prefix'."
	    problem
	fi
    else
	IFSbak="$IFS"
	ok="$GNOME_PATH:$gl_prefix"
	IFS=":"
	passed=no

	for e in $ok; do
	    if test x"$e" != x ; then
		if test $otherpfx = $e ; then
		    passed=yes;
		fi
	    fi
	done

	IFS="$IFSbak"

	if test x"$passed" = xno ; then
	    problem="$2 is not in GNOME_PATH or the same prefix as gnome-libs"
	    rpmsolution="This problem shouldn't happen with RPM installations. Verify your installation of Helix Gnome."
	    debsolution="This problem shouldn't happen with DEB installations. Verify your installation of Helix Gnome."
	    srcsolution="Re-run 'configure' in $2's source directory with the flag '--prefix=$gl_prefix'."
	    comment="Try exporting an environment variable 'GNOME_PATH' with the prefix of $2."
	    problem
	fi
    fi
}

check_sysconf() {
    #othercfg=$1
    #othername=$2

    eval othersysc=\`\$$1 --sysconfdir\`
    if test x"$othersysc" != x"$gl_sysconf" ; then
	problem="gnome-libs and $2 do not share the same sysconfdir"
	rpmsolution="This problem shouldn't happen with RPM installations. Verify your installation of Helix Gnome."
	debsolution="This problem shouldn't happen with DEB installations. Verify your installation of Helix Gnome."
	srcsolution="Re-run 'configure' in $2's source directory with the flag '--sysconfdir=$gl_sysconf'."
	problem
    fi
}

check_oafinfo() {
    #basename=$1
    #othername=$2

    base=$1.oafinfo
    search="${oaf_prefix}/share/oaf:$OAF_INFO_PATH"
    IFSback="$IFS"
    IFS=":"
    ok=no

    for ping in $search ; do
	if test x"$ping" != x ; then
	    if test -f $ping/$base ; then
		file=$ping/$base
		ok=yes
	    fi
	fi
    done

    IFS="$IFSback"

    if test x$ok = xno ; then
	problem="$1.oafinfo isn't installed into Gnome's prefix or in OAF_INFO_PATH"
	rpmsolution="This problem shouldn't happen with RPM installations. Verify your installation of Helix Gnome."
	debsolution="This problem shouldn't happen with DEB installations. Verify your installation of Helix Gnome."
	srcsolution="Re-run 'configure' in $2's source directory with the flag '--datadir=$gl_datadir'."
	comment="Another likely cause of this problem would be a failed installation of $2.
You should check to see that the install succeeded. You may also add the
location of $1.oafinfo to the environment variable OAF_INFO_PATH"
	problem
    fi

    iids=`cat $file |grep iid= |sed -e 's,.*iid="\([^"]*\)".*,\1,'`
    IFS="
"

    tempfile="temp-$$-verifier"
    for iid in $iids ; do
	#echo "Attempting to activate IID \"$iid\"..."
	$OAF_CLIENT -qs "iid == '$iid'" >$tempfile
	result=`cat $tempfile |grep exception`
	if test x"$result" != x ; then
	    problem="The component $2 (in $file) couldn't be activated by OAF"
	    rpmsolution="Verify that $file is valid and that oaf and $2 are correctly installed."
	    debsolution="Verify that $file is valid and that oaf and $2 are correctly installed."
	    srcsolution="Verify that $file is valid and that oaf and $2 are correctly installed."
	    comment="$OAF_CLIENT reported this:
`cat $tempfile`"
	    rm -f $tempfile
	    problem
        fi
    done
    rm -f $tempfile

    IFS="$IFSback"
}

check_bin() {
    #name=$1
    #othername=$2

    IFSbak="$IFS"
    IFS=":"
    passed=no
    exepath=

    for ping in $PATH; do    
	if test x"$ping" != x -a -x $ping/$1 ; then
	    if test x"$passed" = xyes ; then
		problem="The binary $1 is installed in more than one location"
		rpmsolution="Make sure that you only have one copy of the package installed"
		debsolution="Make sure that you only have one copy of the package installed"
		srcsolution="Make sure that you only have one copy of the package installed"
		comment="You probably have a package that was installed in two different prefixes,
either from source twice or from source and from RPM/deb. Remove the older copy.
(Note: \"make uninstall\" works on tarballs but may damage your gettext installation)"
		problem
	    fi
	    passed=yes;
	    exepath="$ping/$1"
	fi
    done

    IFS="$IFSbak"

    if test x"$passed" = xno ; then
	problem="The binary $1 isn't in your PATH"
	rpmsolution="This problem shouldn't happen with RPM installations. Verify your installation of Helix Gnome."
	debsolution="This problem shouldn't happen with DEB installations. Verify your installation of Helix Gnome."
	srcsolution="Re-run 'configure' in $2's source directory with the flag '--bindir=$gl_bindir'."
	comment="Another likely cause of this problem would be a failed installation of $2.
You should check to see that the install succeeded."
	problem
    fi

    lddfile=temp-$$-verify-ldd
    $LDD $exepath </dev/null >$lddfile 2>/dev/null

    if test x$? != x0 ; then
	problem="The binary $1 isn't linked correctly"
	rpmsolution="Make sure that all its dependencies are installed correctly"
	debsolution="Make sure that all its dependencies are installed correctly"
	srcsolution="Recompile $2 and make sure that it links correctly."
	comment="LDD reported the following:
`cat $lddfile`
"
	problem
    fi

    IFS="
"

    use_bonobox=no
    use_bonobo=no

    for ping in `cat $libfile` ; do
	libname=`echo "$ping" |sed -e 's,^@@\([^@][^@]*\)@@[^@][^@]*@@,\1,'`
	libpfx=`echo "$ping" |sed -e 's,^@@[^@][^@]*@@\([^@][^@]*\)@@,\1,'`

	match="`cat $lddfile |grep \"${libname}\.\"`"

	if test x"$match" != x ; then
	    theirlib="`echo $match |sed -e 's,.*=> \([^ ]*\).*$,\1,'`"
	    theirdir="`dirname $theirlib`"

	    if test x"$theirdir" != x"$libpfx" ; then
		problem="The binary $1 is linked against the wrong copy of $libname"
		rpmsolution="The package owning $libname is probably installed twice. Make sure that it's installed correctly."
		debsolution="The package owning $libname is probably installed twice. Make sure that it's installed correctly."
		srcsolution="The package owning $libname is probably installed twice. Make sure that it's installed correctly."
		comment="The version of $libname in $libpfx was expected"
		problem
	    elif test x"$libname" = xlibbonobox ; then
		use_bonobox=yes
	    elif test x"$libname" = xlibbonobo ; then
		use_bonobo=yes
	    fi
	fi
    done

    rm -f $lddfile
    IFS="$IFSback"

    if test x"$need_bonobox" = xyes -a x"$use_bonobo" = xyes -a x"$use_bonobox" != xyes ; then
	problem="The binary $1 is linked against Bonobo but not Bonobo-X"
	rpmsolution="Get the newest possible version of $2, or install it from source if that fails"
	debsolution="Get the newest possible version of $2, or install it from source if that fails"
	srcsolution="Run './config.status --recheck' in $2's source directory and reinstall it"
	problem
    fi
}

check_no_gnorba() {
    #libs=$1
    #othername=$2

    ping=`echo $1 |grep 'gnorba'`

    if test x"$ping" != x ; then
	problem="$2 was built using Gnorba, not OAF"
	rpmsolution="This problem shouldn't happen with RPM installations. Verify your installation of Helix Gnome."
	debsolution="This problem shouldn't happen with DEB installations. Verify your installation of Helix Gnome."
	srcsolution="Update $2 and re-run 'configure' in its source directory with the flag '--enable-oaf=yes'."
	problem
    fi
}

add_lib_to_check() {
    #lib = $1
    #path = $2

    echo "@@${1}@@${2}@@" >>$libfile
}

########################################

versionparse3() {
    #inst_version=$1
    #reqd_version=$2
    #package=$3

    inst_version=`versiongrab3 "$1"`
    inst_major=`echo $inst_version |sed -e 's,\([0-9][0-9]*\)\..*\..*,\1,'`
    inst_minor=`echo $inst_version |sed -e 's,.*\.\([0-9][0-9]*\)\..*,\1,'`
    inst_micro=`echo $inst_version |sed -e 's,.*\..*\.\([0-9][0-9]*\),\1,'`

    reqd_version=`versiongrab3 "$2"`
    reqd_major=`echo $reqd_version |sed -e 's,\([0-9][0-9]*\)\..*\..*,\1,'`
    reqd_minor=`echo $reqd_version |sed -e 's,.*\.\([0-9][0-9]*\)\..*,\1,'`
    reqd_micro=`echo $reqd_version |sed -e 's,.*\..*\.\([0-9][0-9]*\),\1,'`

    ok=yes
    if test $inst_major -lt $reqd_major ; then
	ok=no
    elif test $inst_major -gt $reqd_major ; then
	ok=yes
    elif test $inst_minor -lt $reqd_minor; then
	ok=no
    elif test $inst_minor -gt $reqd_minor; then
	ok=yes
    elif test $inst_micro -lt $reqd_micro; then
	ok=no
    fi

    if test x$ok = xno ; then
	problem="Package $3 is not new enough ($1 detected, $2 required)"
	rpmsolution="Obtain a newer version and install it"
	depsolution="Obtain a newer version and install it"
	srcsolution="Obtain a newer version and install it"
	comment="If you think you have a newer installation, make sure that the
package is not installed twice."
	problem
    fi
}

versiongrab3() {
    echo $1 |sed -e 's,.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*,\1,'
}

check_module3() {
    #$1=gnome-libs name
    #$2=package
    #$3=version
    $GNOME_CONFIG --modversion $1 1>/dev/null 2>/dev/null </dev/null
    if test x$? != x0 ; then
	problem="Package $2 doesn't seem to be installed."
	rpmsolution="Get and install the packages '$2' and '$2-devel'"
	debsolution="Get and install the package $2 and its development libraries" #FIXME
	srcsolution="Download the source and install the package $2"
	comment="If you think the package is installed, check that its prefix is $gl_prefix --
${1}Conf.sh should be installed into `$GNOME_CONFIG --libdir`"
	problem
    fi

    instversion=`$GNOME_CONFIG --modversion $1`

    #eew eew hacky
    # gnome-vfs used to be versioned gnome-vfs-0.1,
    # and gnome-vfs-0.2, but then they moved to three figs--
    # now it's gnome-vfs-0.2.0

    if test $2 = gnome-vfs ; then
	case "$instversion" in
	gnome-vfs-0.1)
	    problem="Package gnome-vfs is not new enough (0.1 detected, $3 required)"
	    rpmsolution="Obtain a newer version and install it"
	    depsolution="Obtain a newer version and install it"
	    srcsolution="Obtain a newer version and install it"
	    comment="If you think you have a newer installation, make sure that the
package is not installed twice."
	    problem
	    ;;
	gnome-vfs-0.2)
	    instversion="gnome-vfs-0.2.0"
	    ;;
	*)
	    #nothing, version is ok
	    ;;
	esac
    fi

    versionparse3 "$instversion" "$3" "$2"
}

########################################

versionparse2() {
    #inst_version=$1
    #reqd_version=$2
    #package=$3

    inst_version=`versiongrab2 "$1"`
    inst_major=`echo $inst_version |sed -e 's,\([0-9][0-9]*\)\..*,\1,'`
    inst_minor=`echo $inst_version |sed -e 's,.*\.\([0-9][0-9]*\),\1,'`

    reqd_version=`versiongrab2 "$2"`
    reqd_major=`echo $reqd_version |sed -e 's,\([0-9][0-9]*\)\..*,\1,'`
    reqd_minor=`echo $reqd_version |sed -e 's,.*\.\([0-9][0-9]*\),\1,'`

    ok=yes
    if test $inst_major -lt $reqd_major ; then
	ok=no
    elif test $inst_major -gt $reqd_major ; then
	ok=yes
    elif test $inst_minor -lt $reqd_minor; then
	ok=no
    fi

    if test x$ok = xno ; then
	problem="Package $3 is not new enough ($1 detected, $2 required)"
	rpmsolution="Obtain a newer version and install it"
	depsolution="Obtain a newer version and install it"
	srcsolution="Obtain a newer version and install it"
	comment="If you think you have a newer installation, make sure that the
package is not installed twice."
	problem
    fi
}

versiongrab2() {
    echo $1 |sed -e 's,.*\([0-9][0-9]*\.[0-9][0-9]*\).*,\1,'
}

check_module2() {
    #$1=gnome-libs name
    #$2=package
    #$3=version
    $GNOME_CONFIG --modversion $1 1>/dev/null 2>/dev/null </dev/null
    if test x$? != x0 ; then
	problem="Package $2 doesn't seem to be installed."
	rpmsolution="Get and install the packages '$2' and '$2-devel'"
	debsolution="Get and install the package $2 and its development libraries" #FIXME
	srcsolution="Download the source and install the package $2"
	comment="If you think the package is installed, check that its prefix is $gl_prefix --
${1}Conf.sh should be installed into `$GNOME_CONFIG --libdir`"
	problem
    fi

    instversion=`$GNOME_CONFIG --modversion $1`
    versionparse2 "$instversion" "$3" "$2"
}

########################################

#prep
libfile=temp-$$-libs-verifier

if test -d /var/lib/rpm ; then
    rpmsystem=yes
    RPM=${RPM_PROG-rpm}

    $RPM --version 1>/dev/null 2>/dev/null </dev/null
    if test x$? != x0 ; then
	problem="The rpm executable ($RPM) does not seem to work."
	rpmsolution="none, if rpm doesn't work."
	debsolution="not applicable."
	srcsolution="download and install rpm manually."
	comment="If rpm really won't work then there is something wrong with your system."
	problem
    fi
else
    rpmsystem=no
fi

if test -d /var/lib/dpkg ; then
    debsystem=yes
    #FIXME: check if deb works
else
    debsystem=no
fi

if test x"$GNOME_PATH" != x ; then
    PATH="$GNOME_PATH:$PATH"
fi

if test x"$LDD" = x ; then
    if ldd -r /lib/libc.so </dev/null 1>/dev/null 2>&1 ; then
	LDD="ldd -r"
    elif type ldd </dev/null 1>/dev/null 2>&1; then
	LDD="ldd"
    else
	problem="Cannot find ldd"
	rpmsolution="Make sure that glibc is correctly installed!?!?!?"
	debsolution="Make sure that glibc is correctly installed!?!?!?"
	srcsolution="See above"
	comment="If you think that you have ldd, export an environment
vairable LDD that points to it."
	problem
    fi
fi

#gnome-libs
check_config GNOME_CONFIG gnome-config gnome-libs

gl_version=`$GNOME_CONFIG --version` # |sed -e 's,.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*,\1,'`
gl_sysconf=`$GNOME_CONFIG --sysconfdir`
gl_prefix=`$GNOME_CONFIG --prefix`
gl_datadir=`$GNOME_CONFIG --datadir`
gl_bindir=`$GNOME_CONFIG --bindir`

versionparse3 "$gl_version" "1.0.59" "gnome-libs"
add_lib_to_check libgnome `$GNOME_CONFIG --libdir`
add_lib_to_check libgnomeui `$GNOME_CONFIG --libdir`

#libunicode
check_config UNICODE_CONFIG unicode-config libunicode
check_prefix UNICODE_CONFIG libunicode
versionparse2 "`$UNICODE_CONFIG --version`" "0.4" libunicode
add_lib_to_check libunicode "`$UNICODE_CONFIG --prefix`/lib"

#ORBit
check_config ORBIT_CONFIG orbit-config ORBit
check_prefix ORBIT_CONFIG ORBit
add_lib_to_check libORBit "`$ORBIT_CONFIG --prefix`/lib"

#oaf
check_config OAF_CONFIG oaf-config oaf
check_prefix OAF_CONFIG oaf
versionparse3 "`$OAF_CONFIG --version`" "0.4.0" "oaf"
add_lib_to_check liboaf "`$OAF_CONFIG --prefix`/lib"
check_bin oafd

OAF_CLIENT=${OAF_CLIENT-oaf-client}
if type $OAF_CLIENT </dev/null >/dev/null 2>&1 ; then
    :
else
    problem="oaf-client couldn't be found"
    rpmsolution="Install the 'oaf-devel' package"
    debsolution="Install the oaf development libraries"
    srcsolution="Make sure that oaf is installed correctly"
    comment="If you think you have oaf-client installed, try exporting an
environment variable OAF_CLIENT pointing to it"
    problem
fi

oaf_prefix=`$OAF_CONFIG --prefix`
check_oafinfo oafd oaf

#gconf
#check_config GCONF_CONFIG gconf-config GConf
#check_prefix GCONF_CONFIG GConf
#versionparse2 "`$GCONF_CONFIG --version`" "0.5" GConf
#if type gconfd-1 </dev/null >/dev/null 2>/dev/null; then
#    check_oafinfo gconfd-1 GConf
#    check_bin gconfd-1
#else
#    check_oafinfo gconfd GConf
#    check_bin gconfd
#fi
#check_no_gnorba "`$GCONF_CONFIG --libs`" GConf

#gnome vfs
check_module3 vfs gnome-vfs "0.2.0"
check_no_gnorba "`$GNOME_CONFIG --libs vfs`" gnome-vfs
add_lib_to_check libgnomevfs "`$GNOME_CONFIG --libdir vfs`"

#gnome print
check_module2 print gnome-print "0.20"
check_no_gnorba "`$GNOME_CONFIG --libs print`" gnome-print
add_lib_to_check libgnomeprint "`$GNOME_CONFIG --libdir print`"

#bonobo
check_module2 bonobo bonobo "0.15"
check_prefix "GNOME_CONFIG bonobo" bonobo strict
add_lib_to_check libbonobo "`$GNOME_CONFIG --libdir bonobo`"

if gnome-config --modversion bonobox </dev/null 1>/dev/null 2>&1 ; then
    need_bonobox=yes
    check_module2 bonobox bonobo "0.16"
    check_prefix "GNOME_CONFIG bonobox" bonobo strict
    add_lib_to_check libbonobox "`$GNOME_CONFIG --libdir bonobox`"
fi

check_oafinfo audio-ulaw bonobo
check_bin bonobo-audio-ulaw bonobo
check_no_gnorba "`$GNOME_CONFIG --libs bonobo`" bonobo

#gtkhtml
check_module2 gtkhtml GtkHTML "0.5"
add_lib_to_check libgtkhtml "`$GNOME_CONFIG --libdir html`"
check_oafinfo html-editor-control GtkHTML
check_bin html-editor-control GtkHTML
check_no_gnorba "`$GNOME_CONFIG --libs gtkhtml`" GtkHTML

#evolution

if test x"$EVOLUTION_PREFIX" = x ; then
    IFSback="$IFS"
    IFS=":"
    passed=no

    for ping in $PATH ; do
	if test x"$ping" != x -a -x $ping/evolution ; then
	    if test x"$passed" = xyes ; then
		problem="Evolution is installed in more than one prefix"
		rpmsolution="Make sure that you only have one copy of Evolution installed"
		debsolution="Make sure that you only have one copy of Evolution installed"
		srcsolution="Make sure that you only have one copy of Evolution installed"
		comment="Evolution was probably installed in two different prefixes,
either from source twice or from source and from RPM/deb. Remove the older copy.
(Note: \"make uninstall\" works on tarballs but may damage your gettext installation)"
		problem
	    else
		passed=yes
		e_prefix=$ping
	    fi
	fi
    done

    IFS="$IFSbak"

    if test x"$passed" != xyes ; then
	problem="Evolution does not appear to have been installed"
    	rpmsolution="Make sure that Evolution was installed properly (rpm -V evolution)"
	debsolution="Make sure that Evolution was installed properly"
	srcsolution="Run 'make install' in the evolution source directory"
        comment="I couldn't find evolution in this path: $PATH"
	problem
    fi

    e_prefix="`echo $e_prefix |sed -e 's,\(.*\)/[^/][^/]*,\1,'`"
    echo "Guessed that evolution is installed in prefix $e_prefix"
else
    e_prefix="$EVOLUTION_PREFIX"
fi

add_lib_to_check libcal-client "$e_prefix/lib"
add_lib_to_check libcamel "$e_prefix/lib"
add_lib_to_check libcamelvee "$e_prefix/lib/evolution/camel-providers/0.5"
check_oafinfo addressbook evolution
check_oafinfo calendar-control evolution
check_oafinfo evolution-mail evolution
check_oafinfo evolution-calendar evolution
check_oafinfo evolution-addressbook-select-names evolution
check_oafinfo wombat evolution
check_bin evolution-mail evolution
check_bin evolution-calendar evolution
check_bin evolution-addressbook evolution
check_bin wombat evolution
check_bin evolution evolution

#done
rm -f $libfile
echo "Your Gnome system appears to be properly set up. Enjoy Evolution!"
exit 0
