/************************************************
 *                 rcreader.c                   *
 ************************************************
 * rcreader opens a .myclirc file in the user's *
 * home directory as long as it is executable,  *
 * and then it calls tokenizer to split it into *
 * tokens to be passed to the executor          *
 ************************************************
 * Author: Justin Weigle                        *
 * Edited: 30 Jul 2020                          *
 ************************************************/

#include "../includes/rcreader.h"
#include "../includes/mycli.h"
#include "../includes/executor.h"
#include "../includes/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/resource.h>

/**
 * Opens the user's home directory using the $HOME environment
 *  variable and tries to find a .mycli file that is executable
 *  to parse it into shell commands
 */
void read_myclirc ()
{
    ListHandler tlist;

    /* set path to home and .myclirc */
    const char *home = getenv("HOME");
    // strcat cuts off \0 bit from *dest, need a temp
    int pathlen = strlen(home);
    char tmpstr[pathlen+1];
    strcpy(tmpstr, home);
    // store full path to .myclirc
    const char *rcfile = strcat(tmpstr, "/.myclirc");

    /* Search for .myclirc in $HOME */
    DIR *dir;
    struct dirent *entry;
    int found = 0;
    if ((dir = opendir(home)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (!strcmp(entry->d_name, ".myclirc")) {
                found = 1;
                // open if executable
                if (access(rcfile, X_OK) == 0) {
                    char buf[BUFF_SIZE];
                    FILE *fp = fopen(rcfile, "r");
                    // read file until EOF is found (fgets() returns NULL)
                    while ((fgets(buf, BUFF_SIZE, fp)) != NULL) {
                        tokenize(&tlist, buf);
                        if (tlist.head) {
                            int ret = run_internal_cmd(tlist);
                            if (ret < 0) {
                                fprintf(stderr, "unable to run internal command\n");
                            } else if (ret == 0) {
                            } else {
                                execute(tlist.head);
                            }
                        }
                        free_tok_list(&tlist);
                    }
                    fclose(fp); // close the file
                } else {
                    perror("In read_myclirc() - .myclirc is not executable ");
                }
            }
        }
        if (!found) {
            fprintf(stderr, "No .myclirc found...\n");
        }
        closedir(dir);
    } else {
        perror("In read_myclirc() - Could not open $HOME ");
    }
}
