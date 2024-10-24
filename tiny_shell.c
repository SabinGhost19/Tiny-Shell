#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>

#define BUFFER_SIZE 100

// Functie pentru extragerea primei comenzi
char *extract_first_cmd(char *buffer)
{
    char *cmd = (char *)malloc(sizeof(char) * 100);
    int i = 0;
    while (buffer[i] != '\n' && buffer[i] != ' ' && buffer[i] != '\0')
    {
        cmd[i] = buffer[i];
        i++;
    }
    cmd[i] = '\0';
    return cmd;
}

// Functie pentru citirea inputului din linia de comanda
char *read_line()
{
    char *buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    printf("> ");
    ssize_t read_bytes = read(STDIN_FILENO, buffer, BUFFER_SIZE);

    if (read_bytes < 0)
    {
        perror("error reading the input");
        exit(-1);
    }

    // Eliminam newline-ul '\n' de la final
    if (read_bytes > 0 && buffer[read_bytes - 1] == '\n')
    {
        buffer[read_bytes - 1] = '\0';
    }
    else
    {
        buffer[read_bytes] = '\0';
    }

    return buffer;
}
char *parse_command(char *line, char **args)
{

    char *arg_cmd = strtok(line, " ");
    // in lista de argumente primul argument
    // trebuie sa fie numele comenzii

    int i = 0;
    while (arg_cmd != NULL && i < 9)
    {
        args[i] = arg_cmd;
        i++;
        arg_cmd = strtok(NULL, " ");
    }
    args[i] = NULL; // final lists of arguments
    return args[0];
}
void replace_env_variables_in_args(char *args[])
{
    int i = 0;
    while (args[i] != NULL)
    {

        if (args[i][0] == '$')
        {
            char *actual_env_variable = getenv(args[i] + 1);
            if (actual_env_variable)
            {
                args[i] = actual_env_variable;
            }
        }
        i++;
    }
}
void normal_execution(char *command, char *args[])
{
    int status;
    pid_t pid = fork();
    switch (pid)
    {
    case 0:
        // child
        replace_env_variables_in_args(args);

        if (execvp(command, args) == -1)
        {
            perror("execvp failed");
        }
        exit(EXIT_FAILURE);
        break;
    case -1:
        perror("Eroare la fork");
        break;
    default:
        // parent
        waitpid(pid, &status, 0);

        if (WIFEXITED(status))
        {
            printf("Process exited with status: %d\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("Process terminated by signal: %d\n", WTERMSIG(status));
        }
        break;
    }
}
void set_environment_variable(char *line)
{
    char *variable = strtok(line, "=");
    char *value = strtok(NULL, "=");
    if (variable != NULL && value != NULL)
    {
        if (setenv(variable, value, 1) < 0)
        {
            perror("set env var failed");
            exit(EXIT_FAILURE);
        }
    }
}

void procs_communicate(char *comm, char *args[], char *comm2, char *args2[])
{

    int pipe_pid_fd[2];
    if (pipe(pipe_pid_fd) < 0)
    {
        perror("error on creating pipe: ");
        exit(-1);
    }

    pid_t pid_from_fork_1 = fork();
    switch (pid_from_fork_1)
    {
    case 0:
        // child
        close(pipe_pid_fd[0]);
        dup2(pipe_pid_fd[1], STDOUT_FILENO);
        // stdout o sa devina intrarea in pipe

        close(pipe_pid_fd[1]);

        replace_env_variables_in_args(args);

        if (execvp(comm, args) < 0)
        {
            perror("error laungh pipe cmmd");
            exit(EXIT_FAILURE);
        }
        break;
    case -1:
        perror("Error forking first process");
        exit(EXIT_FAILURE);
    default:
        // parent
        // nothing
        // lasam ca procesele sa se execute in paralel
        // pentru un flux mai bun
        break;
    }

    pid_t pid_from_fork_2 = fork();
    switch (pid_from_fork_2)
    {
    case 0:
        // child
        close(pipe_pid_fd[1]);
        dup2(pipe_pid_fd[0], STDIN_FILENO);
        // stdin o sa devina iesirea din pipe
        // practic stdin o sa pointeze spre fd de la pipe_pid_fd[0]--iesirea
        close(pipe_pid_fd[0]);
        // nu mai e nevoie de acesta asa ca in inchidem
        // avem deja fd pentru iesire pipe in stdin_fileno

        replace_env_variables_in_args(args);

        if (execvp(comm2, args2) < 0)
        {
            perror("error laungh pipe cmmd");
        }
        exit(EXIT_FAILURE);
        break;
    case -1:
        perror("Error forking second process");
        exit(EXIT_FAILURE);
    default:
        // parent
        // nothing
        // lasam ca procesele sa se execute in paralel
        // pentru un flux mai bun
        break;
    }
    // inchidem ambele capete pentru ca nu avem nevoie de ele
    close(pipe_pid_fd[0]);
    close(pipe_pid_fd[1]);

    // asteptam ca ambele procese sa se termine
    int status1, status2;
    waitpid(pid_from_fork_1, &status1, 0);
    waitpid(pid_from_fork_2, &status2, 0);

    // just for debugging
    if (WIFEXITED(status1))
    {
        printf(" 1 Process exited with status: %d\n", WEXITSTATUS(status1));
    }
    else if (WIFSIGNALED(status1))
    {
        printf("1 Process terminated by signal: %d\n", WTERMSIG(status1));
    }
    if (WIFEXITED(status2))
    {
        printf(" 2 Process exited with status: %d\n", WEXITSTATUS(status2));
    }
    else if (WIFSIGNALED(status2))
    {
        printf(" 2 Process terminated by signal: %d\n", WTERMSIG(status2));
    }
}
char *trim(char *str)
{
    // Eliminam spatiile de la inceput
    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0) // Verificam daca a ramas vreun caracter
        return str;

    // Eliminam spatiile de la sfarsit
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Terminam sirul de caractere
    *(end + 1) = '\0';

    return str;
}
void parse_pipes_commands(char *line)
{
    char *commands__[10];

    char *command = strtok(line, "|");

    int i = 0;
    while (command != NULL)
    {
        commands__[i] = trim(command);
        command = strtok(NULL, "|");
        i++;
    }
    commands__[i] = NULL;

    char *args[10];
    char *args2[10];

    for (int k = 0; k < i - 1; k++)
    {
        char *comm = parse_command(commands__[k], args);
        char *comm2 = parse_command(commands__[k + 1], args2);
        procs_communicate(comm, args, comm2, args2);
    }
}
void redirect_output(char *line)
{
    char *args[10];
    char *first_command = trim(strtok(line, ">"));
    char *file_to_write_in = trim(strtok(NULL, ">"));

    char *commnd = parse_command(first_command, args);

    pid_t file_pid = open(file_to_write_in, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (file_pid < 0)
    {
        perror("Error in opening/creating the file");
        exit(EXIT_FAILURE);
    }

    pid_t fork_pid = fork();
    switch (fork_pid)
    {
    case 0:
        // child
        replace_env_variables_in_args(args);

        dup2(file_pid, STDOUT_FILENO);

        if (execvp(commnd, args) == -1)
        {
            perror("error laungh pipe cmmd");
            exit(EXIT_FAILURE);
        }
        break;

    case -1:
        perror("Error forking second process");
        exit(EXIT_FAILURE);
    default:
        // parent
        wait(NULL);
        if (close(file_pid) < 0)
        {
            perror("closing the file failed...");
            exit(EXIT_FAILURE);
        }
        break;
    }
}
int main(int argc, char *argv[], char *envp[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    char *first_command;
    char *args[10]; // Argumentele comenzii
    // maxim 10 argumente

    while (1)
    {

        // citire linie
        char *line = read_line();

        if (strchr(line, '|') != NULL)
        {
            parse_pipes_commands(line);
            continue;
        }

        if (strchr(line, '=') != NULL)
        {
            set_environment_variable(line); // setam variabila de mediu
            // o adaugam in varibilele de mediu iar dupa o extragem
            free(line);
            continue;
        }

        if (strchr(line, '>') != NULL)
        {
            redirect_output(line);
            free(line);
            continue;
        }

        // extragere first comand
        first_command = parse_command(line, args);

        if (strcmp(first_command, "exit") == 0)
        {
            free(line);
            break;
        }

        // exec
        normal_execution(first_command, args);

        free(line);
    }

    return 0;
}
