#!/bin/bash
#Description: finder.sh shell script
#Author: Ashwini Patil
#Date  : 08/05/2023
#Reference: referred chatGPT to understand how to write the script


# path to a directory on the filesystem
filesdir=$1   
# text string that need to search in files
searchstr=$2

# Check if the script got exactly 2 arguments or not
if [ $# -ne 2 ]
then
    echo "Invalid number of arguments"
    exit 1
fi

# Check if the specified directory exists or not
if [ ! -d "$1" ]; then
    echo Invalid directory
    exit 1
fi


# Use find to search for files containing searchstr
num_matching_files="$(find "$filesdir" -type f | wc -l)"

# Count the number of matching lines
num_matching_lines="$(grep -rnw "$filesdir" -e "$searchstr" | wc -l)"

# Print the results in the expected format
echo The number of files are $num_matching_files and the number of matching lines are $num_matching_lines

#echo The number of files are $(grep -r -l "$searchstr" "$filesdir" | wc -l) and the number of matching lines are $(grep -r "$searchstr" "$filesdir" | wc -l)
exit 0