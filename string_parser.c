/*
 * string_parser.c
 *
 *  Created on: Nov 25, 2020
 *      Author: gguan
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GUN_SOURCE
int count_token (char* buf, const char* delim)
{
	/*
	*	#1.	Check for NULL string
	*	#2.	iterate through string counting tokens
	*		Cases to watchout for
	*			a.	string start with delimeter
	*			b. 	string end with delimeter
	*			c.	account NULL for the last token
	*	#3. return the number of token (note not number of delimeter)
	*/
	
	if (buf == NULL) {
		return 0;
	}
	int tokenCount = 0;
	char *tempToFree;
	char *copy, *ptr;
	copy = strdup(buf);
	tempToFree = copy;
	copy = strtok_r(copy, delim, &ptr);
	while (copy != NULL) {
		copy = strtok_r(NULL, delim, &ptr);
		tokenCount++;
	}
	free(tempToFree);
	return tokenCount;
}

command_line str_filler (char* buf, const char* delim)
{
	/*
	*	#1.	create command_line variable to be filled
	*	#2.	use function strtok_r to take out \n character then count
	*			number of tokens with count_token function, set num_token.
	*	#3. malloc memory for token array inside command_line variable
	*			base on the number of tokens.
	*	#4. malloc each index of the array with the length of tokens,
	*			fill array with tokens, and fill last spot with NULL.
	*	#5. return the variable.
	*/
	// #1.
	command_line filled;
	int num_token = count_token(buf, delim);
    filled.command_list = NULL;
    filled.num_token = num_token;
	if (buf == NULL) {
        return filled;
    }
	// #2.
	char* theRest = " ";
	char* tempToFree = " "; // same purpose as tempToFree in count_token func
    char* token = strdup(buf);
	tempToFree = token;
	token = strtok_r(token, "\n", &theRest);
	// #3.
	filled.num_token = num_token;
    // TODO
	//filled.command_list = malloc(sizeof(char*) * (num_token+1));
    if (num_token < 1) {
        free(token);
        return filled;
    }
    filled.command_list = calloc((num_token + 1), sizeof(char*));
	// #4.
	int i = 0;
	token = strtok_r(token, delim, &theRest);
	while (token != NULL){
		filled.command_list[i] = strdup(token);
		token = strtok_r(NULL, delim, &theRest);
		++i;
	}
	filled.command_list[i] = NULL;
	// #5.
	free(tempToFree);
	return filled;
}


void free_command_line(command_line* command)
{
	/*
	*	#1.	free the array base num_token
	*/

	for (int i = 0; i < command->num_token; i++) {
		free(command->command_list[i]);
	}
	free(command->command_list);
}

