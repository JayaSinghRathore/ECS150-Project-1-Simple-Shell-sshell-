
/********************************************************************************************
 * Project Name: Simple Shell Advance
 * Group Memebrs:
 * Course: ECS150
 *********************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <fcntl.h>

#define CMDLINE_MAX 512

/********************************************************************************************
 * Function Name: process
 * Function Description: This function parameter consist of different data types.
 **********************************************************************************************/
typedef struct process
{
    struct process *next; /* next process in pipeline        */
    char **cmdstr;        /* for exec()                      */
    char **file;          /* for in/output redirection       */
    int numargs;          /* number of arguments in cmdline  */
    pid_t pid;            /* process ID                      */
    char completed;       /* true if process has completed   */
    char stopped;         /* true if process has stopped     */
    int status;           /* report status value             */
} process;

/*********************************************************************************************
 * Function Name: job
 * Description: This function comprises with different type od data types and primrly signifies the coomnds ordere eneterd by the user.
 *********************************************************************************************/

typedef struct job
{
    struct job *next;          /* next active job */
    char *command;             /* command line, used for messages */
    process *first_process;    /* list of processes in this job */
    pid_t pgid;                /* progress group ID */
    char notified;             /* true if user told about stopped job */
    int stdin, stdout, stderr; /* standard i/o channels */
} job;

/*********************************************************************************************
 * Function Name: macroerr
 * Description: This function symbolizes the integers.
 * Its use case is to make coder easy to understand to the readers.
 *********************************************************************************************/
enum macroerr
{
    MAX_ARGS_EXCEEDED = 10,
    DIR_DNE = 1,
    CMD_NOT_FOUND = 12,
    USR_EXITED = 13,
    PWD_ISSUED = 14,
    MISSING_CMD = 15,
    I_FILE_OPEN_FAILURE = 16,
    O_FILE_OPEN_FAILURE = 17,
    NO_IN_FILE = 18,
    NO_OUT_FILE = 19,
};

/*********************************************************************************************
 * Function name -reperr -
 * Description- This function role is to  print error message case by case regarding the commnads.
 * @pptr: This function parameter is a pointer that references the process structure.
 * @status: This function parameter  represents the status for the error messages.
 *********************************************************************************************/

void reperr(struct process *pptr, char *cmd, int status)
{
    switch (status)
    {
    case USR_EXITED:
        fprintf(stderr, "Bye...\n");
        fprintf(stderr, "+ completed '%s' [%d]\n",
                cmd, EXIT_SUCCESS);
        return;
    case MAX_ARGS_EXCEEDED:
        fprintf(stderr, "Error: too many process arguments\n");
        return;
    case DIR_DNE:
        fprintf(stderr,
                "Error: cannot cd into directory\n");
        fprintf(stderr, "+ completed '%s' [%d]\n",
                cmd, 1);
        return;
    case CMD_NOT_FOUND:
        fprintf(stderr, "Error: command not found\n");
        return;
    case PWD_ISSUED:
        fprintf(stderr, "%s\n", cmd);
        fprintf(stderr, "+ completed '%s' [%d]\n",
                pptr->cmdstr[0], 0);
        return;
    case MISSING_CMD:
        fprintf(stderr, "Error: missing command\n");
        return;
    case I_FILE_OPEN_FAILURE:
        fprintf(stderr, "Error: cannot open input file\n");
        return;
    case O_FILE_OPEN_FAILURE:
        fprintf(stderr, "Error: cannot open output file\n");
        return;
    case NO_OUT_FILE:
        fprintf(stderr, "Error: no output file\n");
        return;
    case NO_IN_FILE:
        fprintf(stderr, "Error: no input file\n");
        return;
    default:
        fprintf(stderr, "+ completed '%s' [%d]\n",
                cmd, WEXITSTATUS(pptr->status));
        return;
    }
}
/*********************************************************************************************
 * Function name: safeopen
 * Description: This function parameter attempts to open a file and returns the file descrptor.
 *********************************************************************************************/
int safeopen(struct process *pptr, char *cmd, int stream)
{
    /* Attempt to open out file and modify the appropriate file descriptor */
    int fd = open(pptr->file[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);

    /* Handle 'cannot open output file' error */
    if (fd < 0)
    {
        if (stream)
        {
            reperr(pptr, cmd, O_FILE_OPEN_FAILURE);
            return O_FILE_OPEN_FAILURE;
        }
    }

    return fd;
}

/*********************************************************************************************
 * Function Name: strparse
 * Description- This function converts a string into an array of arguments, commonly referred to as tokenizing the commands.
 * @pptr: This function parameter is a pointer to the process structure.
 * @delim: This  function parameter is used to separate the arguments.
 *********************************************************************************************/
void strparse(struct process *pptr, char *strcopy1, char *delim)
{
    /* Parse the command line for $PATH and arguments */
    char *strcopy2 = strdup(strcopy1);
    char *token;
    int i;

    token = strtok(strcopy1, delim);
    for (pptr->numargs = 0; token != NULL; ++pptr->numargs)
    {
        token = strtok(NULL, delim);
    }

    token = strtok(strcopy2, delim);
    pptr->cmdstr = (char **)malloc((pptr->numargs + 1) * sizeof(char *));
    for (i = 0; i < pptr->numargs; ++i)
    {
        pptr->cmdstr[i] = strdup(token);
        token = strtok(NULL, delim);
    }

    free(strcopy2);
    pptr->cmdstr[pptr->numargs] = NULL;
}

/*********************************************************************************************
 * Function Name: setjob
 * Description- This function role is to  interprets a the arguments like command string regarding pipe.
 * @first_job: This function parameter is indication to  the pointer that is linked with the job structure in  pipelines intial process.
 * @nummetachar: This indicates the count of pipe meta-characters identified within the command string.
 *********************************************************************************************/
void setjob(struct job *first_job, int nummetachar)
{
    /* Initialize the command string(s) of job processes */
    struct process addresses[nummetachar + 1];
    struct process **processes = (struct process**) malloc((nummetachar + 1) * sizeof(struct process*));
    for (int i = 0; i <= nummetachar; ++i) {
        processes[i] = &addresses[i];
    }
    first_job->first_process = processes[0];

    char *cmdcopy;
    char *token;

    if (nummetachar == 1)
    {   
        first_job->first_process->next = processes[1];
        /* (cmd1) | cmd2 */
        cmdcopy = strdup(first_job->command);
        token = strtok(cmdcopy, "|");
        strparse(first_job->first_process, token, " ");

        cmdcopy = strdup(first_job->command);
        token = strchr(cmdcopy, '|');
        strparse(first_job->first_process->next, token + 1, " ");
        free(cmdcopy);
    }
}

void executepipehead(struct process *process1, struct process *process2)
{
    /* Run by forked processes to connect to pipes */
    int mypipe[2];
            /* Create the pipe */
            if (pipe(mypipe))
            {
                perror("pipe");
                exit(1);
            }

    pid_t pid = fork();

    if (pid > (pid_t) 0)
    {
        /* Close unused file descriptor */
        close(mypipe[0]);
        /* Connect parent's stdout to writing end of pipe */
        dup2(mypipe[1], STDOUT_FILENO);
        /* Close unused file descriptor */
        close(mypipe[1]);
        /* Execute external process */
        execvp(process1->cmdstr[0], process1->cmdstr);
        exit(1);
    }
    else if (pid == (pid_t)0)
    {
        /* Close unused file descriptor */
        close(mypipe[1]);
        /* Connect child's stdin to reading end of pipe */
        dup2(mypipe[0], STDIN_FILENO);
        /* Close unused file descriptors */
        close(mypipe[0]);
        /* Execute external process */
        execvp(process2->cmdstr[0], process2->cmdstr);
        exit(1);
    }
    else
    {
        perror("fork");
        exit(1);
    }
}

/*********************************************************************************************
 * Function Name: metaparse
 * Description: This function anlyzes the  commands  that includes a meta-characters.
 * @pptr: This parameter is the reference  to the Pointer linked with the process structure.
 * @cmd: The command parameter is string that brings the meta-character.
 * @delim-  This parameter  representing the meta-character
 * @ return- This returns 0  when the program  successful executed and an error code when it fails to execute
 *********************************************************************************************/
int metaparse(struct process *pptr, char *cmd, char *delim)
{
    /* Separate command on left and file on right of meta-character */
    char **cmdstr;

    /* Get the first half of command before meta-character */
    char *strcopy = strdup(cmd);
    char *tempstr = strtok(strcopy, delim);
    strparse(pptr, tempstr, " ");
    free(strcopy);         /* Call free --after-- parsing to avoid undef behavior */
    cmdstr = pptr->cmdstr; /* Temporarily store resulting string */

    /* Get the second half of command after '>' meta-character for the shell */
    if (!strcmp(delim, ">")) {
        strcopy = strdup(cmd);
        tempstr = strchr(strcopy, '>');
    } else if (!strcmp(delim, "<")) {
        strcopy = strdup(cmd);
        tempstr = strchr(strcopy, '<');
    }

    /* Handle 'no output file' error */
    if (strlen(tempstr) == (size_t)1)
    {
        reperr(pptr, cmd, NO_OUT_FILE);
        return NO_OUT_FILE;
    }

    strparse(pptr, tempstr + 1, " ");
    free(strcopy);
    pptr->file = pptr->cmdstr;
    pptr->cmdstr = cmdstr; /* Restore string of args for exec() */

    return 0;
}
/*********************************************************************************************
 * Function Name-  Main
 * Description- The fundamental purpose of the basic shell program is to read user commands, interpret them, and execute them.
 ***************************************************************************************************************************************************/
int main(void)
{
    char cmd[CMDLINE_MAX];
    char *eof;

    while (1)
    {
        char *nl;
        char *strcopy;
        int nummetachar = 0;
        char *metachar = "";
        char metacharfound = 0;
        struct process p;
        struct process *pptr = &p;
        struct job j;
        struct job *jptr = &j;
        int fd;

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        eof = fgets(cmd, CMDLINE_MAX, stdin);
        if (!eof)
            /* Make EOF equate to exit */
            strncpy(cmd, "exit\n", CMDLINE_MAX);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO))
        {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';

        /* Avoid crashing when user hits 'enter' */
        if (cmd[0] == '\0')
            continue;

        /* Create copy to preserve original string */
        strcopy = strdup(cmd);

        /* Begin parsing for arguments */
        strparse(pptr, strcopy, " ");
        free(strcopy);

        /* Handle parsing error */
        if (pptr->numargs > 16)
        {
            reperr(pptr, cmd, MAX_ARGS_EXCEEDED);
            continue;
        }

        if (!strcmp(pptr->cmdstr[0], ">") || pptr->cmdstr[0][0] == '>')
        {
            reperr(pptr, cmd, MISSING_CMD);
            continue;
        }
        else if (!strcmp(pptr->cmdstr[0], "|") || pptr->cmdstr[0][0] == '|')
        {
            reperr(pptr, cmd, MISSING_CMD);
            continue;
        }

        /* Builtin command */
        if (!strcmp(pptr->cmdstr[0], "exit"))
        {
            reperr(pptr, cmd, USR_EXITED);
            break;
        }

        /* Builtin command */
        if (!strcmp(pptr->cmdstr[0], "pwd"))
        {
            long size = pathconf(".", _PC_PATH_MAX); /* source citation: https://pubs.opengroup.org/onlinepubs/9699969599/functions/getcwd.html */
            char *buffer;
            if ((buffer = (char *)malloc((size_t)size)))
            {
                /* Get current working directory */
                getcwd(buffer, (size_t)size);
                reperr(pptr, buffer, PWD_ISSUED);
                free(buffer);
            }
            else
            {
                perror("getcwd");
                free(buffer);
                exit(1);
            }
            continue;
        }

        /* Builtin command */
        if (!strcmp(pptr->cmdstr[0], "cd"))
        {
            pptr->status = chdir(pptr->cmdstr[1]);
            if (pptr->status == 0)
            {
                reperr(pptr, cmd, EXIT_SUCCESS);
                continue;
            }
            else
            {
                reperr(pptr, cmd, DIR_DNE);
                continue;
            }
        }

        /* Search for meta-character */
        for (int i = 0; i < pptr->numargs; ++i)
        {
            for (size_t j = 0; j < strlen(pptr->cmdstr[i]); ++j)
            {
                if (pptr->cmdstr[i][j] == '>')
                {
                    metacharfound = 1;
                    metachar = ">";
                    break;
                }
                else if (pptr->cmdstr[i][j] == '<')
                {
                    metacharfound = 1;
                    metachar = "<";
                    break;
                }
                else if (pptr->cmdstr[i][j] == '|')
                {
                    metacharfound = 1;
                    metachar = "|";
                    /* Count up to three pipes '|' */
                    ++nummetachar;
                }
            }
        }

        /* Separate command string at meta-character */
        if (metacharfound)
        {
            /* Output redirection */
            if (!strcmp(metachar, ">"))
            {
                if (metaparse(pptr, cmd, metachar) == NO_OUT_FILE)
                {
                    continue;
                }
                if ((fd = safeopen(pptr, cmd, STDOUT_FILENO)) == O_FILE_OPEN_FAILURE)
                {
                    continue;
                }
            } else if (!strcmp(metachar, "<")) {
                if (metaparse(pptr, cmd, metachar) == NO_IN_FILE) {
                    continue;
                }
                if ((fd = safeopen(pptr, cmd, STDIN_FILENO)) == I_FILE_OPEN_FAILURE) {
                    continue;
                }
            }
        }

        /* Regular command or pipeline */
        if (metacharfound && !strcmp(metachar, "|"))
        {
            /* Initialize processes */
            jptr->command = cmd;
            // jptr->first_process = pptr;
            setjob(jptr, nummetachar);
            /* Fork the shell to avoid losing it */
            jptr->first_process->pid = fork();

            if (jptr->first_process->pid == (pid_t) 0)
            { /* Child */
                /* Single pipe case */
                if (nummetachar == 1)
                {
                    executepipehead(jptr->first_process, jptr->first_process->next);
                    /* Two or more pipes case */
                }
                else if (nummetachar > 1)
                {
                    for (int i = 0; i <= nummetachar; ++i)
                    {
                        //jptr->first_process->next->pid = fork();
                        if (jptr->first_process->next->pid > (pid_t) 0)
                        {
                            if (!i)
                            { /* Set up the head of the pipeline */
                    
                            }
                            else if (i == nummetachar)
                            { /* Set up the tail of the pipeline */
                                
                            }
                            else
                            { /* Set up the interior of the pipeline */
                            }
                        }
                        else if (jptr->first_process->next->pid == (pid_t) 0)
                        {
                            
                        }
                    }
                }
            }
            else if (jptr->first_process->pid > (pid_t) 0)
            {
                /* Parent for all child processes to terminate */
                for (int i = 0; i <= nummetachar; ++i)
                {
                    if (!i)
                    {
                        waitpid(jptr->first_process[i].pid, &jptr->first_process[i].status, 0);
                        fprintf(stderr, "+ completed '%s' [%d]",
                                cmd, WEXITSTATUS(jptr->first_process[i].status));
                    }
                    else
                    {
                        waitpid(jptr->first_process->next[i].pid, &jptr->first_process->next[i].status, 0);
                        fprintf(stderr, "[%d]",
                                WEXITSTATUS(jptr->first_process->next[i].status));
                    }
                }
                printf("\n");
                fflush(stdout);
            }
            else
            {
                perror("fork");
                exit(1);
            }

        }
        else
        {
            /* Regular command or redirection */
            pptr->pid = fork();

            if (pptr->pid == (pid_t) 0)
            {
                /* Child process attempts to execute program */
                if (metacharfound && !strcmp(metachar, ">"))
                {
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                } else if (metacharfound && !strcmp(metachar, "<")) {
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                execvp(pptr->cmdstr[0], pptr->cmdstr);
                reperr(pptr, cmd, CMD_NOT_FOUND);
                exit(1);
            }
            else if (pptr->pid > (pid_t) 0)
            {
                /* Parent waits for its child to terminate */
                waitpid(pptr->pid, &pptr->status, 0);
                reperr(pptr, cmd, pptr->status);
            }
            else
            {
                perror("fork");
                exit(1);
            }
        }
    }

    return EXIT_SUCCESS;
}