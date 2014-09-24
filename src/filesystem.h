#ifndef DETFC_FILESYSTEM_H_INCLUDED
#define DETFC_FILESYSTEM_H_INCLUDED

#include <string>
#include <memory>
#include <cstdint>

namespace detfc{

typedef char PathChar;
typedef std::string PathString;
#define PATH_CHAR_L(x) x

// File Name

PathString getPathFileNamePart(const PathString &s);
PathString getPathNotFileNamePart(const PathString &s);
bool isPathTerminatedByRedundantSeparator(const PathString &s);
PathString getPathWithoutLastRedundantSeparator(const PathString &s);
PathString getPathDirectoryPart(const PathString &s);
PathString concatPath(const PathString &a, const PathString &b);

// Directory Entry

enum FileType
{
	FILETYPE_ERROR,
	FILETYPE_REGULAR,
	FILETYPE_DIRECTORY
};
typedef std::uint64_t FileTime;
typedef std::uint64_t FileSize;

class DirectoryEntry
{
	PathString dir_;
	PathString filename_;
	FileType type_;
	FileSize size_;
	FileTime lastWriteTime_;
public:
	DirectoryEntry(
		const PathString &dir = PathString(),
		const PathString &filename = PathString(),
		FileType type = FILETYPE_ERROR,
		FileSize size = 0,
		FileTime lastWriteTime = 0)
		: dir_(dir), filename_(filename), type_(type), size_(size), lastWriteTime_(lastWriteTime){}
	PathString getPath() const { return concatPath(dir_, filename_);}
	PathString getFilename() const { return filename_;}
	FileTime getLastWriteTime() const { return lastWriteTime_;}
	FileSize getFileSize() const { return size_;}
	FileType getFileType() const { return type_;}
	bool isDirectory() const { return type_ == FILETYPE_DIRECTORY;}
	bool isRegularFile() const { return type_ == FILETYPE_REGULAR;}

	void assign(const PathString &filename, FileType type, FileSize size, FileTime lastWriteTime)
	{
		filename_ = filename;
		type_ = type;
		size_ = size;
		lastWriteTime_ = lastWriteTime;
	}
};

class DirectoryEntryEnumerator
{
	class Impl;
	std::shared_ptr<Impl> impl_;
public:
	DirectoryEntryEnumerator(const PathString &dir);
	~DirectoryEntryEnumerator();
	const DirectoryEntry &getEntry() const;
	void increment();
	bool isEnd() const;
};


// File Operation

FileType getPathFileType(const PathString &p);
bool isPathExists(const PathString &p);
bool isPathDirectory(const PathString &p);
bool isPathRegularFile(const PathString &p);
DirectoryEntry getPathDirectoryEntry(const PathString &p);
FileTime getPathLastWriteTime(const PathString &p);
FileTime getPathFileSize(const PathString &p);


}//namespace detfc
#endif
