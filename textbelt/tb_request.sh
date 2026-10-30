#!/bin/bash

# Script to send a text message request to the textbelt api

# Test for correct number of arguments passed into the script
# if [ $# -lt 2 ]; then
# 	echo "ERROR: invalid number of arguments."
# 	echo "Total number of arguments should be 2."
# 	echo "The order of the arguments should be:"
# 	echo "	1. The full path to the write file (including filename)"
# 	echo "	2. The string to be written to the write file"
# 	exit 1
# fi

# Test for correct number of arguments passed into the script
if [ $# -lt 2 ]; then
	echo "ERROR: invalid number of arguments."
	echo "Total number of arguments should be 2."
	echo "The order of the arguments should be:"
	echo "	1. The message string to send (in double quotes)"
    echo "	2. The phone number to send the message to (no dashes)"
	exit 1
fi

MSG=$1
#MSG="Hello World!!!!!!!"

PHONE=$2
#PHONE=9137355704

APIKEY=$(cat /home/ryan/projects/ecea5307-final-project/textbelt/text_belt_api_key.txt)
# echo $APIKEY


curl -X POST https://textbelt.com/text \
     --data-urlencode phone=$PHONE \
     --data-urlencode message="$MSG" \
     -d key=$APIKEY