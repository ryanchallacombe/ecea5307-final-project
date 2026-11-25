#!/bin/bash

# Script to send a text message request to the textbelt api
# 	example call: ./tb_request.sh "TGIF!" 9137355704

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
PHONE=$2

# path to the api key file on the native vm
KEY_FILE_VM=/home/ryan/projects/ecea5307-final-project/client/include/text_belt_api_key.txt
# path to the api key file on the embedded system
KEY_FILE_RPI4=/aesd_client_include/text_belt_api_key.txt

if [ -f "$KEY_FILE_VM" ]; then
	APIKEY=$(cat $KEY_FILE_VM)
else
	APIKEY=$(cat $KEY_FILE_RPI4)
fi

curl -X POST https://textbelt.com/text \
     --data-urlencode phone=$PHONE \
     --data-urlencode message="$MSG" \
     -d key=$APIKEY \
     -k 