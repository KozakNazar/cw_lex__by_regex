#define _CRT_SECURE_NO_WARNINGS  // for using sscanf in VS
/************************************************************
* N.Kozak // Lviv'2024 // example lexical analysis by regex *
*                         file: cwlex.cpp                   *
*                                                           *
*************************************************************/
//#define USE_PREDEFINED_PARAMETERS // enable this define for use predefined value

#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include <fstream>
#include <iostream>
//#include <algorithm>
#include <iterator>
#include <regex>

#define SUCCESS_STATE 0

#define MAX_TEXT_SIZE 8192
#define MAX_WORD_COUNT (MAX_TEXT_SIZE / 5)
#define MAX_LEXEM_SIZE 1024
#define MAX_VARIABLES_COUNT 256
#define MAX_KEYWORD_COUNT 64

#define KEYWORD_LEXEME_TYPE 1
#define IDENTIFIER_LEXEME_TYPE 2
#define VALUE_LEXEME_TYPE 4
#define UNEXPEXTED_LEXEME_TYPE 127

#define LEXICAL_ANALISIS_MODE 1
#define SEMANTIC_ANALISIS_MODE 2
#define FULL_COMPILER_MODE 4

#define DEBUG_MODE 512

#define MAX_PARAMETERS_SIZE 4096
#define PARAMETERS_COUNT 4
#define INPUT_FILENAME_PARAMETER 0

#define DEFAULT_MODE (LEXICAL_ANALISIS_MODE | DEBUG_MODE)

#define DEFAULT_INPUT_FILENAME "file1.cwl"

#define PREDEFINED_TEXT \
	"name MN\r\n" \
	"data\r\n" \
	"    #*argumentValue*#\r\n" \
	"    long int AV\r\n" \
	"    #*resultValue*#\r\n" \
	"    long int RV\r\n" \
	";\r\n" \
	"\r\n" \
	"body\r\n" \
	"    RV << 1; #*resultValue = 1; *#\r\n" \
	"\r\n" \
	"    #*input*#\r\n" \
	"	 get AV; #*scanf(\"%d\", &argumentValue); *#\r\n" \
	"\r\n" \
	"    #*compute*#\r\n" \
	"	 CL: #*label for cycle*#\r\n" \
	"    if AV == 0 goto EL #*for (; argumentValue; --argumentValue)*#\r\n" \
	"        RV << RV ** AV; #*resultValue *= argumentValue; *#\r\n" \
	"        AV << AV -- 1; \r\n" \
	"    goto CL\r\n" \
	"    EL: #*label for end cycle*#\r\n" \
	"\r\n" \
	"    #*output*#\r\n" \
	"    put RV; #*printf(\"%d\", resultValue); *#\r\n" \
	"end" \

unsigned int mode;
char parameters[PARAMETERS_COUNT][MAX_PARAMETERS_SIZE] = { "" };

struct LexemInfo{
	char lexemStr[MAX_LEXEM_SIZE];
	unsigned int lexemId;
	unsigned int tokenType;
	unsigned int ifvalue;
	unsigned int row;
	unsigned int col;
	// TODO: ...
};

struct LexemInfo lexemesInfoTable[MAX_WORD_COUNT] = { { "", 0, 0, 0 } };
struct LexemInfo * lastLexemInfoInTable = lexemesInfoTable; // first for begin

char identifierIdsTable[MAX_WORD_COUNT][MAX_LEXEM_SIZE] = { "" };

void printLexemes(struct LexemInfo * lexemInfoTable, char printBadLexeme = 0){
	if (printBadLexeme){
		printf("Bad lexeme:\r\n");
	}
	else{
		printf("Lexemes table:\r\n");
	}
	printf("-------------------------------------------------------------------\r\n");
	printf("index\t\tlexeme\t\tid\ttype\tifvalue\trow\tcol\r\n");
	printf("-------------------------------------------------------------------\r\n");
	for (unsigned int index = 0; (!index || !printBadLexeme) && lexemInfoTable[index].lexemStr[0] != '\0'; ++index){
		printf("%5d%17s%12d%10d%11d%4d%8d\r\n", index, lexemInfoTable[index].lexemStr, lexemInfoTable[index].lexemId, lexemInfoTable[index].tokenType, lexemInfoTable[index].ifvalue, lexemInfoTable[index].row, lexemInfoTable[index].col);
	}
	printf("-------------------------------------------------------------------\r\n\r\n");

	return;
}

// get identifier id
unsigned int getIdentifierId(char(*identifierIdsTable)[MAX_LEXEM_SIZE], char * str){
	unsigned int index = 0;
	for (; identifierIdsTable[index][0] != '\0'; ++index){
		if (!strncmp(identifierIdsTable[index], str, MAX_LEXEM_SIZE)){
			return index;
		}
	}
	strncpy(identifierIdsTable[index], str, MAX_LEXEM_SIZE);
	identifierIdsTable[index + 1][0] = '\0'; // not necessarily for zero-init identifierIdsTable
	return index;
}

// try to get identifier
unsigned int tryToGetIdentifier(struct LexemInfo* lexemInfoInTable, char(*identifierIdsTable)[MAX_LEXEM_SIZE]) {
	char identifiers_re[] = "[A-Z][A-Z]";

	if (std::regex_match(std::string(lexemInfoInTable->lexemStr), std::regex(identifiers_re))) {
		lexemInfoInTable->lexemId = getIdentifierId(identifierIdsTable, lexemInfoInTable->lexemStr);
		lexemInfoInTable->tokenType = IDENTIFIER_LEXEME_TYPE;
		return SUCCESS_STATE;
	}

	return ~SUCCESS_STATE;
}

// try to get value
unsigned int tryToGetUnsignedValue(struct LexemInfo* lexemInfoInTable) {
	char unsignedvalues_re[] = "0|[1-9][0-9]*";

	if (std::regex_match(std::string(lexemInfoInTable->lexemStr), std::regex(unsignedvalues_re))) {
		lexemInfoInTable->ifvalue = atoi(lastLexemInfoInTable->lexemStr);
		lexemInfoInTable->lexemId = MAX_VARIABLES_COUNT + MAX_KEYWORD_COUNT;
		lexemInfoInTable->tokenType = VALUE_LEXEME_TYPE;
		return SUCCESS_STATE;
	}

	return ~SUCCESS_STATE;
}

int commentRemover(char * text = (char*)"", const char * openStrSpc = "//", const char * closeStrSpc = "\r\n"){
	bool eofAlternativeCloseStrSpcType = false;
	bool explicitCloseStrSpc = true;
	if (!strcmp(closeStrSpc, "\r\n")) {
		eofAlternativeCloseStrSpcType = true;
		explicitCloseStrSpc = false;
	}
	
	unsigned int commentSpace = 0;

	unsigned int textLength = strlen(text);               // strnlen(text, MAX_TEXT_SIZE);
	unsigned int openStrSpcLength = strlen(openStrSpc);   // strnlen(openStrSpc, MAX_TEXT_SIZE);
	unsigned int closeStrSpcLength = strlen(closeStrSpc); // strnlen(closeStrSpc, MAX_TEXT_SIZE);
	if (!closeStrSpcLength){
		return -1; // no set closeStrSpc
	}
	unsigned char oneLevelComment = 0;
	if (!strncmp(openStrSpc, closeStrSpc, MAX_LEXEM_SIZE)){
		oneLevelComment = 1;
	}

	for (unsigned int index = 0; index < textLength; ++index){
		if (!strncmp(text + index, closeStrSpc, closeStrSpcLength) && (explicitCloseStrSpc || commentSpace)) {
			if (commentSpace == 1 && explicitCloseStrSpc){
				for (unsigned int index2 = 0; index2 < closeStrSpcLength; ++index2){
					text[index + index2] = ' ';
				}
			}
			else if (commentSpace == 1 && !explicitCloseStrSpc){
				index += closeStrSpcLength - 1;
			}
			oneLevelComment ? commentSpace = !commentSpace : commentSpace = 0;
		}
		else if (!strncmp(text + index, openStrSpc, openStrSpcLength)) {
			oneLevelComment ? commentSpace = !commentSpace : commentSpace = 1;
		}

		if (commentSpace && text[index] != ' ' && text[index] != '\t' && text[index] != '\r' && text[index] != '\n'){
			text[index] = ' ';
		}

	}

	if (commentSpace && !eofAlternativeCloseStrSpcType){
		return -1;
	}

	return 0;
}

void comandLineParser(int argc, char* argv[], unsigned int * mode, char (* parameters)[MAX_PARAMETERS_SIZE]){
	char useDefaultModes = 1;
	*mode = 0;
	for (int index = 1; index < argc; ++index){
		if (!strcmp(argv[index], "-lex")){
			*mode |= LEXICAL_ANALISIS_MODE;
			useDefaultModes = 0;
			continue;
		}
		else if (!strcmp(argv[index], "-d")){
			*mode |= DEBUG_MODE;
			useDefaultModes = 0;
			continue;
		}
		
		// other keys
		// TODO:...

		// input file name
		strncpy(parameters[INPUT_FILENAME_PARAMETER], argv[index], MAX_PARAMETERS_SIZE);
	}

	// default input file name,  if not entered manually
	if (parameters[INPUT_FILENAME_PARAMETER][0] == '\0'){
		strcpy(parameters[INPUT_FILENAME_PARAMETER], DEFAULT_INPUT_FILENAME);
		printf("Input file name not setted. Used default input file name \"file1.cwl\"\r\n\r\n");
	}

	// default mode,  if not entered manually
	if (useDefaultModes){
		*mode = DEFAULT_MODE;
		printf("Used default mode\r\n\r\n");
	}

	return;
}

// after using this function use free(void *) function to release text buffer
size_t loadSource(char ** text, char * fileName){
	if (!fileName){
		printf("No input file name\r\n");
		return 0;
	}

	FILE * file = fopen(fileName, "rb");

	if (file == NULL){
		printf("File not loaded\r\n");
		return 0;
	}

	fseek(file, 0, SEEK_END);
	long fileSize_ = ftell(file);
	if (fileSize_ >= MAX_TEXT_SIZE) {
		printf("the file(%ld bytes) is larger than %d bytes\r\n", fileSize_, MAX_TEXT_SIZE);
		fclose(file);
		exit(2); // TODO: ...
		//return 0;
	}
	size_t fileSize = fileSize_;
	rewind(file);

	if (!text) {
		printf("Load source error\r\n");
		return 0;
	}
	*text = (char*)malloc(sizeof(char) * (fileSize + 1));
	if (*text == NULL) { 
		fputs("Memory error", stderr); 
		fclose(file);
		exit(2); // TODO: ...
		//return 0;
	}

	size_t result = fread(*text, sizeof(char), fileSize, file);
	if (result != fileSize) {
		fputs("Reading error", stderr);
		fclose(file);
		exit(3); // TODO: ...
		//return 0;
	}
	(*text)[fileSize] = '\0';

	fclose(file);
	
	return fileSize;
}

// try to get KeyWord
char tryToGetKeyWord(struct LexemInfo* lexemInfoInTable) {
	char keywords_re[] = ";|<<|\\+\\+|--|\\*\\*|==|!=|:|name|data|body|end|get|put|if|goto|div|mod|le|ge|not|and|or|long|int";
	char keywords_[] =           ";|<<|++|--|**|==|!=|:|name|data|body|end|get|put|if|goto|div|mod|le|ge|not|and|or|long|int";
		
	if (std::regex_match(std::string(lexemInfoInTable->lexemStr), std::regex(keywords_re))){
		lexemInfoInTable->lexemId = MAX_VARIABLES_COUNT +
		strstr(keywords_, lexemInfoInTable->lexemStr) - keywords_;
		lexemInfoInTable->tokenType = KEYWORD_LEXEME_TYPE;
		return SUCCESS_STATE;
	}

	return ~SUCCESS_STATE;
}

void setPositions(const char* text, struct LexemInfo* lexemInfoTable) {
	int line_number = 1;
	const char *pos = text, *line_start = text;

	if (lexemInfoTable) while (*pos != '\0' && lexemInfoTable->lexemStr[0] != '\0') {
		const char* line_end = strchr(pos, '\n');
		if (!line_end) {
			line_end = text + strlen(text);
		}

		char line_[4096], * line = line_; //!! TODO: ...
		strncpy(line, pos, line_end - pos);
		line[line_end - pos] = '\0';

		for (char* found_pos; lexemInfoTable->lexemStr[0] != '\0' && (found_pos = strstr(line, lexemInfoTable->lexemStr)); line += strlen(lexemInfoTable->lexemStr), ++lexemInfoTable) {			
			lexemInfoTable->row = line_number;
			lexemInfoTable->col = found_pos - line_ + 1;
		}
		line_number++;
		pos = line_end;
		if (*pos == '\n') {
			pos++;
		}
	}
}

struct LexemInfo lexicalAnalyze(struct LexemInfo* lexemInfoInPtr, char(*identifierIdsTable)[MAX_LEXEM_SIZE]) {
	struct LexemInfo ifBadLexemeInfo = { 0 };

	if (tryToGetKeyWord(lexemInfoInPtr) == SUCCESS_STATE);
	else if (tryToGetIdentifier(lexemInfoInPtr, identifierIdsTable) == SUCCESS_STATE);
	else if (tryToGetUnsignedValue(lexemInfoInPtr) == SUCCESS_STATE);
	else {
		ifBadLexemeInfo.tokenType = UNEXPEXTED_LEXEME_TYPE;
	}

	return ifBadLexemeInfo;
}

struct LexemInfo tokenize(char* text, struct LexemInfo** lastLexemInfoInTable, char(*identifierIdsTable)[MAX_LEXEM_SIZE], struct LexemInfo(*lexicalAnalyzeFunctionPtr)(struct LexemInfo*, char(*)[MAX_LEXEM_SIZE])) {
	char tokens_re[] = ";|<<|\\+\\+|--|\\*\\*|==|!=|:|[_0-9A-Za-z]+|[^ \t\n\r\f\v]";
	std::regex tokens_re_(tokens_re);
	struct LexemInfo ifBadLexemeInfo = { 0 };
	std::string stringText(text);

	for (std::sregex_token_iterator end, tokenIterator(stringText.begin(), stringText.end(), tokens_re_); tokenIterator != end; ++tokenIterator, ++(*lastLexemInfoInTable)) {
		std::string str = *tokenIterator;
		strncpy((*lastLexemInfoInTable)->lexemStr, str.c_str(), MAX_LEXEM_SIZE);
		if ((ifBadLexemeInfo = (*lexicalAnalyzeFunctionPtr)(*lastLexemInfoInTable, identifierIdsTable)).tokenType == UNEXPEXTED_LEXEME_TYPE) {
			break;
		}
	}

	setPositions(text, lexemesInfoTable);
			
	if (ifBadLexemeInfo.tokenType == UNEXPEXTED_LEXEME_TYPE) {
		strncpy(ifBadLexemeInfo.lexemStr, (*lastLexemInfoInTable)->lexemStr, MAX_LEXEM_SIZE);
		ifBadLexemeInfo.row = (*lastLexemInfoInTable)->row;
		ifBadLexemeInfo.col = (*lastLexemInfoInTable)->col;			
	}
			
	return ifBadLexemeInfo;
}

int main(int argc, char* argv[]) {

#ifdef	USE_PREDEFINED_PARAMETERS
	mode = DEFAULT_MODE;
	char text[MAX_TEXT_SIZE] = PREDEFINED_TEXT;
#else
	comandLineParser(argc, argv, &mode, parameters);
	char* text;
	size_t sourceSize = loadSource(&text, parameters[INPUT_FILENAME_PARAMETER]);
	if (!sourceSize) {
		printf("Press Enter to exit . . .");
		(void)getchar();
		return 0;
	}
#endif

	if (!(mode & LEXICAL_ANALISIS_MODE)) {
		printf("NO SUPORTED MODE ...\r\n");
		printf("Press Enter to exit . . .");
		(void)getchar();
		return 0;
	}

	if (mode & DEBUG_MODE) {
		printf("Original source:\r\n");
		printf("-------------------------------------------------------------------\r\n");
		printf("%s\r\n", text);
		printf("-------------------------------------------------------------------\r\n\r\n");
	}

	int commentRemoverResult = commentRemover(text, "#*", "*#");
	if (commentRemoverResult) {
		printf("Comment remover return %d\r\n", commentRemoverResult);
		printf("Press Enter to exit . . .");
		(void)getchar();
		return 0;
	}
	if (mode & DEBUG_MODE) {
		printf("Source after comment removing:\r\n");
		printf("-------------------------------------------------------------------\r\n");
		printf("%s\r\n", text);
		printf("-------------------------------------------------------------------\r\n\r\n");
	}

	struct LexemInfo ifBadLexemeInfo = tokenize(text, &lastLexemInfoInTable, identifierIdsTable, lexicalAnalyze);

	if (ifBadLexemeInfo.tokenType == UNEXPEXTED_LEXEME_TYPE) {
		UNEXPEXTED_LEXEME_TYPE;
		ifBadLexemeInfo.tokenType;
		printf("Lexical analysis detected unexpected lexeme\r\n");
		printLexemes(&ifBadLexemeInfo, 1);
		printf("Press Enter to exit . . .");
		(void)getchar();
		return 0;
	}
	if (mode & DEBUG_MODE) {
		printLexemes(lexemesInfoTable);
	}
	else {
		printf("Lexical analysis complete success\r\n");
	}

	printf("Press Enter to exit . . .");
	(void)getchar();

#ifndef	USE_PREDEFINED_PARAMETERS
	free(text);
#endif

	return 0;
}
