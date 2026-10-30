#!/bin/sh		
# above updated 11/3/2025 for assignment 3 part 2 qemu build (busybox doesn't have bash, just sh)

###############################################
#
#	UC Boulder ECE 5305
#
#	Assignment# 1: Bash Scripting Basics
#
#	File: finder.sh
#
#	Author: Ryan Challacombe
#	Date: 10/5/2025
#
#
###############################################

######## Assignment Instructions ##############

#Accepts the following runtime arguments: the first argument is a path to a directory on the filesystem, referred to below as filesdir; the second argument is a text string which will be searched within these files
#referred to below as searchstr

#Exits with return value 1 error and print statements if any of the parameters above were not specified

#Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem

#Prints a message "The number of files are X and the number of matching lines are Y" where X is the number of files in the directory and all subdirectories and Y is the number of matching lines found in respective files, where a matching line refers to a line which contains searchstr (and may also contain additional content)

###############################################

# Test for correct number of arguments passed into the script
if [ $# -lt 2 ]; then
	echo "ERROR: invalid number of arguments."
	echo "Total number of arguments should be 2."
	echo "The order of the arguments should be:"
	echo "	1. The file directory path."
	echo "	2. The string to be searched in the directory path."
	exit 1
fi

num_files=$( find $1 -type f | wc -l )
num_lines=$( grep -r $2 $1 | wc -l )


echo "The number of files are $num_files and the number of matching lines are $num_lines."


exit 0

