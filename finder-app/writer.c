#include<stdio.h>
#include<stdlib.h>
#include<syslog.h>


int main(int argc, char *argv[]){
    openlog("writer-log", 0, LOG_USER);   //initializing syslog to log messages

    if(argc<3 || argc>3) {
        syslog(LOG_ERR, "Invalid number of arguments received. %d arguments received instead of 2", argc-1);
        return 1;
    }

    FILE *file;
    char *fileName = argv[1];  
    char *writeStr = argv[2];

    file = fopen(fileName, "w");      // open the file in a writing mode

     if(file == NULL) {
        syslog(LOG_ERR, "Could not open specified file");
        return 1;
    }

    int result = fputs(writeStr, file);
    switch (result) {
       case EOF:
          syslog(LOG_ERR, "Error writing into file");
            break;
        default:
            syslog(LOG_DEBUG, "Writing %s to %s\nSuccessfully written into the file", writeStr, fileName);
            break;
    }

    fclose(file);
    syslog(LOG_INFO, "File closed successfully\n");
    return 0;
}

