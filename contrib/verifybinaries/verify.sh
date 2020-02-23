#!/bin/bash
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

###   This script attempts to download the signature file SHA256SUMS.asc from
###   bitcoincore.org and bitcoin.org and compares them.
###   It first checks if the signature passes, and then downloads the files specified in
###   the file, and checks if the hashes of these files match those that are specified
###   in the signature file.
###   The script returns 0 if everything passes the checks. It returns 1 if either the
###   signature check or the hash check doesn't pass. If an error occurs the return value is 2

function clean_up {
   for file in $*
   do
      rm "$file" 2> /dev/null
   done
}

WORKINGDIR="/tmp/bitcoin_verify_binaries"
TMPFILE="hashes.tmp"

SIGNATUREFILENAME="SHA256SUMS.asc"
RCSUBDIR="test"
HOST1="https://bitcoincore.org"
HOST2="https://bitcoin.org"
BASEDIR="/bin/"
VERSIONPREFIX="bitcoin-core-"
RCVERSIONSTRING="rc"

if [ ! -d "$WORKINGDIR" ]; then
   mkdir "$WORKINGDIR"
fi

cd "$WORKINGDIR" || exit 1

#test if a version number has been passed as an argument
if [ -n "$1" ]; then
   #let's also check if the version number includes the prefix 'bitcoin-',
   #  and add this prefix if it doesn't
   if [[ $1 == "$VERSIONPREFIX"* ]]; then
      VERSION="$1"
   else
      VERSION="$VERSIONPREFIX$1"
   fi

   STRIPPEDLAST="${VERSION%-*}"

   #now let's see if the version string contains "rc" or a platform name (e.g. "osx")
   if [[ "$STRIPPEDLAST-" == "$VERSIONPREFIX" ]]; then
      BASEDIR="$BASEDIR$VERSION/"
   else
      # let's examine the last part to see if it's rc and/or platform name
      STRIPPEDNEXTTOLAST="${STRIPPEDLAST%-*}"
      if [[ "$STRIPPEDNEXTTOLAST-" == "$VERSIONPREFIX" ]]; then

         LASTSUFFIX="${VERSION##*-}"
         VERSION="$STRIPPEDLAST"

         if [[ $LASTSUFFIX == *"$RCVERSIONSTRING"* ]]; then
            RCVERSION="$LASTSUFFIX"
         else
            PLATFORM="$LASTSUFFIX"
         fi

      else
         RCVERSION="${STRIPPEDLAST##*-}"
         PLATFORM="${VERSION##*-}"

         VERSION="$STRIPPEDNEXTTOLAST"
      fi

      BASEDIR="$BASEDIR$VERSION/"
      if [[ $RCVERSION == *"$RCVERSIONSTRING"* ]]; then
         BASEDIR="$BASEDIR$RCSUBDIR.$RCVERSION/"
      fi
   fi
else
   echo "Error: need to specify a version on the command line"
   exit 2
fi

#first we fetch the file containing the signature
WGETOUT=$(wget -N "$HOST1$BASEDIR$SIGNATUREFILENAME" 2>&1)

#and then see if wget completed successfully
if [ $? -ne 0 ]; then
   echo "Error: couldn't fetch signature file. Have you specified the version number in the following format?"
   echo "[$VERSIONPREFIX]<version>-[$RCVERSIONSTRING[0-9]] (example: ${VERSIONPREFIX}0.10.4-${RCVERSIONSTRING}1)"
   echo "wget output:"
   echo "$WGETOUT"|sed 's/^/\t/g'
   exit 2
fi

WGETOUT=$(wget -N -O "$SIGNATUREFILENAME.2" "$HOST2$BASEDIR$SIGNATUREFILENAME" 2>&1)
if [ $? -ne 0 ]; then
   echo "bitcoin.org failed to provide signature file, but bitcoincore.org did?"
   echo "wget output:"
   echo "$WGETOUT"|sed 's/^/\t/g'
   clean_up $SIGNATUREFILENAME
   exit 3
fi

SIGFILEDIFFS="$(diff $SIGNATUREFILENAME $SIGNATUREFILENAME.2)"
if [ "$SIGFILEDIFFS" != "" ]; then
   echo "bitcoin.org and bitcoincore.org signature file