#!/bin/bash
#Description: writer.sh shell script
#Author: Ashwini Patil
#Date  : 08/05/2023
#Reference: referred chatGPT to understand how to write the script and to debug & solve errors

# Check if the script got exactly 2 arguments or not
if [ $# -ne 2 ]
then
    echo "Invalid number of arguments"
    exit 1
fi

# a full path to a file including the filename on the filesystem
writefile=$1
# text string that need to write into the file
writestr=$2

# Check if the write operation was successful
if [ -z "$writestr" ]
then
    echo "Write file path or string is not specified"
    exit 1
fi

# Create the directory path if it doesn't exist
writefile_dir=$(dirname "$writefile")
if [ ! -d "$writefile_dir" ]; then
    mkdir -p "$writefile_dir"
fi

# Check if the write operation was successful
if [ -f "$writefile" ]
then
# Attempt to write the content to the file
    echo "$writestr" > "$writefile"
    echo "Write operation is successful"
    exit 0
else
    echo "File does not exit so create new file"
    touch "$writefile"
    echo "$writestr" > "$writefile"
    exit 0
fi

exit 0