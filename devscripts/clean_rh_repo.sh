#!/bin/bash
#
# Clean up older RPMS on MDSplus Redhat Distribution
#
#
# Format:
#
# clean_rh_repo dist branch number-of-most-recent-releases-to-keep
# 
# Examples:
#
# clean_rh_repo fc25 stable 20
# clean_rh_repo all alpha 10
# clean_rh_repo all all 10
#
basedir=/mdsplus/dist
dist=$1
branch=$2
keep=$3
if [ "$dist" == "all" ]
then
  for distdir in $(ls -d ${basedir}/fc* ${basedir}/el*)
  do
    dist=$(basename $distdir)
    $0 $dist $branch $keep
  done
else
  if [ "$branch" == "all" ]
  then
    for branch in alpha stable
    do
      $0 $dist $branch $keep
    done
  else
    if [ -z "$keep" ]
    then
      keep=10
    fi
    pushd ${basedir}/${dist}/${branch}
    rsync -a RPMS/* RPMS_OLD
    pushd RPMS
    for rpm in $(dnf repomanage -k $keep --new --space .)
    do
      rsync -aR ${rpm} ../RPMS_NEW/
    done
    popd
    if ( createrepo --update --deltas RPMS_NEW )
    then
      mv RPMS RPMS_DEL
      mv RPMS_NEW RPMS
      rm -Rf RPMS_DEL
    fi
    popd
  fi
fi

