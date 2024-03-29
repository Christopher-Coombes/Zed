#include "compiler.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Tokenizer Helper Functions

bool compiler::isIdChar(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_';
}

bool compiler::isSymbolChar(char c) {
	for (const char& symb : symbolChars) {
		if (symb == c) return true;
	}
	return false;
}

bool compiler::isTypeToken(TokenType type) {
	return type == TokenType::TYPE_BOOL
		|| type == TokenType::TYPE_CHAR
		|| type == TokenType::TYPE_FLOAT
		|| type == TokenType::TYPE_INT;
}

int parseInt(char str[], int strlen, int base) {
	int out = 0;
	for (int i = 0; i < strlen; i++) {
		out *= base;
		if ('0' <= str[i] && str[i] <= '9') {
			out += str[i] - '0';
		} else if ('A' <= str[i] && str[i] <= 'Z') {
			out += str[i] - 'A' + 10;
		} else if ('a' <= str[i] && str[i] <= 'z') {
			out += str[i] - 'a' + 10;
		}
	}
	return out;
}

float parseFloat(char str[], int strlen, float base) {
	float out = 0;
	int i = 0;
	while (i < strlen && str[i] != '.') {
		out *= base;
		if ('0' <= str[i] && str[i] <= '9') {
			out += str[i] - '0';
		} else if ('A' <= str[i] && str[i] <= 'Z') {
			out += str[i] - 'A' + 10;
		} else if ('a' <= str[i] && str[i] <= 'z') {
			out += str[i] - 'a' + 10;
		}
		i++;
	}
	i++;
	float place = 1;
	while (i < strlen) {
		place /= base;
		if ('0' <= str[i] && str[i] <= '9') {
			out += (str[i] - '0') * place;
		} else if ('A' <= str[i] && str[i] <= 'Z') {
			out += (str[i] - 'A' + 10) * place;
		} else if ('a' <= str[i] && str[i] <= 'z') {
			out += (str[i] - 'a' + 10) * place;
		}
		i++;
	}
	return out;
}

void putSymbols(const char* str, int slen, int line, int column, compiler::TokenData& outputData) {
	using namespace compiler;
	if (slen == 0) return;
	if (slen == 1) {
		for (int i = 0; i < symbolCount; i++) {
			if (symbolChars[i] == str[0]) {
				outputData.putType(static_cast<TokenType>(i + firstSymbol), line, column);
				break;
			}
		}
		return;
	}

	int currlen = slen > maxMultiSymbolLen ? maxMultiSymbolLen : slen;
	int i = 0, j;
	while (static_cast<int>(strlen(multiSymbolStrings[i])) > currlen && i < multiSymbolCount) i++;
	while (i < multiSymbolCount) {
		currlen = strlen(multiSymbolStrings[i]);
		j = 0;
		while (true) {
			if (multiSymbolStrings[i][j] != str[j]) break;
			if (j >= currlen - 1) {
				// PUT symbol
				outputData.putType(static_cast<TokenType>(i + firstMultiSymbol), line, column);
				putSymbols(str + currlen, slen - currlen, line, column + currlen, outputData);
				return;
			}
			j++;
		}
		i++;
	}
	putSymbols(str, 1, line, column, outputData);
	putSymbols(str + 1, slen - 1, line, column + 1, outputData);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TokenData

compiler::Token* compiler::TokenData::put(TokenType type, int line, int column) {
	tokens.emplace_back(type, line, column);
	return &tokens.back();
}
void compiler::TokenData::putInt(int val, int line, int column) {
	put(TokenType::NUM_INT, line, column)->int_ = val;
}
void compiler::TokenData::putFloat(float val, int line, int column) {
	put(TokenType::NUM_FLOAT, line, column)->float_ = val;
}
void compiler::TokenData::putChar(char val, int line, int column) {
	put(TokenType::CHAR, line, column)->char_ = val;
}
void compiler::TokenData::putType(TokenType type, int line, int column) {
	tokens.emplace_back(type, line, column);
}
void compiler::TokenData::putStr(TokenType type, int line, int column, std::string str) {
	int index = -1;
	for (size_t i = 0; i < strList.size(); i++) {
		if (strList[i] == str) {
			index = i;
			break;
		}
	}

	if (index == -1) {
		index = strList.size();
		strList.push_back(str);
	}

	put(type, line, column)->strIndex = index;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Tokenizer Functions

int compiler::tokenize(std::iostream& inputFile, TokenData& outputData, CompilerSettings& settings, std::ostream& stream) {
	// Go to the start of the file
	inputFile.clear();
	inputFile.seekg(0, std::ios::beg);

	// Flags/settings
	const bool isDebug = settings.flags.hasFlags(Flags::FLAG_DEBUG);

	/*
	Keep track of current type:
	NONE, STRING, CHAR, NUMBER, IDENTIFIER, SYMBOLS, COMMENT, BLOCK_COMMENT

	For none:
	" starts string
	// or /* starts comment
	' starts char
	0-9 starts number
	a-z, A-Z (and what else? _?) starts identifier
	symbol starts symbols

	For string:
	do escaping
	" ends string

	For comment
	Newline ends //
	* / ends /*

	For number:
	Know if there's been a .
	Check base patterns (0x, 0b)
	End on non-number

	For identifier:
	a-z, A-Z, 0-9 (and what else?) can be in an identifier
	End on non-identifier char

	For symbols:
	End on non-symbol char
	Helper function to push symbols, greedily take largest
	Ex:
	+===       +===      +===            ==
	^^^^ (no)  ^^^ (no)  ^^ (yes, push)  ^^ (yes, push)


	*/

	// Keeping track of chars
	char c;
	char currGroup[MAX_TOKEN_STR_LEN];
	int currGroupLen = 0;
	enum class GroupType {
		NONE,
		SKIP,
		STRING,
		CHAR,
		NUMBER,
		SYMBOL,
		ID,
		COMMENT,
		BLOCK_COMMENT
	};
	GroupType currGroupType = GroupType::NONE;

	// Specific to certain types
	bool isEscaped = false;
	bool hasDecimal = false;
	int numBase = 10;
	int keyword;

	// Line and column
	int line = 0;
	int column = 0;
	// Starting line and column for the current token
	int startLine = 0;
	int startColumn = 0;
	// end
	bool end = inputFile.eof();

	// In each loop, it does something based on the current type and the current char
	// If it processes the char and retains its type, the char is simply appended to currGroup
	// If it processes the char and sets type to GroupType::NONE, it means it doesn't want the char
	// and the char will be used to start a new token
	// if it processes the char and sets type to GroupType::SKIP, it means it doesn't want the char
	// but also not to use the char to start a new token
	while (!end) {
		inputFile.get(c);
		end = inputFile.eof();
		if (c == '\n') {
			line++;
			column = 0;
		} else {
			column++;
		}

		switch (currGroupType) {
			case GroupType::NONE:
				break;

			case GroupType::STRING:
				// Match certain escape sequences
				if (end) {
					throw CompilerException(CompilerException::ErrorType::UNCLOSED_STRING, startLine, startColumn);
				} else if (isEscaped) {
					if (c == 'n') {
						c = '\n';
						// line--;
					} else if (c == 'c') {
						c = '\033';
					} else if (c == '0') {
						c = '\0';
					} else if (c == 't') {
						c = '\t';
					}
					// For anything else the escape doesn't change anything
					// That means for example \" will become " but it won't terminate the string

					isEscaped = false;
				} else if (c == '"') {
					// PUT string
					currGroup[currGroupLen] = '\0';
					outputData.putStr(TokenType::STRING, startLine, startColumn, std::string(currGroup));
					currGroupType = GroupType::SKIP;
				} else {
					if (c == '\\') {
						isEscaped = true;
						// Don't add this char to the group
						continue;
					}
					// Other stuff specific to string management?


				}
				break;

			case GroupType::CHAR:
				// Match certain escape sequences
				if (end) {
					throw CompilerException(CompilerException::ErrorType::UNCLOSED_CHAR, startLine, startColumn);
				} if (isEscaped) {
					if (c == 'n') {
						c = '\n';
						// line--;
					} else if (c == 'c') {
						c = '\033';
					} else if (c == '0') {
						c = '\0';
					} else if (c == 't') {
						c = '\t';
					}
					// For anything else the escape doesn't change anything
					// That means for example \" will become " but it won't terminate the string

					isEscaped = false;
				} else if (c == '\'') {
					if (currGroupLen != 1) {
						throw CompilerException(CompilerException::ErrorType::INVALID_CHAR, startLine, startColumn);
					}
					outputData.putChar(currGroup[0], startLine, startColumn);
					currGroupType = GroupType::SKIP;
				} else {
					if (currGroupLen != 0) {
						throw CompilerException(CompilerException::ErrorType::INVALID_CHAR, startLine, startColumn);
					}
					if (c == '\\') {
						isEscaped = true;
						// Don't add this char to the group
						continue;
					}
				}
				break;

			case GroupType::NUMBER:
				if (currGroupLen == 1 && currGroup[0] == '0' && (c == 'x' || c == 'b')) {
					if (c == 'x') {
						numBase = 16;
					} else if (c == 'b') {
						numBase = 2;
					}
					// Don't add this char to the group
					continue;
				} else if (!hasDecimal && c == '.') {
					hasDecimal = true;
				} else if (!end && (('0' <= c && c <= '9') ||
									('A' <= c && c < ('A' + numBase - 10)) ||
									('a' <= c && c < ('a' + numBase - 10)))) {
					// All good
				} else {
					// PUT number
					if (!hasDecimal) {
						outputData.putInt(parseInt(currGroup, currGroupLen, numBase), startLine, startColumn);
					} else {
						outputData.putFloat(parseFloat(currGroup, currGroupLen, static_cast<float>(numBase)), startLine, startColumn);
					}
					currGroup[currGroupLen] = '\0';
					currGroupType = GroupType::NONE;
				}
				break;

			case GroupType::SYMBOL:
				if (!end && currGroupLen >= 1) {
					if (currGroup[currGroupLen - 1] == '/' && c == '/') {
						currGroupType = GroupType::COMMENT;
						currGroupLen--;
					} else if (currGroup[currGroupLen - 1] == '/' && c == '*') {
						currGroupType = GroupType::BLOCK_COMMENT;
						currGroupLen--;
					}
				}
				if (end || currGroupType != GroupType::SYMBOL || !isSymbolChar(c)) {
					// PUT symbols (greedy thing)
					currGroup[currGroupLen] = '\0';
					putSymbols(currGroup, currGroupLen, startLine, startColumn, outputData);
					currGroupType = currGroupType == GroupType::SYMBOL ? GroupType::NONE : currGroupType;
				}
				break;

			case GroupType::ID:
				if (end || !isIdChar(c)) {
					currGroup[currGroupLen] = '\0';
					keyword = lookupString(currGroup, keywordStrings, keywordCount);
					if (keyword == -1) {
						outputData.putStr(TokenType::IDENTIFIER, startLine, startColumn, std::string(currGroup));
					} else {
						outputData.putType(static_cast<TokenType>(keyword + firstKeyword), startLine, startColumn);
					}
					currGroupType = GroupType::NONE;
				}
				break;

			case GroupType::COMMENT:
				if (end || c == '\n') {
					// Don't PUT comment
					currGroupType = GroupType::SKIP;
				}
				break;

			case GroupType::BLOCK_COMMENT:
				if (end || (currGroupLen > 0 && currGroup[0] == '*' && c == '/')) {
					// Don't PUT comment
					currGroupType = GroupType::SKIP;
				}
				break;
		}

		if (end) break;
		if (currGroupType == GroupType::NONE) {
			// Start a new token
			currGroupLen = 1;
			currGroup[0] = c;
			startLine = line;
			startColumn = column;
			if (c == '"') {
				currGroupLen = 0;
				currGroupType = GroupType::STRING;
				isEscaped = false;
			} else if (c == '\'') {
				currGroupLen = 0;
				currGroupType = GroupType::CHAR;
				isEscaped = false;
			} else if ('0' <= c && c <= '9') {
				currGroupType = GroupType::NUMBER;
				hasDecimal = false;
				numBase = 10;
			} else if (isIdChar(c)) {
				currGroupType = GroupType::ID;
			} else if (isSymbolChar(c)) {
				currGroupType = GroupType::SYMBOL;
			}
			// Anything else, like whitespace, is ignored and GroupType::NONE persists on the next loop iteration
		} else if (currGroupType == GroupType::SKIP) {
			// Start a new token on the NEXT iteration
			currGroupType = GroupType::NONE;
			currGroupLen = 0;
		} else {
			// Add current char to the current group
			if (currGroupLen >= MAX_TOKEN_STR_LEN) {
				throw CompilerException(CompilerException::ErrorType::STRING_TOO_LONG, startLine, startColumn);
			}
			// For comments we only care about the single previous character (to check for the ending sequence)
			if (currGroupType == GroupType::COMMENT || currGroupType == GroupType::BLOCK_COMMENT) {
				currGroupLen = 0;
			}
			currGroup[currGroupLen] = c;
			currGroupLen++;
		}
	}

	return 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Printing Functions

void compiler::printTokens(TokenData& tokenData, std::ostream& stream) {
	int line = 0;
	stream << IO_NORM;
	for (Token& t : tokenData.tokens) {
		if (line != t.line) stream << '\n';
		line = t.line;
		printToken(t, tokenData, stream);
		stream << ' ';
	}
	stream << '\n';
}

void compiler::printToken(Token& t, TokenData& tokenData, std::ostream& stream) {
	int type = static_cast<int>(t.type);

	if (t.type == TokenType::STRING) {
		stream << IO_FMT_STRING(tokenData.strList[t.strIndex]);
	} else if (t.type == TokenType::IDENTIFIER) {
		stream << IO_FMT_ID(tokenData.strList[t.strIndex]);
	} else if (t.type == TokenType::NUM_INT) {
		stream << IO_FMT_INT(t.int_);
	} else if (t.type == TokenType::NUM_FLOAT) {
		stream << IO_FMT_FLOAT(t.float_);
	} else if (t.type == TokenType::CHAR) {
		stream << IO_FMT_CHAR(t.char_);
	} else if (type >= firstSymbol && type - firstSymbol < symbolCount) {
		stream << IO_FMT_SYMB(symbolChars[type - firstSymbol]);
	} else if (type >= firstMultiSymbol && type - firstMultiSymbol < multiSymbolCount) {
		stream << IO_FMT_MULTISYMB(multiSymbolStrings[type - firstMultiSymbol]);
	} else if (type >= firstKeyword && type - firstKeyword < keywordCount) {
		stream << IO_FMT_KEYWORD(keywordStrings[type - firstKeyword]);
	}
	
}