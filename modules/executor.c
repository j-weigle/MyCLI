/************************************************
 *                 executor.c                   *
 ************************************************
 * executor takes a linked list and runs the    *
 * strings stored in it as (a) command(s)       *
 ************************************************
 * Author: Justin Weigle                        *
 * Edited: 30 Jul 2020                          *
 ************************************************/

#include "../includes/executor.h"
#include "../includes/mycli.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>

typedef struct path_node {
    char *path;
    struct path_node *next;
} path_node;

enum Read_Write {
    READ,
    WRITE
};

static void parse_cmd (ListHandler);
static ListHandler get_next_subsection (tok_node *);
static bool bin_exists (char *);
static int count_pipes (tok_node *);
static int get_fd (char *, enum Read_Write, bool);
static void output_to_file (char *, bool);
static void file_to_input (char *);
static path_node* get_path();
static void save_path(char *, path_node **);
static void free_path (path_node *);

/**
 * Counts how many pipes there are in the given linked list of tokens and
 * forks() a process for each command and pipes between them as necessary
 */
void execute (tok_node *head)
{
    /* find number of pipes in linked list */
    int pipe_ct = count_pipes(head);
    int cmd_ct = pipe_ct + 1;

    /* allocate storage for the cmd(s) */
    ListHandler cmds[cmd_ct];

    /* split input into separate cmds if pipe(s) */
    tok_node *curr = head;
    for (int i = 0; i < cmd_ct; i++) {
        cmds[i] = get_next_subsection(curr);
        curr = cmds[i].tail;
        if(curr->next != NULL) { // move to next non pipe token
            curr = curr->next->next;
        }
    }

    /* create variables for pipes */
    int pipefd[pipe_ct][2];

    /* create the pipes */
    for (int i = 0; i < pipe_ct; i++) {
        if (pipe(pipefd[i]) < 0) {
            perror("pipe failed in execute");
        }
    }

    /* for forks */
    int status;
    pid_t pid;

    /* fork for every cmd in input */
    for (int i = 0; i < cmd_ct; i++) {
        fflush(NULL); // flush all open output streams(especially pipes)
        pid = fork();
        if (pid < 0) {
            perror("fork failed in execute");
            exit(-1);
        } else if (pid == 0) { // child
            signal(SIGINT, SIG_DFL);
            if (i > 0) { // if not the first cmd
                /* connect read end of prev proc pipe to STDIN of curr proc */
                if (dup2(pipefd[i-1][0], STDIN_FILENO) < 0) {
                    perror("dup2 failed in execute");
                }
                close(pipefd[i-1][0]); // close read end prev proc pipe
                close(pipefd[i-1][1]); // close write end prev proc pipe
            }
            if (i+1 < cmd_ct) { // if there is a next command
                close(pipefd[i][0]); // close read end of curr proc pipe
                /* connect write end of curr proc pipe to STDOUT */
                if (dup2(pipefd[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2 failed in execute");
                }
                close(pipefd[i][1]); // close write end curr proc pipe
            }
            parse_cmd(cmds[i]); // parse and exec curr command
            perror("exec failed"); // if parse_cmd returns, error
            exit(-1);
        } else { // parent
            if (i > 0) { // make sure prev proc pipes are closed
                close(pipefd[i-1][0]); // close read end prev proc pipe
                close(pipefd[i-1][1]); // close write end prev proc pipe
            }
            /* wait for children to terminate */
            while(wait3(&status, 0, NULL) <= 0) {}
        }
    }

    return;
}

/**
 * Takes a single command and parses it to find any redirects, then
 * executes the command
 */
static void parse_cmd (ListHandler cmd_list)
{
    /* allocate strings for each token plus room for a NULL */
    char *cmd[cmd_list.count +1];

    tok_node *curr = cmd_list.head;
    int i = 0;
    /* build command. stop at first redirect or end */
    while ((curr != NULL)  && !(curr->special)) {
        cmd[i++] = curr->token;
        curr = curr->next;
    }
    cmd[i] = NULL; // end of cmd must be NULL for exec

    curr = cmd_list.head;
    /* stop when curr is whatever is right after the tail of the command */
    while (curr != ((tok_node *)cmd_list.tail)->next) {
        if (curr->special) {
            if (!strcmp(curr->token, ">")) {
                /* next token should be output file, append false */
                output_to_file(curr->next->token, false);
            }
            if (!strcmp(curr->token, ">>")) {
                /* next token should be output file, append true */
                output_to_file(curr->next->token, true);
            }
            if (!strcmp(curr->token, "<")) {
                /* next token should be input to current cmd */
                file_to_input(curr->next->token);
            }
        }
        curr = curr->next;
    }

    /* run commands locally if they start with ./ or / */
    if (cmd[0][0] == '/') {
        cmd[0] = cmd[0] + 1;
        execv(cmd[0], cmd);
    } else if (cmd[0][0] == '.') {
        if (cmd[0][1] == '/') {
            execv(cmd[0], cmd);
        }
    /* if the cmd is a bin in the path, execute */
    } else if (bin_exists(cmd[0])) {
        execvp(cmd[0], cmd);
    } else {
        fprintf(stderr, "command %s not found or does not exist\n", cmd[0]);
        exit(127);
    }
    perror("could not exec in parse_cmd");
    exit(-1);
}

/**
 * Gets the next cmd subsection of the linked list based on whether
 * it reaches the end or it finds a |
 */
static ListHandler get_next_subsection (tok_node *curr)
{
    ListHandler cmd;
    cmd.head = curr; // head assumed to be first node passed in

    int count = 0;
    tok_node *prev = NULL;
    while (curr != NULL) {
        if (curr->special) {
            if (!strcmp(curr->token, "|")) { // if pipe found
                cmd.tail = prev; // then prev token is the tail
                break;
            }
        }
        prev = curr;
        curr = curr->next;
        if (curr == NULL) { // if end was found
            cmd.tail = prev; // the prev token is the tail
        }
        count++;
    }
    cmd.count = count;

    return cmd;
}

/**
 * checks the path environment variable to see if bin is in it
 */
static bool bin_exists (char *bin)
{
    /* get path in form of linked list */
    path_node *phead = get_path();

    bool found = false;
    DIR *dir;
    struct dirent *entry;
    path_node *list = phead;

    /* search path to see if the given string bin is in the directories */
    while (list != NULL && found == false) {
        if ((dir = opendir(list->path)) != NULL) {
            while ((entry = readdir(dir)) != NULL && found == false) {
                if (!strcmp(entry->d_name, bin)) { // if true, the bin exists
                    found = true;
                }
            }
        }
        list = list->next;
    }
    closedir(dir);
    free_path(phead);
    return found;
}

/**
 * count total pipes in a given tok_node linked list
 */
static int count_pipes (tok_node *head)
{
    int pipe_ct = 0;
    while (head != NULL) {
        if (head->special) {
            if (!strcmp(head->token, "|")) {
                pipe_ct++;
            }
        }
        head = head->next;
    }
    return pipe_ct;
}

/**
 * gets a file descriptor for the given file name f
 */
static int get_fd (char *f, enum Read_Write rw, bool append)
{
    int fd;
    if (rw == READ) {
        /* open f READ only */
        fd = open(f, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
    }
    if (rw == WRITE) {
        if (append) {
            /* open f WRITE only with append turned on */
            fd = open(f, O_WRONLY | O_CREAT | O_APPEND, S_IWUSR | S_IRUSR);
        } else {
            /* open f WRITE only without append turned on */
            fd = open(f, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
        }
    }
    if (fd < 0) {
        perror("open failed in get_fd\n");
        exit(-1);
    }

    return fd;
}

/**
 * redirect stdout to the file listed after >
 * append controls whether the file is appended or overwritten
 */
static void output_to_file (char *fname, bool append)
{
    int fd;
    fd = get_fd(fname, WRITE, append);
    dup2(fd, STDOUT_FILENO); // stdout > file
    close(fd); // done, connection made with dup2
}

/**
 * redirect a file to stdin
 */
static void file_to_input (char *fname)
{
    int fd;
    fd = get_fd(fname, READ, false);
    dup2(fd, STDIN_FILENO); // stdin < file
    close(fd); // done, connection made with dup2
}

/**
 * gets the user's path environment variable and returns it
 * as a linked list
 */
static path_node* get_path()
{
    char *fpath = getenv("PATH"); // get path
    int length = strlen(fpath);
    char path[length]; // make buffer to store individual path strings
    path_node *phead = NULL;

    /* go through full path(fpath) searching for colons, which are the
     * delimiters of the path environment variable */
    for (int i = 0, j = 0; i < length; i++) {
        if (fpath[i] == ':') {
            path[j] = '\0';
            save_path(path, &phead);
            j = 0;
        } else {
            path[j++] = fpath[i];
        }
    }

    return phead;
}

/**
 * saves a path into a path_node linked list
 */
static void save_path(char *path, path_node **phead)
{
    path_node *list = *phead;
    path_node *p_node = malloc(sizeof(path_node)); // make new node
    if (p_node == NULL) {
        perror("malloc failed in save_path()");
    }
    int length = strlen(path);
    p_node->path = malloc(sizeof(char)*length+1); // make space for path
    if (p_node->path == NULL) {
        perror("malloc failed in save_path()");
    }
    strncpy(p_node->path, path, length+1); // put path in node
    p_node->path[length] = '\0'; // in case strncpy doesn't null terminate
    p_node->next = NULL; // set next to NULL, node is going at the end

    if (*phead == NULL) { // if phead is NULL, new node is phead
        *phead = p_node;
        return;
    } else { // otherwise insert node at the end
        while (list->next != NULL) {
            list = list->next;
        }
        list->next = p_node;
    }
    return;
}

/**
 * takes a path linked list and frees it
 */
static void free_path (path_node *phead)
{
    path_node *list = phead;
    while (list != NULL) {
        phead = phead->next;
        free(list->path);
        free(list);
        list = phead;
    }
}
