#!/bin/bash
#Description: writer.sh shell script
#Author: Ashwini Patil
#Date  : 08/05/2023
#Reference: referred chatGPT to understand how to write the script

# a full path to a file including the filename on the filesystem
writefile="$1"
# text string that need to write into the file
writestr="$2"

# Check if the script got exactly 2 arguments or not
if [ "$#" -eq 2 ]; then
    echo "Correct arguments are passed"
else
    echo "ERROR: Invalid number of arguments"
    exit 1
fi 

# Check if writefile or writestr is empty or not
if [ -z "$writefile" ] || [ -z "$writestr" ]; then
    echo "Error: Both file path and text to write must be specified"
    exit 1
fi

# Create the directory path if it doesn't exist
writefile_dir=$(dirname "$writefile")
if [ ! -d "$writefile_dir" ]; then
    mkdir -p "$writefile_dir"
fi

# Attempt to write the content to the file
echo "$writestr" > "$writefile"

# Check if the write operation was successful
if [ $? -ne 0 ]; then
    echo "Error: Could not write to '$writefile'"
    exit 1
fi

# Print a success message
echo "Successfully wrote '$writestr' to '$writefile'"

exit 0
