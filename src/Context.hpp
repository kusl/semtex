#ifndef __CONTEXT_HPP__
#define __CONTEXT_HPP__

#include "FileQueue.hpp"

//! A global context. Used to pass around a ball of variables shared by lots of the code.
struct Context {
	bool verbose; //!< True to print additional information to stdout
	std::atomic_bool error; //!< Error flag. When this is raised, threads should no longer process more files
	std::vector<std::string> generatedFiles; //!< LaTeX files generated by SemTeX
	std::mutex generatedFilesMutex; //!< A mutex for generatedFiles
	FileQueue queue; //!< Queue of SemTeX files to be processed

	//! Constructor (just hands callback to queue)
	Context(FileQueue::QueueUsedCallback cb) : error(false), queue(cb) { }
};

#endif
