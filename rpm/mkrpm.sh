#!/bin/bash

name=bufu_filebroker
version=2.0.0.alpha.1
release=0.centos7

SRC="../dist"
DEST="${name}-${version}"

mkdir "$DEST"

#----------
# copy files which should be packaged into
# a temporary directory

rsync -a -v "$SRC/" "$DEST"
cp "./${name}.spec" "$DEST"

# create an archive from the files to be packaged
tar cvfz ./${name}-${version}.tgz ./${name}-${version}

#----------
mkdir -p RPMBUILD/{RPMS/{i386,i586,i686,x86_64},SPECS,BUILD,SOURCES,SRPMS}
rpmbuild --define "_topdir `pwd`/RPMBUILD"  \
         --define "ver ${version}"        \
         --define "rel ${release}"        \
         --define "name ${name}"        \
	 -ts ./${name}-${version}.tgz
rpmbuild --define "_topdir `pwd`/RPMBUILD" \
         --define "ver ${version}"        \
         --define "rel ${release}"        \
         --define "name ${name}"        \
	 -tb ./${name}-${version}.tgz
find RPMBUILD/ -type f -name "*.rpm" -exec mv {} . \;
rm -rf RPMBUILD/ ${name}-${version}/ ./${name}-${version}.tgz
