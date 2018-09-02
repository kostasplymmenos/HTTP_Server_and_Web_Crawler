#!/bin/bash

total=0
for file_index in log/*
	do
	if [ -e $file_index ]
		#  exists  file
		then if [ -f $file_index ]
			# is a  regular  file
			then  echo  File Exists
		else
			echo  File Error
			exit
		fi
	else
		echo  File Does not exist
		exit
	fi

	list=$(cut -d':' -f4 $file_index)
	for item in $list
		do
		if [ $item == "/search" ]
			then total=$((total+1))
		fi
		done
	done
echo "Total Searches : " $total
