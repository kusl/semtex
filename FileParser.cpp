#include "FileParser.hpp"

#include <cstring>
#include <fstream>
#include <regex>
#include <sys/stat.h>
#include <sstream>

#include "Exceptions.hpp"
#include "Context.hpp"

struct ReplacementMapping {
 	std::string key; //!< String to trigger a replacement
	ReplacementGenerator* gen; //!< Function pointer to a function that will do the replacement

	// Used for sorting mappings from shortest to longest
	bool operator< (const ReplacementMapping& other) { return key.length() < other.key.length(); }
};

static const size_t kInputLen = strlen("\\input"); //!< Length of "\input"
static const size_t kIncludeLen = strlen("\\include"); //! Length of "\include"

// TODO: Create a series of ReplacementMappings
static void init()
{
}

static bool fileExists(const std::string& file)
{
	struct stat sb;
	return stat(file.c_str(), &sb) == 0;
}

bool processFile(const std::string& file, Context& ctxt)
{
	if (ctxt.verbose)
		printf("Processing %s...\n", file.c_str());

	std::ifstream inf(file, std::ifstream::binary);
	if (!inf.good()) {
		printf("Error: Could not open %s\n", file.c_str());
		return false;
	}
	inf.seekg(0, std::ifstream::end);
	size_t fileSize = inf.tellg();
	inf.seekg(0, std::ifstream::beg);
	std::unique_ptr<char[]> fileBuff(new char[fileSize]);
	inf.read(fileBuff.get(), fileSize);
	inf.close();
	//! \todo Convert to UTF-8 if needed

	ParseInfo pi(file, fileBuff.get(), fileBuff.get() + fileSize, ctxt);
	while (pi.curr < pi.end) {
		// Characters to the end of the file
		const size_t remaining = pi.end - pi.curr;
		if (strncmp(pi.curr, "\\include", std::min(kIncludeLen, remaining)) == 0 ||
		    strncmp(pi.curr, "\\input", std::min(kInputLen, remaining)) == 0)
			processInclude(pi);

		// Gobble whitespace
		while (readNewline(pi));
		eatWhitespace(pi);
	}

	if (ctxt.verbose)
		printf("Done processing %s...\n", file.c_str());

	return true;
}

bool readNewline(ParseInfo& pi)
{
	if (pi.curr >= pi.end)
		return false;

	if (*pi.curr == '\n' || *pi.curr == '\r') {
		if (*pi.curr == '\r') {
			if (*(pi.curr + 1) == '\n') {
				++pi.windowsNewlines;
				pi.curr +=2;
			}
			else {
				++pi.macNewlines;
				++pi.curr;
			}
		}

		if (*pi.curr == '\n') {
			if (*(pi.curr + 1) == '\r') {
				// Rare, but possible
				++pi.windowsNewlines;
				pi.curr +=2;
			}
			else {
				++pi.unixNewlines;
				++pi.curr;
			}
		}
		return true;
	}
	return false;
}

void processInclude(ParseInfo& pi)
{
	// For printing purposes, etc., determine if it is \include or \input
	bool isInclude = pi.curr[3] == 'c';
	printf("Is %san include\n", isInclude ? "" : "not "); // TEMP

	// Increment curr appropriately
	pi.curr += isInclude ? kIncludeLen : kInputLen;

	//! Get our args
	std::unique_ptr<MacroArgs> args;
	try {
		args = parseArgs(pi);
	}
	catch (const Exceptions::InvalidInputException& ex) {
		std::stringstream err;
		err << pi.filename << ":" << pi.currLine << ": " << ex.message << " for \\include or \\import";
		throw Exceptions::InvalidInputException(err.str(), __FUNCTION__);
	}

	// We should only have one unnamed arg
	if (args->unnamed.size() != 1 || args->named.size() != 0) {
		std::stringstream err;
		err << pi.filename << ":" << pi.currLine << ": ";
		err << "\\include and \\input only take a single, unnamed argument";
		throw Exceptions::InvalidInputException(err.str(), __FUNCTION__);
	}

	const std::string& filename = args->unnamed[0];

	if (fileExists(filename + ".tex")) {
		if (pi.ctxt.verbose)
			printf("Adding %s to the list of files to be processed\n", (filename + ".tex").c_str());
	}
}

std::unique_ptr<MacroArgs> parseArgs(ParseInfo& pi) {
	// Regex for matching args
	static std::regex unquoted(R"regex(\s*([^",]+)\s*)regex");
	static std::regex quoted(R"regex(\s*"([^"]+)"\s*)regex");
	static std::regex quotedNamed(R"regex(^\s*([a-zA-Z]+)=\s*"([^"]+)"\s*,\s*$)regex");

	eatWhitespace(pi);
	// Accept one newline and more whitespace, then demand a {
	if (readNewline(pi)) {
		eatWhitespace(pi);
	}

	if (pi.curr >= pi.end)
		throw Exceptions::InvalidInputException("End of file reached before finding arguments", __FUNCTION__);

	if (*pi.curr != '{')
		throw Exceptions::InvalidInputException("A new paragraph was found before arguments were found", __FUNCTION__);

	++pi.curr;

	// Argument loop: Accept whitespace, then an arg, then whitespace, then a newline. A comma starts a new argument
	bool namedReached = false; //Becomes true when named arguments are reached
	while (true) {
		eatWhitespace(pi);
		if (readNewline(pi)) {
			eatWhitespace(pi);
			// We cannot have two newlines in a row during an argument list. Make sure we don't get another
			if (readNewline(pi)) {
				throw Exceptions::InvalidInputException("A new paragraph was found in the middle of the argument list",
				                                        __FUNCTION__);
			}
		}
		// Prepare the string to pass to regex
		const char* argEnd = pi.curr;
		while(*argEnd != '\r' && *argEnd != '\n' && *argEnd != ',' && *argEnd != '}')
			++argEnd;

	}


	return nullptr;
}
