#!/bin/bash

list=""
cmdstring=''
for file_index in log/*
	do
	if [ -e $file_index ]
		#  exists  file
		then if [ -f $file_index ]
			# is a  regular  file
			then  echo  "Reading worker file : " $file_index
		else
			echo  File Error
			exit
		fi
	else
		echo  File Does not exist
		exit
	fi
	cmdstring+=" <(cut -d':' -f5- $file_index) "
	
	done

threshold=1

cmd="paste $cmdstring | sort -u | awk '{ FS = \":\" } ; { if (NF-1>0) print NF-1,\$1 } ;' | sort -rn | tail -1"
echo "Executing : " $cmd $'\n'
echo "Min Word"
eval $cmd
