#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h> // PATH_MAX 

#define TRUE 1
#define FALSE 0
typedef int bool;

char* readNormalizeFillBuffer(const char *fileName, long *doubleFileSize);
char** createTokens(const char *fileContent, const long fileSize, int *numOfTokens);
bool isCommandKey(const char * token);
bool isControlKey(const char * token);
bool isKey(const char * token);
bool isPathValid(char **tokens, int pathStart, int pathEnd);
bool arePathTokensValid(char **tokens, int numOfTokens, int startIndex, int *endIndex);
bool areCommandTokensValid(char **tokens, int numOfTokens, int startIndex, int *endIndex);
bool areControlTokensValid(char **tokens, int numOfTokens, int startIndex, int *endIndex);
bool validateTokens(char **tokens, int numOfTokens, int startPosition, int *endPosition, int useSeparator);
char** deleteCommentTokens(char **tokens, int *numOfTokens);

void writeExecutablesToFile(const char *fileName, char **tokens, int numOfTokens);
void readAndExecute(const char *fileName);

int firstOccurrenceIndex(const char *str, const char x);
int firstOccurrenceIndexStr(char **tokens, int numOfTokens, int startIndex, const char * searchedStr);
int lastOccurrenceIndexStr(char **tokens, int numOfTokens, int startIndex, const char * searchedStr);
int findOccurrenceTimes(const char *str, const char x);
bool isAlphabet(const char x);
bool isDigit(const char x);
bool isExpectedSymbol(const char x);
bool isValidPathExpression(const char *pathExp);
bool isFolderNameValid(const char *folderName);
bool isFolderExisting(const char *path);
bool makeRecursive(const char *path, const int pathLength);
bool makeCommand(const char *path, const int pathLength);
bool goCommand(const char *path);
void deleteCommandRecursive(const char *path);
void deleteCommand(const char *path);
bool ifCommand(const char *pathControl, const char *command, const char *pathCommand);
bool ifnotCommand(const char *pathControl, const char *command, const char *pathCommand);
void printCWD();

void test_isValidPathExpression();
void printTokens(char **tokens, int numOfTokens);
void freeAllMem(char *buffer, char **tokens, int tokensNum);

/*
	Task: Path_maker is a basic scripting language for creating 
	directory trees. (Rules of the language are below). This is 
	a basic interpreter for Path_maker.

	Basic Commands:
	go		<path_expression>;
	make	<path_expression>;
	delete 	<path_expression>;
	
	Control Structures:
	if		<path_expression> command
	ifnot 	<path_expression> command

	command could be any Basic Command or Block
	
	Blocks: Between {}, they can be nested
	Comments: Between [], they cannot be nested
	
	Note: Nested blocks are not fully functioning!

*/
int main(int argc, char *argv[]) {
	// Buffer to hold the file content
	// Not memory efficient nor good for big files, 
	// but fast for implementation and meets the assignment requirements
	char *buffer_fc;
	long buffer_size = 0;
	char **tokens;
	int i, numOfTokens = 0, startParsingPosition = 0, endParsingPosition = 0;
	// Used only for better tracking/debugging
	int useSeparator = 1;
	bool areTokensValid = FALSE;
	char *executableCommands;
	
	// Change the input file name from here
	buffer_fc = readNormalizeFillBuffer("memo.pmk", &buffer_size);

		fprintf(stdout, "BufferSize: %ld\n", buffer_size);
		fprintf(stdout, "BufferLen: %ld\n", strlen(buffer_fc));
		fprintf(stdout, "__________BufferContent__________\n");
		fprintf(stdout, "%s", buffer_fc);
		fprintf(stdout, "_________________________________\n");

	tokens = createTokens(buffer_fc, buffer_size, &numOfTokens);

		fprintf(stdout, "numOfTokens:%d\n", numOfTokens);
		if(numOfTokens < 1){
			fprintf(stderr, "No possible tokens, tokens num:%d!\n", numOfTokens);
			// Freeing all allocated memory space for buffer and tokens
			freeAllMem(buffer_fc, tokens, numOfTokens);
			exit(1);
		}

		printTokens(tokens, numOfTokens);

		fprintf(stdout, "__________<<<Deleting Comment Tokens>>>__________\n");
	tokens = deleteCommentTokens(tokens, &numOfTokens);
		fprintf(stdout, "_________________________________________________\n");

		if(numOfTokens < 1){
			fprintf(stderr, "No possible tokens, tokens num:%d!\n", numOfTokens);
			// Freeing all allocated memory space for buffer and tokens
			freeAllMem(buffer_fc, tokens, numOfTokens);
			exit(1);
		}

		fprintf(stdout, "numOfTokens:%d\n", numOfTokens);
		printTokens(tokens, numOfTokens);

		fprintf(stdout, "__________<<<Tokens verification>>>__________\n");

	areTokensValid = validateTokens(tokens, numOfTokens, startParsingPosition, &endParsingPosition, useSeparator);

		if(areTokensValid){
			fprintf(stdout, "__________<<<All tokens are valid!>>>__________\n");
		}
		else{
			fprintf(stderr, "__________<<<One or more tokens are invalid!>>>__________\n");
			fprintf(stdout, "Program is exiting, please fix your syntax errors!\n");
			// Freeing all allocated memory space for buffer and tokens
			freeAllMem(buffer_fc, tokens, numOfTokens);
			exit(1);
		}

	// Write tokens to a file which is formatted for executing commands
	writeExecutablesToFile("___temp___.pmk", tokens, numOfTokens);

	// Freeing all allocated memory space for buffer and tokens
	freeAllMem(buffer_fc, tokens, numOfTokens);

	// Read and execute commands
	readAndExecute("___temp___.pmk");

	// Remove the temporary file
	if(remove("___temp___.pmk") == 0){
		fprintf(stdout, "____________________________\n");
		fprintf(stdout, "The temp file was deleted!\n");
	}
	else{
		fprintf(stdout, "____________________________\n");
		fprintf(stdout, "The temp file was not deleted!");
		fprintf(stdout, " Delete the temp file before running again!\n");
		fprintf(stdout, "Most probably the script you executed changed CWD ;)\n");
	}

    return 0;
}

// Read from .pmk file, normalize and return buffer with fileContent
// The function allocates space which must be freed after
// Normalization Process to make the content strtok() parceable:
// - If there isn't a whitespace before and after each keyword, whitespace is added
// - If there isn't a whitespace before and after each symbol, whitespace is added
//   Symbols included: {}<>[];/
// - If there is a "*" symbol it's changed with ".." to make it Linux compatible
// - Whitespaces are removed from paths as much as possible 
// 	 But not if they are in the name of the dir which is not possible
char* readNormalizeFillBuffer(const char *fileName, long *doubleFileSize) {
    FILE* fPtr = fopen(fileName, "r");
    char *fileContent;
    int index = 0;
    int char_ascii;
    char currChar, prevChar;

    // Sets the file position of the stream at the end of the file
    fseek(fPtr, 0, SEEK_END);
    // Finds the size of the file (char count to be allocated)
    // Multiplies the size with 2 to make sure there is enough space
    *doubleFileSize = ftell(fPtr) * 2;
    // Sets the file position of the stream at the beggining of the file
	fseek(fPtr, 0, SEEK_SET);

 	// Allocate enough buffer space
 	// Just to make sure we do not run out of space
 	// while adding additional whitespaces
 	fileContent = (char *) malloc((*doubleFileSize) * sizeof(char));

	// Put a space at index 0
 	fileContent[index++] = ' ';
 	prevChar = ' ';

 	// Read the file char by char and process
 	while ((char_ascii = fgetc(fPtr)) != EOF) {
 		currChar = (char)char_ascii;

 		// If the current char is a whitespace
 		if(currChar == ' ' && prevChar == ' '){
 			// and the previous char was a whitespace
 			if(prevChar == ' '){
 				// skip
 				continue;
 			}
 		}

 		// If the current char is one of {}<>[];
 		if(isExpectedSymbol(currChar)){
 			// and the previous char was not a whitespace
 			if(prevChar != ' '){
				// Insert a whitespace before the current char
				// and update the value of previous char
				prevChar = fileContent[index++] = ' ';
 			}
 		}

 		// If the previous char was one of {}<>[];
 		if(isExpectedSymbol(prevChar)){
 			// and the current char is not a whitespace
 			if(currChar != ' '){
				// Insert a whitespace before the current char
				prevChar = fileContent[index++] = ' ';
 			}
 		} 

 		// If the current char is a '*' change it with ".."
 		if(currChar == '*'){
 			prevChar = fileContent[index++] = '.';
 			prevChar = fileContent[index++] = '.';
 			// skip saving the '*'
 			continue;
 		}

 		// Skip saving back to back '\n'
 		if(currChar == '\n' && prevChar == '\n'){
 			continue;
 		}

 		// Save the current char into the buffer 
 		// and update the value of previous char
 		prevChar = fileContent[index++] = currChar;
	}

	// Put null char at the end
    fileContent[index] = '\0';

	//fprintf(stdout, "DoubleFileSize: %ld\n", *doubleFileSize);
	//fprintf(stdout, "FileContentLen: %ld\n", strlen(fileContent));
    //fprintf(stdout, "FileContent:\n___\n%s\n___\n", fileContent);

    fclose(fPtr);
    return fileContent;
}

// Creates tokens from the file content
char** createTokens(const char *fileContent, const long fileSize, int *numOfTokens){
	char *currentToken, *buffer = malloc(fileSize);
	char tokenDelim[7] = " \t\n\v\f\r";
	char **tokens;
	int i=0;

	// Copy the fileContent to the buffer, 
	// because strtok modifies content
	// First iteration is to just count 
	// tokens and get their sizes
	strncpy(buffer, fileContent, fileSize);

	// separate the buffer to tokens and count them
	currentToken = strtok(buffer, tokenDelim);
	while(currentToken != NULL){		
		currentToken = strtok(NULL, tokenDelim);
		++(*numOfTokens);
	}

	// Allocate space for numOfTokens
	tokens = (char**) malloc((*numOfTokens) * sizeof(char *));

	// Initialize the buffer again
	// This time to save the tokens
	strncpy(buffer, fileContent, fileSize);

	// separate buffer to tokens and iterate them
	currentToken = strtok(buffer, tokenDelim);
	while(currentToken != NULL){
		// Allocate space for the token and save it
		tokens[i] = (char*) malloc(strlen(currentToken)+1 * sizeof(char));
		strncpy(tokens[i], currentToken, strlen(currentToken)+1);
		currentToken = strtok(NULL, tokenDelim);
		++i;
	}

	// free the allocated buffer
	if(buffer != NULL)
		free(buffer);

	return tokens;
}

// Checks whether a token is a command keyword
bool isCommandKey(const char * token){
	return ((strcmp(token, "go") == 0) || 
			(strcmp(token, "make") == 0) || 
			(strcmp(token, "delete") == 0));
}

// Checks whether a token is a control keyword
bool isControlKey(const char * token){
	return ((strcmp(token, "if") == 0) || 
			(strcmp(token, "ifnot") == 0));
}

// Ckecks whether a token is a keyword
bool isKey(const char * token){

	return (isCommandKey(token) || isControlKey(token));
}

// Used internally inside arePathTokensValid function
// Checks directory names' syntax
bool isPathValid(char **tokens, int pathStart, int pathEnd){
	// Is Folder Name Valid
	bool isFNValid = TRUE;
	int i;

	if(strcmp(tokens[pathStart], "/") == 0){
		fprintf(stderr, "\t\t\tPath cannot start with \"/\"!\n");
		return FALSE;
	}

	if(strcmp(tokens[pathEnd-1], "/") == 0){
		fprintf(stderr, "\t\t\tPath cannot end with \"/\"!\n");
		return FALSE;
	}

	// Validate the path
	for(i=pathStart; i<pathEnd; i += 2){
		if(tokens[i] != NULL){
			isFNValid = isFolderNameValid(tokens[i]);
			if(!isFNValid){
				fprintf(stderr, "\t\tDirectory name \"%s\" is invalid!\n", tokens[i]);
				return FALSE;
			}
		}

		if(tokens[i+1] != NULL && i<pathEnd-1){
			if(strcmp(tokens[i+1], "/") != 0){
				fprintf(stderr, "\t\tDirectory name \"%s %s\" cannot have space!\n", tokens[i], tokens[i+1]);
				return FALSE;
			}
		}
	}

	return TRUE;
}

// Checks if path tokens are valid
// startIndex hold's '<' symbols index
// endIndex will be assigned with '>' symbols index
bool arePathTokensValid(char **tokens, int numOfTokens, int startIndex, int *endIndex){
	// Is Path Valid
	bool isPValid = TRUE;
	int path_start_index = startIndex;

	// Case when there is a command keyword but not enough tokens for <path_expression>
	if(path_start_index >= numOfTokens){
		fprintf(stderr, "\tSyntax Error! \"%s\" command's path is missing components!\n", tokens[startIndex-1]);
		return FALSE;
	}

	// If there is not a '<' at the path beginning
	if(strcmp(tokens[path_start_index], "<") != 0){
		fprintf(stderr, "\tSyntax Error! \"<\" expected, but \"%s\" received!\n", tokens[path_start_index]);
		return FALSE;
	}

	// endIndex holds the index of a '>' symbol
	*endIndex = firstOccurrenceIndexStr(tokens, numOfTokens, path_start_index, ">");

	// The case where a '>' symbol was found
	if(*endIndex != -1){
		// The case where '<' is not followed by '>'
		if(*endIndex != 0){
			// Checks if the path is valid
			// if not prints the error
			if(!isPathValid(tokens, path_start_index+1, *endIndex)){
				isPValid = FALSE;
				fprintf(stderr, "\tPath between \"<>\" is invalid!\n");
			} 
		} 
		else{
			isPValid = FALSE;
			fprintf(stderr, "\tNo specified path between \"<>\"!\n");
		}
	} 
	else{
		isPValid = FALSE;
		if(numOfTokens-1 >= 0 && tokens[numOfTokens-1] != NULL){
			fprintf(stderr, "\t\">\" expected, but \"%s\" received!\n", tokens[numOfTokens-1]);
		}
	}

	return isPValid;
}

// Returns TRUE if the command tokens are valid
// endIndex holds the index of matching ";" symbol
// if endIndex is not found, -1 will be assigned
bool areCommandTokensValid(char **tokens, int numOfTokens, int startIndex, int *endIndex){
	int i, path_end_index, semicolon_index;
	// Is Command Valid
	bool isCValid = TRUE;

	// Holds the index of this command's name
	int bcn_idx = startIndex;
	// Holds the index of '<' symbol
	int path_start_index = startIndex + 1;

	fprintf(stdout, "BasicCommand:[%s]S_idx[%d]\n", tokens[bcn_idx], bcn_idx);
	// Checks if path tokens are valid
	// path_start_index holds the index of the openning '<'
	// path_end_index will be assigned with the '>' symbol's 
	// index value, if '>' is missing, -1 
	if(arePathTokensValid(tokens, numOfTokens, path_start_index, &path_end_index)){
		// if there is a '>' symbol
		if(path_end_index != -1){
			// a ';' symbol must be right after the '>'
			semicolon_index = path_end_index + 1;

			// Checks if semicolon_index does not overflow numOfTokens
			// Checks if there is a ';' symbol after the '>' symbol
			if(semicolon_index < numOfTokens && strcmp(tokens[semicolon_index], ";") == 0){
				*endIndex = semicolon_index;
			} else {
				fprintf(stderr, "\tSyntax Error! \"%s\" command is missing \";\" at the end!\n", tokens[bcn_idx]);
				isCValid = FALSE;
			}			
		}
	} else {
		fprintf(stderr, "\tSyntax Error! \"%s\" command's path is faulty!\n", tokens[bcn_idx]);
		isCValid = FALSE;
	}

	fprintf(stdout, "BasicCommand:[%s]S_idx[%d]E_idx[%d]\n", tokens[bcn_idx], bcn_idx, *endIndex);

	return isCValid;
}

// Returns TRUE if the control is valid
// endIndex holds the index of matching ";" or "}" symbol
// if endIndex is not found, -1 will be assigned
// In case there is a block, the function will call recursively validateTokens function
bool areControlTokensValid(char **tokens, int numOfTokens, int startIndex, int *endIndex){
	int i, path_start_index, path_end_index;
	int command_start_index, command_end_index, block_start_index, block_end_index;
	int csn_idx;
	// Is Command Valid
	bool isCValid = TRUE;

	// Holds the index of this command's name
	csn_idx = startIndex;
	// Holds the index of '<' symbol
	path_start_index = startIndex + 1;

	fprintf(stdout, "ControlStructure:[%s]S_idx[%d]\n", tokens[csn_idx], csn_idx);

	// Checks if path tokens are valid
	// path_start_index holds the index of the openning '<'
	// path_end_index will be assigned with the '>' symbol's 
	// index value, if '>' is missing, -1 
	if(arePathTokensValid(tokens, numOfTokens, path_start_index, &path_end_index)){
		// if there is a '>' symbol
		if(path_end_index != -1){
			// The next token can be either a basic command
			// or a '{' symbol which indicates the start of a block

			// If the token is a command key
			if(isCommandKey(tokens[path_end_index+1])){
				command_start_index = path_end_index+1;

				if(areCommandTokensValid(tokens, numOfTokens, command_start_index, &command_end_index)){
					// Update the endIndex of the caller, so it points 
					// to the next basic command or control structure
					*endIndex = command_end_index;
				} else {
					fprintf(stderr, "Syntax Error! \"%s\" command's syntax is faulty!\n", tokens[command_start_index]);
					isCValid = FALSE;
				}
			}
			// If the token is a '{'
			else if(strcmp(tokens[path_end_index+1], "{") == 0){
				// a '{' symbol must be right after the '>'
				block_start_index = path_end_index + 1;
				block_end_index = lastOccurrenceIndexStr(tokens, numOfTokens, block_start_index, "}");

				fprintf(stdout, "Block:[%s]S_idx[%d]E_idx[%d]\n", tokens[block_start_index], block_start_index, block_end_index);

				// Call validateTokens recursively to evaluate blocks and inner blocks if any
				// Only tokens between the {} symbols are sent, {} are excluded
				// Do not use separator
				if(validateTokens(tokens, numOfTokens, block_start_index, &block_end_index, 0)){
					fprintf(stdout, "Block:[%s]S_idx[%d]E_idx[%d]\n", tokens[block_end_index], block_start_index, block_end_index);
					*endIndex = block_end_index;
				} else {
					fprintf(stderr, "\tSyntax Error! \"%s\" control has a faulty block data!\n", tokens[csn_idx]);
					isCValid = FALSE;
				}
			} else {
				fprintf(stderr, "\tSyntax Error! \"%s\" control is missing a \"{\" or a Basic Command!\n", tokens[csn_idx]);
				isCValid = FALSE;
			}		
		}
	} else {
		fprintf(stderr, "\tSyntax Error! \"%s\" control's path is faulty!\n", tokens[csn_idx]);
		isCValid = FALSE;
	}

	fprintf(stdout, "ControlStructure:[%s]S_idx[%d]E_idx[%d]\n", tokens[csn_idx], csn_idx, *endIndex);

	return isCValid;
}

// Validates tokens
// startPosition and endPositions are used for recursive iterations in case
// a control structure has a block as a command
bool validateTokens(char **tokens, int numOfTokens, int startPosition, int *endPosition, int useSeparator){
	bool areTokensValid = TRUE, wasMatchingFound = FALSE;
	int i, j, index, startIndex, endIndex;
	int tab_count = 0; // used just for better tracking/debugging 
	int bcn_idx, // basic command name index
		csn_idx; // control structure name index

	//if(startPosition >= numOfTokens)
	//	return areTokensValid;

	// Iterate all tokens and validate
	for(i=startPosition; i<numOfTokens; ++i){

		// Stop iterating if there is a faulty token
		if(!areTokensValid)
			break;

		// If the token is a keyword
		if(isKey(tokens[i])){
			// If the token is a command keyword
			if(isCommandKey(tokens[i])){
				bcn_idx = i;
				// Checks if all tokens for this command are valid
				// endIndex will be assigned with the index value of the ';' symbol
				// which must be at the end of every valid command
				if(areCommandTokensValid(tokens, numOfTokens, bcn_idx, &endIndex)){
					// Update the loop iterator, so it points 
					// to the next basic command or control structure
					i = endIndex;
				} else {
					fprintf(stderr, "Syntax Error! \"%s\" command's syntax is faulty!\n", tokens[i]);
					areTokensValid = FALSE;
				}
			}
			// The token is a control keyword
			else{
				csn_idx = i;
				// Checks if all tokens for this command are valid
				// endIndex will be assigned with the index value of the ';' symbol
				// which must be at the end of every valid command
				if(areControlTokensValid(tokens, numOfTokens, csn_idx, &endIndex)){
					// Update the loop iterator, so it points 
					// to the next basic command or control structure
					i = endIndex;
				} else {
					fprintf(stderr, "Syntax Error! \"%s\" control's syntax is faulty!\n", tokens[i]);
					areTokensValid = FALSE;
				}
			}
		} 
		// if the token is a '{'
		else if(strcmp(tokens[i], "{") == 0){
			// Check if there is a closing '}'
			for(j=i+1; j<numOfTokens; ++j){
				// If there is, skip this token
				if(strcmp(tokens[j], "}") == 0){
					wasMatchingFound = TRUE;
					break;
				}
			}

			if(!wasMatchingFound){
				fprintf(stderr, "Syntax error! \"}\" does not have an openning match!\n");
				areTokensValid = FALSE;
			}

			// reset
			wasMatchingFound = FALSE;
			continue;

		} 
		// if the token is a '}'
		else if(strcmp(tokens[i], "}") == 0){
			// Check if there is an opening '{' before this token
			for(j=numOfTokens-1; j>=0; --j){
				// If there is, skip this token
				if(strcmp(tokens[j], "{") == 0){
					wasMatchingFound = TRUE;
					break;
				}
			}

			if(!wasMatchingFound){
				fprintf(stderr, "Syntax error! \"}\" does not have an openning match!\n");
				areTokensValid = FALSE;
			}

			// reset
			wasMatchingFound = FALSE;
			// Since we already iterated the current block, 
			// it's time to return back
			return areTokensValid;
		} 
		// Unknown token format
		else
		{
			fprintf(stderr, "Syntax error! \"%s\" was not expected at this position!\n", tokens[i]);
			areTokensValid = FALSE;
		}
		if(useSeparator == 1)
			fprintf(stdout, "________________SEPARATOR________________\n");
	}

	return areTokensValid;
}

// Removes the comment related tokens
char** deleteCommentTokens(char **tokens, int *numOfTokens){
	int i, j = 0;
	char **no_comm_tokens;
	int no_comm_tokens_num = *numOfTokens;
	int currNumOfTokens = *numOfTokens;
	int difference;
	// when set to true, will ignore tokens
	// '[' symbol sets to TRUE
	// ']' symbol sets to FALSE
	bool ignore = FALSE;

	for(i=0; i<currNumOfTokens; ++i){
		if(strcmp(tokens[i], "[") == 0){
			--no_comm_tokens_num;
			ignore = TRUE;
			continue;
		}

		if(strcmp(tokens[i], "]") == 0){
			--no_comm_tokens_num;
			ignore = FALSE;
			continue;
		}

		if(ignore)
			--no_comm_tokens_num;
	}

	printf("NewTokenNum:%d\n", no_comm_tokens_num);
	difference = currNumOfTokens - no_comm_tokens_num;
	printf("DeletedCommentTokenNum:%d\n", difference);

	// Allocate space for no comment tokens
	no_comm_tokens = (char**) malloc(no_comm_tokens_num * sizeof(char *));

	for(i=0; i<currNumOfTokens; ++i){
		if(strcmp(tokens[i], "[") == 0){
			ignore = TRUE;
		}

		// If not ignoring, safe the tokens
		if(!ignore){
			no_comm_tokens[j] = (char*) malloc(strlen(tokens[i])+1 * sizeof(char));
			strncpy(no_comm_tokens[j], tokens[i], strlen(tokens[i])+1);
			++j;
		}

		if(strcmp(tokens[i], "]") == 0){
			ignore = FALSE;
		}
	}

	// free tokens memory
	if(tokens != NULL){
		for(i=0; i<currNumOfTokens; ++i){
			if(tokens[i] != NULL)
				free(tokens[i]);
		}
		free(tokens);
	}

	*numOfTokens = no_comm_tokens_num;
	return no_comm_tokens;
}

// Combines tokens and creates executable commands
// The commands are saved into a temp file from where
// they will be read and executed
void writeExecutablesToFile(const char *fileName, char **tokens, int numOfTokens){
	int i, j;

	//printTokens(tokens, numOfTokens);

	remove(fileName);
	FILE* fPtr = fopen(fileName, "w");

	for(i=0; i<numOfTokens; ++i){
		if(isCommandKey(tokens[i])){
			// Write the command keyword
			fprintf(fPtr, "%s ", tokens[i++]);
			++i; // Skip the "<" token

			// Write the path without <>
			while(strcmp(tokens[i], ">") != 0){
				fprintf(fPtr, "%s", tokens[i++]);
			}
			++i; // Skip the ">" token

			// Write a semicolon ";"
			fprintf(fPtr, " %s\n", tokens[i]);

		} else if(isControlKey(tokens[i])){
			// Write the control keyword
			fprintf(fPtr, "%s ", tokens[i++]);
			++i; // Skip the "<" token

			// Write the path without <>
			while(strcmp(tokens[i], ">") != 0){
				fprintf(fPtr, "%s", tokens[i++]);
			}

			if((i+1<numOfTokens) && strcmp(tokens[i+1], "{") == 0){
				// Write Block openning
				fprintf(fPtr, " {\n");
				++i;
			} else {
				// Write a colon ":"
				fprintf(fPtr, " : ");				
			}

		} else {
			// Case for writing "}"
			fprintf(fPtr, "%s\n", tokens[i]);
		}
	}
	fprintf(fPtr, "~~~\n");

	fclose(fPtr);
}

// This function is used as a helper inside readAndExecute
// To handle the nested block cases, this function will be called recursively
// if the read line is equal to the stopper, the function will return
// skip_to_stopper is set to 1 to skip executing block in case 
// the "if" of that block returned FALSE result
void readAndExecuteHelper(FILE* fPtr, char *stopper, int skip_to_stopper){
	char lineFromFile[PATH_MAX*4];
	char *currentToken;
	char tokenDelim[3] = " \n";
	char *keyword, *command, *firstPath, *secondPath, *controlSymbol;

	while(fgets(lineFromFile, PATH_MAX*4, fPtr) != NULL) {
		//printf("%s", lineFromFile);

		// End condition  of reading
		// special line inserted inside the file
		if(strncmp(lineFromFile, "~~~", 3) == 0)
			break;

		keyword = strtok(lineFromFile, tokenDelim);

		// If the keyword is equal to the stopper, return
		// the stopper is either "}" to return from block recursion
		// or is "~~~" to indicate file end line
		if(strcmp(keyword, stopper) == 0)
			return;

		if(skip_to_stopper)
			continue;

		firstPath = strtok(NULL, tokenDelim);

		// Is the keyword a command one
		if(isCommandKey(keyword)){
			// Execute the command
			if(strcmp(keyword, "make") == 0){
				makeCommand(firstPath, strlen(firstPath));
			} else if(strcmp(keyword, "go") == 0){
				goCommand(firstPath);
			} else if(strcmp(keyword, "delete") == 0){
				deleteCommand(firstPath);
			} else {
				fprintf(stderr, "ERROR_READ_EXECUTE_COMMAND\n");
				fprintf(stderr, "key[%s]_firstPath[%s]", keyword, firstPath);
			}
			continue;
		} 
		// The keyword is a control one
		else {
			// Get control symbol
			controlSymbol = strtok(NULL, tokenDelim);

			// We have only single command after control
			if(strcmp(controlSymbol, ":") == 0){
				// Get the command
				command = strtok(NULL, tokenDelim);
				// Get the commandPath
				secondPath = strtok(NULL, tokenDelim);

				if(strcmp(keyword, "if") == 0){
					// firstPath is controlPath
					ifCommand(firstPath, command, secondPath);
				} else if(strcmp(keyword, "ifnot") == 0){
					ifnotCommand(firstPath, command, secondPath);
				} else {
					fprintf(stderr, "ERROR_READ_EXECUTE_CONTROL\n");
					fprintf(stderr, "key[%s]_firstPath[%s]", keyword, firstPath);
				}
			} 
			else if(strcmp(controlSymbol, "{") == 0){
				// the control symbol is "{"
				// recurse inside the block if the 
				// condition of the "if" before this 
				// block is TRUE

				fprintf(stdout, "Checking: \"%s <%s>\"\n", keyword, firstPath);

				// keyword is if
				if(strcmp(keyword, "if") == 0){
					// the stopper is set to "}"
					if(isFolderExisting(firstPath)){
						fprintf(stdout, "  if returned TRUE for <%s>\n", firstPath);
						fprintf(stdout, "The block will be executed!\n");
						// The "if" returned TRUE, don't skip the block
						readAndExecuteHelper(fPtr, "}", 0);					
					} else {
						fprintf(stdout, "  if returned FALSE for <%s>\n", firstPath);
						fprintf(stdout, "The block will be skipped!\n");
						// The "if" returned FALSE, skip the block
						readAndExecuteHelper(fPtr, "}", 1);
					}	
				} 
				// keyword is ifnot
				else {
					// the stopper is set to "}"
					if(!isFolderExisting(firstPath)){
						fprintf(stdout, "  ifnot returned TRUE for <%s>\n", firstPath);
						fprintf(stdout, "The block will be executed!\n");
						// The "ifnot" returned TRUE, don't skip the block
						readAndExecuteHelper(fPtr, "}", 0);					
					} else {
						fprintf(stdout, "  ifnot returned FALSE for <%s>\n", firstPath);
						fprintf(stdout, "The block will be skipped!\n");
						// The "ifnot" returned FALSE, skip the block
						readAndExecuteHelper(fPtr, "}", 1);
					}	
				}


			} else {
				fprintf(stderr, "ERROR_READ_EXECUTE_CONTROL\n");
				fprintf(stderr, "controlSymbol[%s]", controlSymbol);
			}
			continue;
		}
	}
}

// Reads commands from a file and executes them
void readAndExecute(const char *fileName){
	FILE* fPtr = fopen(fileName, "r");

	// Remove the temporary file
	remove("___temp___.pmk");
	
	// The stopper is set to "~~~"
	readAndExecuteHelper(fPtr, "~~~", 0);

	fclose(fPtr);
}

// Checks whether a char is an alphabet char
bool isAlphabet(const char x){ 
	return ((x >= 'a' && x <= 'z') || 
			(x >= 'A' && x <= 'Z')); 
}

// Checks whether a char is a digit char
bool isDigit(const char x){ 

	return (x >= '0' && x <= '9'); 
}

// Returns TRUE if the char x is 
// one of the following symbols: {}<>[];/
bool isExpectedSymbol(const char x){
	if((x == '{') || (x == '}') ||
	   (x == '<') || (x == '>') || 
	   (x == '[') || (x == ']') ||
	   (x == ';') || (x == '/')){
		return TRUE;
	}

	return FALSE;
}

// Searches for a character inside a string 
// and returns first occurrence index,
// returns -1 if no occurrence
int firstOccurrenceIndex(const char *str, const char x){
	int i;
	if(str == NULL) return -1;

	for(i = 0; str[i] != '\0'; ++i)
		if(str[i] == x) return i;

	return -1;
}

// Searches for a string inside strings(tokens) 
// and returns first occurrence index,
// returns -1 if no occurrence
int firstOccurrenceIndexStr(char **tokens, int numOfTokens, int startIndex, const char * searchedStr){
	int i;
	if(tokens == NULL) return -1;

	for(i = startIndex; i < numOfTokens; ++i){
		if(strcmp(tokens[i], searchedStr) == 0)
			return i;
	}

	return -1;
}

// Searches for a string inside strings(tokens) 
// and returns last occurrence index,
// returns -1 if no occurrence
int lastOccurrenceIndexStr(char **tokens, int numOfTokens, int startIndex, const char * searchedStr){
	int i, lastIndex = -1;
	if(tokens == NULL) return -1;

	for(i = startIndex; i < numOfTokens; ++i){
		if(strcmp(tokens[i], searchedStr) == 0)
			lastIndex = i;
	}

	return lastIndex;
}

// Counts how many times char x is inside the str
// returns the times
int findOccurrenceTimes(const char *str, const char x){
	int times = 0;

	if(str == NULL) return 0;

	while(*str != '\0'){
		if(*str == x)
			++times;

		++str;
	}

	return times;
}

// Checks whether the path expression is valid
// Rules:
// - Parent ".." can only be used at the beginning of path expressions
//   - <hi/../there> -> not valid
// - Operator / cannot be used at the beginning or the end of any path
//   - </hi/there> or <hi/there/> -> not valid
// - Blanks in a path expression are ignored (unless they exist in a dir name)
//   - < .. /.. / mydirectory> -> allowed
//   - < .. /.. / mydi rectory> -> not allowed
// Pass any string between <> to verify validity
bool isValidPathExpression(const char *pathExp){
	int i, tokensNum = 0, slashsNum = 0;
	bool isTValid = TRUE, isExpValid = TRUE, wasPrevTParent = TRUE;
	char *currentToken, buffer[PATH_MAX];
	char tokenDelim[3] = " /";
	char lastCharInExp;

	if(pathExp == NULL) return FALSE;

	// Find how many '/' are inside the pathExp
	// This number will be compared later with tokensNum
	// To check special case of failure
	slashsNum = findOccurrenceTimes(pathExp, '/');

	// Copy the pathExp to the buffer, 
	// because strtok modifies content
	strncpy(buffer, pathExp, PATH_MAX);
	//printf("PathBuffer:[%s]\n", buffer);

	// if the expression starts with '/'
	if(*pathExp == '/') return FALSE;

	// find the last character before '\0'
	while(*pathExp != '\0'){
		lastCharInExp = *pathExp;
		++pathExp;
	}

	// if the expression ends with '/'
	if(lastCharInExp == '/') return FALSE;

	// separate pathExp to tokens and iterate them
	currentToken = strtok(buffer, tokenDelim);
	while(currentToken != NULL){
		//printf("Token:[%s]", currentToken);
		++tokensNum;

		// If the token is Parent,
		// the previous must also be a parent
		if(strcmp(currentToken, "..") == 0){
			isTValid = TRUE;
			// if previous token is not a parent
			if(!wasPrevTParent){
				isTValid = FALSE;
				isExpValid = FALSE;
			} 
		} else {
			isTValid = isFolderNameValid(currentToken);
			wasPrevTParent = FALSE;
		}

		//printf("\t\t\tTokenValidity: %d\n", isTValid);
		currentToken = strtok(NULL, tokenDelim);

		// If any token is invalid
		// the path is invalid
		if(!isTValid)
			isExpValid = FALSE;
	}

	// Checks whether the number of tokens is one more than the number or slashes
	// Example: data2/dat a3/data4
	// strtok will create 4 tokens, but slashes are only 2
	// This is a special invalid case when there is a blank char inside a dir name
	if(tokensNum != (slashsNum + 1)){
		//printf("TokensNum is a lot greater!\n");
		isExpValid = FALSE;
	}

	//printf("PathValidity=%d\n", isExpValid);
	return isExpValid;
}

// Checks whether a folder name is valid 
// starting with alpha and consisting of 
// alpha, digit or underscore
bool isFolderNameValid(const char *folderName){
	bool returnStatus = TRUE;

	if(folderName == NULL) return FALSE;
	if(strcmp(folderName, "..") == 0) return TRUE;

	// If the folderName does not start with letter
	if(!isAlphabet(*folderName)){
		fprintf(stderr, "\t\t\tLetter expected, but \"%c\" received!\n", *folderName);
		fprintf(stderr, "\t\t\tDirectory name must start with a letter!\n");
		return FALSE;
	}
	
	++folderName;
	while(*folderName != '\0'){
		if(isAlphabet(*folderName) || isDigit(*folderName) || (*folderName == '_')){
			++folderName;
			continue;
		} else {
			fprintf(stderr, "\t\t\tUnexpected \"%c\" char in the directory name!\n", *folderName);
			return FALSE;
		}
	}

	return TRUE;
}

// Checks whether the *path value is an 
// existing folder
// path must be in valid format
bool isFolderExisting(const char *path){
	struct stat fileStatus;
	int error = stat(path, &fileStatus);

	if(error == -1){
		if(errno == ENOENT){
			//fprintf(stdout, "Directory is not available <%s>\n", path);
		} else {
			fprintf(stderr, "Error type: <%s>\n", strerror(errno));
		}

		return FALSE;
	} 

	if(S_ISDIR(fileStatus.st_mode)) return TRUE;
	
	return FALSE;
}

// Helper called inside the makeCommand
// Do not call this function somewhere else
bool makeRecursive(const char *path, const int pathLength){
	//printf("make_command_start: ");
	//printf("[%s], len[%d]\n", path, pathLength);

	bool returnStatus;
	int error;
	int firstSlashIndex = firstOccurrenceIndex(path, '/');
	
	// Recursive base case - single folder name
	if(firstSlashIndex == -1){
		if(isFolderExisting(path)){
			fprintf(stdout, "  Directory <%s> exists!\n", path);
		} else {
			fprintf(stdout, "  Directory <%s> is missing!\n", path);
			error = mkdir(path);
			if(error == -1){
				fprintf(stderr, "    Error type: <%s>\n", strerror(errno));
				returnStatus = FALSE;
			} else {
				fprintf(stdout, "    Directory <%s> created!\n", path);
			}

			returnStatus = TRUE;
		}
	} 
	// Recursive deep case - path with folders
	else {
		int lengthOfRest = pathLength - firstSlashIndex - 1;

		//printf("index=%d, indexRest:%d\n", firstSlashIndex, lengthOfRest);

		char *firstPart = (char*) malloc((firstSlashIndex + 1) * sizeof(char));
		char *restPath = (char*) malloc((lengthOfRest + 1) * sizeof(char));

		strncpy(firstPart, path, firstSlashIndex);
		firstPart[firstSlashIndex] = '\0';
		strncpy(restPath, path+firstSlashIndex+1, lengthOfRest);
		restPath[lengthOfRest] = '\0';

		//printf("sizeFirst=%d, sizeRest:%d\n", strlen(firstPart), strlen(restPath));
		//printf("firstPart[%s]\n", firstPart);
		//printf("restPath[%s]\n", restPath);

		// If the folder exists, continue checking the rest 
		// of the path recursively, if not, create the folder
		if(isFolderExisting(firstPart)){
			fprintf(stdout, "  Directory <%s> exists!\n", firstPart);
			chdir(firstPart);
			returnStatus = makeRecursive(restPath, strlen(restPath));
		} else {
			fprintf(stdout, "  Directory <%s> is missing!\n", firstPart);
			int error = mkdir(firstPart);
			if(error == -1){
				fprintf(stderr, "    Error type: <%s>\n", strerror(errno));
				returnStatus = FALSE;
			} else {
				fprintf(stdout, "    Directory <%s> created!\n", firstPart);
				chdir(firstPart);
				returnStatus = makeRecursive(restPath, strlen(restPath));
			}
		}

		// Free the allocated space
		if(firstPart != NULL) 
			free(firstPart);
		if(restPath != NULL)
			free(restPath);

		chdir("..");
	}

	return returnStatus;
}

// Creates or completes a path
// path must be in valid format
bool makeCommand(const char *path, const int pathLength){
	bool returnStatus = TRUE;
	char cwd[PATH_MAX];

	if(path == NULL){
		fprintf(stderr, "path == NULL, inside makeCommand function!\n");
		return FALSE;		
	}

	fprintf(stdout, "Executing: \"make <%s>\"\n", path);

	// Check the whole path for existence
	if(isFolderExisting(path)){
		fprintf(stdout, "  Warning: <%s> already exists!\n", path);
		return FALSE;		
	}

	// Save the current working directory
	getcwd(cwd, sizeof(cwd));

	// Path does not exists or exists partially
	// Perform recursion
	returnStatus = makeRecursive(path, pathLength);

	// Restore the working directory
	chdir(cwd);

	return returnStatus;
}

// Changes the current working directory to path
// path must be in valid format
bool goCommand(const char *path){
	bool returnStatus = TRUE;
	int error;

	if(path == NULL){
		fprintf(stderr, "path == NULL, inside goCommand function!\n");
		return FALSE;		
	}

	fprintf(stdout, "Executing: \"go <%s>\"\n", path);

	error = chdir(path);

	if(error == -1){
		if(errno == ENOENT){
			fprintf(stderr, "  Error: No such directory: <%s>\n", path);
			returnStatus = FALSE;
		} else if(errno == ENOTDIR){
			fprintf(stderr, "  Error: Not a directory file: <%s>\n", path);
			returnStatus = FALSE;
		} else {
			fprintf(stderr, "  Other error type with errno: <%d>\n", errno);
			returnStatus = FALSE;
		}
	} else {
		fprintf(stdout, "  CWD changed to: <%s>\n", path);
		returnStatus = TRUE;
	}

	return returnStatus;
}

// path must be in valid format
void deleteCommandRecursive(const char *path){
	struct dirent *entry = NULL;
	DIR *dir = NULL;

	if(path == NULL){
		fprintf(stderr, "path == NULL, inside deleteCommand function!\n");
		return;		
	}

	dir = opendir(path);
	while(entry = readdir(dir))
	{   
		DIR *sub_dir = NULL;
		FILE *file = NULL;
		char* abs_path = malloc(256 * sizeof(char));
		if((*(entry->d_name) != '.') || ((strlen(entry->d_name) > 1) && (entry->d_name[1] != '.')))
		{
			sprintf(abs_path, "%s/%s", path, entry->d_name);
			if(sub_dir = opendir(abs_path))
			{
				closedir(sub_dir);
				deleteCommandRecursive(abs_path);
			}
			else
			{
				if(file = fopen(abs_path, "r"))
				{
					fclose(file);
					remove(abs_path);
				}
			}
		}
		free(abs_path);   
	}
	remove(path);
}

void deleteCommand(const char *path){
	if(path == NULL){
		fprintf(stderr, "path == NULL, inside deleteCommand function!\n");
		return;
	}

	fprintf(stdout, "Executing: \"delete <%s>\"\n", path);
	deleteCommandRecursive(path);
}

bool ifCommand(const char *pathControl, const char *command, const char *pathCommand){
	bool status;

	fprintf(stdout, "Checking: \"if <%s>\"\n", pathControl);

	status = isFolderExisting(pathControl);

	if(status){
		fprintf(stdout, "  if returned TRUE for <%s>\n", pathControl);
		if(strcmp(command, "make") == 0){
			makeCommand(pathCommand, strlen(pathCommand));
		} else if(strcmp(command, "go") == 0){
			goCommand(pathCommand);
		} else if(strcmp(command, "delete") == 0){
			deleteCommand(pathCommand);
		} else {
			fprintf(stderr, "UNEXPECTED_ERROR_INSIDE_IF\n");
			fprintf(stderr, "pathControl[%s]_command[%s]_pathCommand[%s]", pathControl, command, pathCommand);		
		}
	} else {
		fprintf(stdout, "  if returned FALSE for <%s>\n", pathControl);
	}
	
	return status;
}

bool ifnotCommand(const char *pathControl, const char *command, const char *pathCommand){
	bool status;

	fprintf(stdout, "Checking: \"ifnot <%s>\"\n", pathControl);

	status = isFolderExisting(pathControl);

	if(!status){
		fprintf(stdout, "  ifnot returned TRUE for <%s>\n", pathControl);
		if(strcmp(command, "make") == 0){
			makeCommand(pathCommand, strlen(pathCommand));
		} else if(strcmp(command, "go") == 0){
			goCommand(pathCommand);
		} else if(strcmp(command, "delete") == 0){
			deleteCommand(pathCommand);
		} else {
			fprintf(stderr, "UNEXPECTED_ERROR_INSIDE_IFNOT\n");
			fprintf(stderr, "pathControl[%s]_command[%s]_pathCommand[%s]", pathControl, command, pathCommand);		
		}
	} else {
		fprintf(stdout, "  ifnot returned FALSE for <%s>\n", pathControl);
	}
	
	return !status;	
}

// Main Functions
void printCWD(){
	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));
	printf("~~~~~CWD: <%s>\n", cwd);
}

// Only a testing function
void test_isValidPathExpression(){
	char t1[PATH_MAX] = "data2";
	char t2[PATH_MAX] = "data2/data3/data4";
	char t3[PATH_MAX] = "/data2/data3/data4";
	char t4[PATH_MAX] = "data2/data3/data4/";
	char t5[PATH_MAX] = "data2   /data3  /data4";
	char t6[PATH_MAX] = " .. / ../data4";
	char t7[PATH_MAX] = "data2/../data4";
	char t8[PATH_MAX] = "data2/ da?ta3  /data4";
	char t9[PATH_MAX] = "data2  /  data3  /data4";
	char t10[PATH_MAX] = "data2/  dat a3  /data4";

	printf("PathBuffer1:[%s]\n", t1);
	printf("PathValidity1=%d\n", isValidPathExpression(t1));

	printf("PathBuffer2:[%s]\n", t2);
	printf("PathValidity2=%d\n", isValidPathExpression(t2));

	printf("PathBuffer3:[%s]\n", t3);
	printf("PathValidity3=%d\n", isValidPathExpression(t3));

	printf("PathBuffer4:[%s]\n", t4);
	printf("PathValidity4=%d\n", isValidPathExpression(t4));

	printf("PathBuffer5:[%s]\n", t5);
	printf("PathValidity5=%d\n", isValidPathExpression(t5));

	printf("PathBuffer6:[%s]\n", t6);
	printf("PathValidity6=%d\n", isValidPathExpression(t6));

	printf("PathBuffer7:[%s]\n", t7);
	printf("PathValidity7=%d\n", isValidPathExpression(t7));

	printf("PathBuffer8:[%s]\n", t8);
	printf("PathValidity8=%d\n", isValidPathExpression(t8));

	printf("PathBuffer9:[%s]\n", t9);
	printf("PathValidity9=%d\n", isValidPathExpression(t9));

	printf("PathBuffer9:[%s]\n", t10);
	printf("PathValidity9=%d\n", isValidPathExpression(t10));
}

// Prints all tokens
void printTokens(char **tokens, int numOfTokens){
	int i;

	for(i=0; i<numOfTokens; ++i){
		fprintf(stdout, "[%s]", tokens[i]);
		if(strcmp(tokens[i], ";") == 0 || strcmp(tokens[i], "{") == 0 || strcmp(tokens[i], "}") == 0)
			fprintf(stdout, "\n");
	}

	fprintf(stdout, "\n");
}

// Frees the memory locations of file buffer and tokens
void freeAllMem(char *buffer, char **tokens, int tokensNum){
	int i;

	if(buffer != NULL)
		free(buffer);

	if(tokens != NULL){
		for(i=0; i<tokensNum; ++i){
			if(tokens[i] != NULL)
				free(tokens[i]);
		}
		free(tokens);
	}
}