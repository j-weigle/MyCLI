/************************************************
 *                 internal.c                   *
 ************************************************
 * Handles internal commands for SUSH           *
 ************************************************
 * Author: Justin Weigle                        *
 * Edited: 30 Jul 2020                          *
 ************************************************/

#include "../includes/internal.h"
#include "../includes/mycli.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static bool env_var_delete (ListHandler);
static bool env_var_set (ListHandler);
static bool change_directory (ListHandler);
static bool print_wdirectory ();

/**
 * Runs a given internal command as long as it's
 * valid
 */
int run_internal_cmd (ListHandler tlist) {
    bool found_internal_cmd = false;
    bool err_found = false;
    char *token = ((tok_node *)tlist.head)->token;
    if (!strcmp(token, "setenv")) {
        /* set an environment variable */
        err_found = env_var_set(tlist);
        found_internal_cmd = true;
    } else if (!strcmp(token, "unsetenv")) {
        /* delete an environment variable */
        err_found = env_var_delete(tlist);
        found_internal_cmd = true;
    } else if (!strcmp(token, "cd")) {
        /* change directory */
        err_found = change_directory(tlist);
        found_internal_cmd = true;
    } else if (!strcmp(token, "pwd")) {
        /* print the current working directory */
        print_wdirectory();
        found_internal_cmd = true;
    } else if (!strcmp(token, "exit")) {
        /* print accounting info and exit */
        free_tok_list(&tlist);
        found_internal_cmd = true;
        exit(0);
    }
    if (found_internal_cmd) {
        if (err_found) {
            return -1; // found, but error
        } else {
            return 0; // found, no error
        }
    } else {
        return 1; // not found
    }
}

/**
 * Add a new environment variable or modify an existing one
 */
static bool env_var_set (ListHandler tlist)
{
    char *env_var_name = ((tok_node *)tlist.head)->next->token;
    char *env_var_val = ((tok_node *)tlist.tail)->token;
    if (tlist.count == 3) {
        if(setenv(env_var_name, env_var_val, 1)) {
            perror("couldn't set env var");
            return true; // error
        }
    } else {
        fprintf(stderr, "setenv takes 2 arguments");
        return true; // error
    }
    return false; // no error
}

/**
 * Delete an environment variable
 */
static bool env_var_delete (ListHandler tlist)
{
    if (tlist.count == 2) {
        if(unsetenv(((tok_node *)tlist.tail)->token)) {
            perror("couldn't delete");
            return true; // error
        }
    } else {
        fprintf(stderr, "unsetenv takes 1 argument");
        return true; // error
    }
    return false; // no error
}

/**
 * change to a give directory.
 * ~ == HOME
 */
static bool change_directory (ListHandler tlist)
{
    if (tlist.count == 2) {
        if ((((tok_node *)tlist.head)->next->token)[0] == '~') {
            const char *home = getenv("HOME");
            char tmpstr[strlen(home)+1];
            strcpy(tmpstr, home);
            const char *fpath = strcat(tmpstr, &(((tok_node *)tlist.tail)->token)[1]);
            chdir(fpath);
            return false; // no error
        }
        chdir(((tok_node *)tlist.tail)->token);
    } else {
        fprintf(stderr, "cd takes 1 argument\n");
        return true; // error
    }
    return false; // no error
}

/**
 * print the current working directory to STDOUT
 */
static bool print_wdirectory ()
{
    char buff[BUFF_SIZE];
    if (getcwd(buff, BUFF_SIZE) == NULL) {
        perror("couldn't get current working directory");
        return true; // error
    }
    printf("%s\n", buff);
    return false; // no error
}
