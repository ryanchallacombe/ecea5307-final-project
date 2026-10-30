#!/usr/bin/bash

###############################################
#
#	UC Boulder ECE 5305
#
#	Assignment# 1: Bash Scripting Basics
#
#	File: writer.sh
#
#	Author: Ryan Challacombe
#	Date: 10/5/2025
#
#
###############################################

######## Assignment Instructions ##############

# Accepts the following arguments: the first argument is a full path to a file (including filename) 
# on the filesystem, referred to below as writefile; the second argument is a text string which will 
# be written within this file, referred to below as writestr

# Exits with value 1 error and print statements if any of the arguments above were not specified

# Creates a new file with name and path writefile with content writestr, overwriting any existing file 
# and creating the path if it doesn’t exist. Exits with value 1 and error print statement if the file 
# could not be created.

###############################################

# Test for correct number of arguments passed into the script
if [ $# -lt 2 ]; then
	echo "ERROR: invalid number of arguments."
	echo "Total number of arguments should be 2."
	echo "The order of the arguments should be:"
	echo "	1. The full path to the write file (including filename)"
	echo "	2. The string to be written to the write file"
	exit 1
fi

fullpath=$1
#echo "fullpath = $fullpath"

fname=$(basename $fullpath)
#echo "File name = $fname"

dir_path=$(dirname $fullpath)
#echo "Directory = $dir_path"

# If path does not esist, create it
# also the file will no exist, so create it
if [ ! -d $dir_path ]; then
	#echo "Path $dir_path does not exist"
	mkdir -p $dir_path
	touch $fullpath
fi

# Write to file
echo "$2" > $fullpath

exit 0 
