#!/bin/bash


INPUT_FILE=''

while getopts 'f:s:' flag; do
  case "${flag}" in
    f) INPUT_FILE="${OPTARG}" ;;
    s) SOUND_FILE="${OPTARG}" ;;
      *) echo Flags mismatch
       exit 1 ;;
  esac
done

if [[ ! -f $INPUT_FILE ]] ; then
  echo Input file $INPUT_FILE is not readable
  exit 1
fi

cat $INPUT_FILE | grep -E '<wpt.+?>' | sed 's/[^0-9\.\ ]*//g' | sed -e 's/^[[:space:]]*//' | sed 's/\"//g' | awk -F' ' -v ID=1 '{print "{\n  \"id\": " ID++ ",\n  \"lat\": " $1 ",\n  \"lon\": " $2 "\n  \"path\": <replace>\n" "},"}' 
