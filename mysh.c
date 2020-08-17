#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/wait.h>

//limits
#define MAX_TOKENS 100
#define MAX_STRING_LEN 100

size_t MAX_LINE_LEN = 10000;
int child = 0;
int stdInCopy;
int stdOutCopy;


// builtin commands
#define EXIT_STR "exit"
#define EXIT_CMD 0
#define UNKNOWN_CMD 99


struct string_array {
	int size;
	int capacity;
	char **strings;
};


struct string_array *string_array_init() {
	/*
		initialize a pointer to `struct string_array` by
		allocating memory, setting initial capacity and size.
	*/
	int initial_capacity = 4;
	struct string_array *string_arr = (struct string_array*) malloc(sizeof(struct string_array));
	string_arr->strings = (char**) malloc(sizeof(char*) * (initial_capacity + 1));
	string_arr->capacity = initial_capacity;
	string_arr->size = 0;
	// NULL terminator
	string_arr->strings[string_arr->size] = NULL;
	return string_arr;
}

void string_array_append(struct string_array *string_arr, char *string) {
	/*
		if size is equal to capacity,
		resize array
	*/
	if(string_arr->capacity == string_arr->size) {
		int new_capacity = ((string_arr->capacity * 3) / 2) + 1;
		string_arr->strings = (char**) realloc(string_arr->strings, sizeof(char*) * (new_capacity + 1));
		string_arr->capacity = new_capacity;
	}
	/*
		add element to the array
	*/
	string_arr->strings[string_arr->size] = string;
	string_arr->size += 1;
	// NULL terminator
	string_arr->strings[string_arr->size] = NULL;
}

void string_array_free(struct string_array *string_arr) {
	// free up memory
	free(string_arr->strings);
	free(string_arr);
}


struct string_array *tokenize(char * string)
{
	// break the string into individual token
	struct string_array *string_arr = string_array_init();
	char *token;

	// split by whitespace characters
	while ( (token = strsep( &string, " \t\v\f\n\r")) != NULL) {
		if (*token == '\0') continue;
		string_array_append(string_arr, token);
	}
	return string_arr;
}

char *read_command(FILE *fp) 
{
	char *line;
	assert( (line = malloc(sizeof(char) * MAX_STRING_LEN)) != NULL);
	// getline will reallocate if input exceeds max length
	assert( getline(&line, &MAX_LINE_LEN, fp) > -1); 
	return line;
}


void handle_command(char **cmd, int inf, int outf) {
	struct string_array *command = string_array_init();
	int index = 0;
	char *input_redirected = NULL, *output_redirected = NULL;
	// loop through all the tokens
	while(cmd[index] != NULL) {
		if(strcmp(cmd[index], ">") == 0) {
			// `>` means next token is a filename
			// where the output needs to be redirected to
			output_redirected = cmd[++index];
			index++;
		} else if(strcmp(cmd[index], "<") == 0) {
			// `<` means next token is a filename
			// from where input needs to be taken
			input_redirected = cmd[++index];
			index++;
		} else {
			string_array_append(command, cmd[index++]);
		}
	}
	if(input_redirected != NULL) {
		// if input is redirected handle it
		int fd = open(input_redirected, O_RDONLY, S_IRUSR | S_IWUSR);
		dup2(fd, STDIN_FILENO);
		close(fd);
	} else {
		// otherwise use the given stream
		dup2(inf, STDIN_FILENO);
	}
	if(output_redirected != NULL) {
		// if outut is redirected handle it
		int fd = open(output_redirected, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		dup2(fd, STDOUT_FILENO);
		close(fd);
	} else {
		// otherwise use the given stream
		dup2(outf, STDOUT_FILENO);
	}
	// execute the command
	execvp(command->strings[0], command->strings);
	string_array_free(command);
}


void execute_pipeline(char ***cmds, size_t pos, int in_fd) {
	if (cmds[pos + 1] == NULL) {
		// this is the last command
		handle_command(cmds[pos], in_fd, stdOutCopy);
	} else {
		int fd[2];
		pipe(fd);
		if(fork() == 0) {
			// this is child process
			child = 1;
			close(fd[0]);
			// run the command and exit
			handle_command(cmds[pos], in_fd, fd[1]);
			exit(0);
		} else {
			// this parent process
			close(fd[1]);
			close(in_fd);
			// pipe all commands to form a chain
			execute_pipeline(cmds, pos + 1, fd[0]);
		}
	}
}

void pipe_commands(struct string_array *tokens) {
	int index = 0;
	int cmds_size = 100, cmds_done = 0;
	char ***cmds = (char***) malloc(sizeof(char***) * cmds_size + 1);
	struct string_array *cmd = string_array_init();
	// loop through all the tokens
	while(index < tokens->size) {
		if(strcmp(tokens->strings[index], "|") == 0 || index == tokens->size - 1) {
			if(strcmp(tokens->strings[index], "|") != 0) {
				// if this is the last token, append the commad
				string_array_append(cmd, tokens->strings[index]);
			}
			// if array limit reached, resize array before appending
			if(cmds_done == cmds_size) {
				cmds_size *= 2;
				cmds = (char***) realloc(cmds, sizeof(char***) * cmds_size + 1);
			}
			cmds[cmds_done++] = cmd->strings;
			// free the struct
			free(cmd);
			// initialize new struct, for the next command
			cmd = string_array_init();
		} else {
			string_array_append(cmd, tokens->strings[index]);
		}
		index++;
	}
	string_array_free(cmd);
	cmds[cmds_done] = NULL;
	// save input and output streams
	dup2(stdInCopy, STDIN_FILENO);
	dup2(stdOutCopy, STDOUT_FILENO);
	if(fork() == 0) {
		// child process, executes the commands
		execute_pipeline(cmds, 0, STDIN_FILENO);
		exit(0);
	} else {
		// parent process waits until all the
		// child processes have completed
		wait(NULL);
		// free up used resources
		for(int i=0; i<cmds_done; i++) {
			free(cmds[i]);
		}
		free(cmds);
	}
}

int run(struct string_array *tokens) {
	if(strcmp(tokens->strings[0], EXIT_STR) == 0) {
		// if exit command was given, quit
		return EXIT_CMD;
	}
	pipe_commands(tokens);
	return UNKNOWN_CMD;
}

int main()
{
	FILE *fp; // file struct for stdin
	struct string_array *tokens;
	char *line;
	int command;

	stdInCopy = dup(STDIN_FILENO);
	stdOutCopy = dup(STDOUT_FILENO);
	
	assert( (fp = fdopen(STDIN_FILENO, "r")) != NULL);

	while(1) {
		printf("mysh> ");
		// read line and extract tokens
		line = read_command(fp);
		tokens = tokenize(line);

		// run the command
		command = run(tokens);

		// free allocated memory
		string_array_free(tokens);
		free(line);
		if(command == EXIT_CMD) {
			break;
		}
	}
	fclose(fp);
	return 0;
}
