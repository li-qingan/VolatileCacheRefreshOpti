#!/bin/bash

# args checking
if [ $# -lt 1 ]
then
	echo "Argument error!"
	echo "Ex1: ./result.sh \"Total refresh\""
	echo "Ex2: ./result.sh \"Total cycles\""
	exit 1
fi


FIELD=$1
find . -name "*.stats" | xargs grep "${FIELD}"



