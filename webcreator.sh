#!/bin/bash

if [ -e $1 ]
	then if [ -d $1 ]
		then  echo  "Root_dir: " $1
		if [ "$(ls -A $1)" ]
			then "Dir not empty removing content"
			rm -rf $1/*
		else echo "Dir empty"
		fi
	else
		echo  "Arg 1 is not a dir"
		exit
	fi
else
	echo  "Dir not exists. Making new dir : " $1
	mkdir $1
fi

if [ -e $2 ]
	then if [ -f $2 ]
		then  echo  "Text_file : " $2
	else
		echo  "Arg 2 is not a file"
		exit
	fi
else
	echo  "File not exists"
	exit
fi

if [ $3 -eq $3 ]
	then echo "Arg 3 is an integer"
else
	echo "Arg 3 is not an integer exiting"
	exit	
fi

if [ $4 -eq $4 ]
	then echo "Arg 4 is an integer"
else
	echo "Arg 4 is not an integer exiting"
	exit	
fi

lines=$(wc < $2 | cut -d' ' -f2)

if [ $lines -lt 10000 ]
	then echo "File has only $lines lines exiting"
	exit
else
	echo "File has $lines lines"
fi

echo  "=== Starting Website Creator ==="
touch tempf.txt

for i in `seq 0 $(($3-1))`
do
	for j in `seq 0 $(($4-1))`
	do	
		echo $1/site$i/page$i\_$RANDOM.html:0 >> tempf.txt # 0 indicates if it has incoming link
	done
done

cat tempf.txt
echo "=========="
constant_f=$((($4/2)+1))
constant_q=$((($3/2)+1))

for sitei in `seq 0 $(($3-1))`	#for each site dir
	do
	echo "Generating Website $sitei."
	mkdir $1/site$sitei
	
	for pagei in `seq 0 $(($4-1))`	#for each page
		do
		#pick k,m constants
		constant_k=$((($RANDOM%($lines-2000))+1))
		constant_m=$((($RANDOM%1000)+1000))

		#pick internal links
		sed -n "$(($sitei*$4+1)),+$(($4-1))p" tempf.txt | cut -d':' -f1 > tempf2.txt	#pick internal links from file
		sed -i "$(($sitei*$4+1)),+$(($4-1))s/:./:1/ " tempf.txt	# the selected websites have at least one incoming link
		pagename=`sed "$(($pagei+1))!d" tempf2.txt`
		echo "Generating Page: $pagename"	#website to create
		sed  -i "$(($pagei+1))d" tempf2.txt	#remove current site from intenal links file
		shuf -o tempf4.txt < tempf2.txt		#shuffle the websites and pick constant_f internal links
		sed -n "1,$(($constant_f))p" tempf4.txt > tempf2.txt
	
		#pick external links
		sed "$(($sitei*$4+1)),$(($sitei*$4+$4))d" tempf.txt | cut -d':' -f1 > tempf3.txt
		sed -i "$(($sitei*$4+1)),$(($sitei*$4+$4))s/:./:1/ " tempf.txt
		shuf -o tempf4.txt < tempf3.txt
		sed -n "1,$(($constant_f))p" tempf4.txt > tempf3.txt
		cat tempf2.txt >> tempf3.txt	#tempf3.txt has the links to be written

		#write html header
		touch $pagename
		printf "<!DOCTYPE html>\n<html>\n\t<body>\n" > $pagename

		#write content and links
		i=0
		for i in `seq 0 $(($constant_f+$constant_q-1))`
			do
			nlines=$(($constant_m/($constant_f+$constant_q)))
			sed -n "$(($constant_k+$i*$nlines)),+$(($nlines))p" $2 >> $pagename #copy text to webpage
			printf "<a href=\"/" >> $pagename
			#printf "<a href=\"../../" >> $pagename
			sed -n "$(($i+1))p" tempf3.txt >> $pagename #copy link to webpage
			printf "\">link_$i</a>\n" >> $pagename
			echo "link_$i added to page"
		done
		#write closing tags		
		printf "\n\t</body>\n</html>" >> $pagename
	done
done

# we count the number of 1's in tempf.txt if the sum is equal to w*p all have incoming links
echo "==== DONE ===="
cut -d':' -f2 tempf.txt > tempfc.txt

sed -i 's/0//g' tempfc.txt
sed -i '/^$/d' tempfc.txt


sum=`wc -l tempfc.txt | awk '{print \$1}'`
if [ $sum -eq $(($3*$4)) ]
	then echo "All Webpages have at least one incoming link"
else
	echo "Some websites have not incoming link"
fi
rm tempf*.txt


