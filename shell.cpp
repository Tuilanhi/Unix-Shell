#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <vector>
#include <string>
#include <ctime>

#include "Tokenizer.h"

#define MAX_PATH 256
char CURRENT_PATH[MAX_PATH];

// all the basic colours for a shell prompt
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define WHITE "\033[1;37m"
#define NC "\033[0m"

using namespace std;
vector<pid_t> bgs_pid;

void create_processes(int in, int out, Command *cm)
{
    // create processes are requested by user for each command
    // fork the child
    pid_t pid = fork();
    if (pid < 0)
    { // error check
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        // check if the current command hasInput()
        if (cm->hasInput())
        {
            // open the file for reading as a file descriptor
            // use dup2 to redirect stdin (0) to the file descriptor opened for reading
            int fd = open(cm->in_file.c_str(), O_RDONLY);
            dup2(fd, 0);
            close(fd);
        }
        // check if in parameter is STDIN, if not dup2 the child's STDIN to the in parameter
        // check for input file
        else if (in != 0 && !cm->hasInput())
        {
            dup2(in, 0);
        }

        // check if the current command hasOutput()
        if (cm->hasOutput())
        {
            int fd = open(cm->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            dup2(fd, 1);
            close(fd);
        }
        // check for output file
        // check if out parameter is STDOUT, if not dup2 the child's STDOUT to the out parameter
        else if (out != 1 && !cm->hasOutput())
        {
            dup2(out, 1);
        }

        char **args = new char *[cm->args.size() + 1];
        for (unsigned int i = 0; i < cm->args.size(); i++)
        {
            args[i] = const_cast<char *>(cm->args[i].c_str());
        }
        args[cm->args.size()] = nullptr;

        // In child, execute the command
        if (execvp(args[0], args) < 0)
        { // error check
            perror("execvp");
            exit(2);
        }
        execvp(args[0], args);
    }
    waitpid(pid, nullptr, 0);
}

void check_for_background()
{
    // implementation iteration over vector of bg pid (vector outside loop, in the global variable)
    // waitpid() - using flag for non-blocking
    // before prompting user, iterate over vector to reap processes
    int status = 0;
    for (unsigned int i = 0; i < bgs_pid.size(); i++)
    {
        // make waitpid non blocking for background processes
        status = waitpid(bgs_pid[i], &status, WNOHANG);
        if (WIFSIGNALED(status) == true)
        {
            cout << "This pid has been killed: "
                 << "[" << bgs_pid[i] << "]"
                 << "\n";
            // remove process from the list, leaving an empty list
            bgs_pid.erase(bgs_pid.begin() + i);
            // keeping i at the same spot, at the front
            i--;
        }
    }
}

int main()
{
    // Save original stdin stdout
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);

    // prompting for cd
    string curr_dir = getcwd(CURRENT_PATH, MAX_PATH);
    string prev_dir = getcwd(CURRENT_PATH, MAX_PATH);

    for (;;)
    {
        check_for_background();

        // implementation date/time with TODO
        // implement username with getlogin()
        // implement curdir with getcwd()
        // need date/time, username, and absolute path to current dir
        // iterate for the vector and checks
        time_t t = time(0);
        char *current_time = ctime(&t);
        cout << YELLOW << current_time << getenv("USER") << ':' << getcwd(CURRENT_PATH, MAX_PATH) << NC << "$";

        string input;
        getline(cin, input);

        if (input.empty())
        {
            continue;
        }

        if (input == "exit")
        { // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl
                 << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer token(input);
        if (token.hasError())
        { // continue to next prompt if input had an error
            continue;
        }

        if (token.commands.size() == 1)
        {
            Command *cm = token.commands.back();
            char **args = new char *[cm->args.size() + 1];
            for (unsigned int j = 0; j < cm->args.size(); j++)
            {
                args[j] = const_cast<char *>(cm->args[j].c_str());
            }
            args[cm->args.size()] = nullptr;

            // chdir() (change the process' working directory to PATH)
            // if dir (cd <dir>) is "-", then go to the previous directory
            // variable storing previous working directory (it needs to be declared outside loop)
            // use getcwd (get current working directory)

            if (strcmp(args[0], "cd") == 0)
            {
                curr_dir = CURRENT_PATH;
                if (strcmp(args[1], "-") == 0)
                {
                    // we move back to the prev directory
                    chdir(prev_dir.c_str());
                    // prev directory becomes the directory we move from
                    prev_dir = curr_dir;
                }
                else
                {
                    // move to command at args[1]
                    chdir(args[1]);
                    // store the prev_dir as the directory we move from
                    prev_dir = curr_dir;
                }
                delete[] args;
                continue;
            }

            // Create child to run first command
            pid_t pid = fork();
            if (pid < 0)
            { // error check
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (pid == 0)
            {
                // check if the current command hasInput()
                if (cm->hasInput())
                {
                    // open the file for reading as a file descriptor
                    // use dup2 to redirect stdin (0) to the file descriptor opened for reading
                    int fd = open(cm->in_file.c_str(), O_RDONLY);
                    dup2(fd, 0);
                    close(fd);
                }

                // check if the current command hasOutput()
                if (cm->hasOutput())
                {
                    int fd = open(cm->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    dup2(fd, 1);
                    close(fd);
                }

                // In child, execute the command
                if (execvp(args[0], args) < 0)
                { // error check
                    perror("execvp");
                    exit(2);
                }
                execvp(args[0], args);
            }
            else
            {
                // checks for background process, if it is True, we push it into the vector<pid_t> bgs_pid
                // store pid from fork in vector of background processes
                if (cm->isBackground())
                {
                    bgs_pid.push_back(pid);
                }
                // if there is no background process, we perform waitpid as usual
                else
                {
                    waitpid(pid, nullptr, 0);
                }
            }
            // deallocate memmory
            delete[] args;
        }
        else if (token.commands.size() > 1)
        {
            int in, out;

            int pipefd[2];

            // first process gets input from STDIN
            in = STDIN_FILENO;

            for (unsigned int i = 0; i < token.commands.size(); i++)
            {
                // create a pipe
                pipe(pipefd);

                // this is the last process, out equals parent's STDOUT
                if (i == token.commands.size() - 1 && !token.commands.back()->hasOutput())
                {
                    out = STDOUT_FILENO;
                }
                else
                {
                    // except for the last process, all the process out equals input end of pipe or contains I/O output direction
                    out = pipefd[1];
                }

                // create processes as more pipes are needed
                // in = STDIN, out = write end of the pipe
                // perform fork(), execvp() in the process just like process command
                create_processes(in, out, token.commands[i]);

                // close the unused write end of the pipe
                if (out != 1)
                {
                    close(out);
                }

                // as then next process, reads from the read side of the pipe
                in = pipefd[0];
            }
        }
    }
    // Reset the input and output file descriptors of the parent.
    // Overwrite in/out with what was saved.
    dup2(saved_in, STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);

    return 0;
}
