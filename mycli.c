/************************************************
 *                   mycli.c                    *
 ************************************************
 * Linux shell command interpreter.             *
 * Reads a .myclirc file from the user's home   *
 * directory and execs it line by line if it is *
 * executable, then waits for user input to     *
 * tokenize and run commands                    *
 ************************************************
 * Author: Justin Weigle                        *
 * Edited: 30 Jul 2020                          *
 ************************************************/

#include "includes/mycli.h"
#include "includes/tokenizer.h"
#include "includes/executor.h"
#include "includes/internal.h"
#include "includes/rcreader.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

int main (int argc, char **argv)
{
    signal(SIGINT, SIG_IGN);

    read_myclirc();

    ListHandler tlist;
    tlist.head = NULL;
    tlist.tail = NULL;
    tlist.count = 0;

    char userin[BUFF_SIZE];
    while (!feof(stdin)) {
        char *PS1 = getenv("PS1");
        if (PS1 == NULL) {
            printf("$ ");
        } else {
            printf("%s", PS1);
        }
        if (fgets(userin, BUFF_SIZE, stdin) == NULL) {
            perror("failed to get user input");
            exit(0);
        }

        /* get tokenized input */
        tokenize(&tlist, userin);

        /* print the tokenized input */
//        print_tokens(tlist.head);

        /* exec tokenized input */
        if (tlist.head) {
            int ret = run_internal_cmd(tlist);
            if (ret < 0) {
                fprintf(stderr,"Unable to run internal command\n");
            } else if (ret == 0) {
            } else {
                execute(tlist.head);
            }
        }

        /* free the tokenized input */
        free_tok_list(&tlist);
    }

    return 0;
}
