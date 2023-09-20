//References- https://stackoverflow.com/questions/19099663/how-to-correctly-use-fork-exec-wait
//            https://vitux.com/fork-exec-wait-and-exit-system-call-explained-in-linux/
//            referred chatgpt to understand how fork(), execv(), wait_pid() works


#include "systemcalls.h"
#include <syslog.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>



/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int flag = system(cmd);

    if(flag == 0){
        return true;
    }
    
    return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    pid_t fork_status;
    int exec_status = 0;
    int wait_status = 0;
    int status;

    fork_status = fork();
    
    if(fork_status == -1){
        syslog(LOG_ERR, "Fork failed");
        exit(EXIT_FAILURE);
    }

    else if(fork_status == 0){
        syslog(LOG_INFO, "Child process PID: %d", getpid());
        exec_status = execv(command[0], command);
        if(exec_status == -1){
            syslog(LOG_ERR, "execv failed");
            exit(EXIT_FAILURE);
        }
    }

    wait_status = waitpid(fork_status,&status,0);

    if(wait_status == -1){
        syslog(LOG_ERR, "Error occured while waiting for child process");
        return false;
    } 
    else if(WIFEXITED(status)){
        int exit_status = 0;
        exit_status=WEXITSTATUS(status);
        if(exit_status != 0){
            syslog(LOG_ERR, "Child process is not terminated");
            return false;
        }
    }

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    pid_t fork_status;
    int exec_status = 0;
    int wait_status = 0;
    int status;

    int fd_status = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if(fd_status == -1){
        syslog(LOG_ERR, "Failed to open the output file");
        return false;
    }

    if (dup2(fd_status, 1) == -1){
        syslog(LOG_ERR,"duplication failed");
        return false;
        close(fd_status);
    }

    fork_status = fork();
    
    if(fork_status == -1){
        syslog(LOG_ERR, "Fork failed");
        return false;
    }

    else if(fork_status == 0){
        syslog(LOG_INFO, "Child process PID: %d", getpid());
        exec_status = execv(command[0], command);
        if(exec_status == -1){
            syslog(LOG_ERR, "execv failed");
            exit(EXIT_FAILURE);
        }
    }

    wait_status = waitpid(fork_status,&status,0);

    if(wait_status == -1){
        syslog(LOG_ERR, "Error occured while waiting for child process");
        return false;
    } 
    else if(WIFEXITED(status)){
        int exit_status = 0;
        exit_status=WEXITSTATUS(status);
        if(exit_status != 0){
            syslog(LOG_ERR, "Child process is not terminated");
            return false;
        }
    }    
 
    va_end(args);

    return true;
}
