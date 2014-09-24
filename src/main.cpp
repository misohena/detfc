/**
 * @file
 * @brief Detect File Change
 * @since 2014-09-22
 * @author AKIYAMA Kouhei
 */
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <cstdlib>
#include <cassert>
#include <cctype>

#include "filesystem.h"
#include "binaryio.h"



namespace detfc {

class CommandLine
{
public:
	std::vector<PathString> targets_;
	bool includesDirectoryInTarget_;
	bool includesSubEntriesInTarget_;
	PathString dbFile_;
	PathString commandChanged_;
	std::string checkingMethod_;
	std::vector<PathString> targetExtensions_;
public:
	CommandLine()
		: includesDirectoryInTarget_(false)
		, includesSubEntriesInTarget_(false)
		, checkingMethod_()
	{}

	const std::vector<PathString> &getTargets() const { return targets_;}
	bool optIncludesDirectoryInTarget() const { return includesDirectoryInTarget_;}
	bool optIncludesSubEntriesInTarget() const { return includesSubEntriesInTarget_;}
	PathString getDBFile() const { return dbFile_;}
	PathString getCommandChanged() const { return commandChanged_;}
	const std::string &getCheckingMethod() const { return checkingMethod_;}

	bool matchTargetExtension(const PathString &p) const
	{
		if (targetExtensions_.empty()){
			return true;
		}
		for (const PathString &ext : targetExtensions_){
			if (p.size() >= ext.size()){
				std::size_t count = ext.size();
				for (PathString::const_iterator a = p.end() - ext.size(), b = ext.begin(); count; --count, ++a, ++b){
					if (std::toupper(*a) != std::toupper(*b)){
						break;
					}
				}
				if (count == 0){
					return true;
				}
			}
		}
		return false;
	}

	bool parse(int argc, char * const *argv)
	{
		assert(argc >= 1);
		char * const *argIt = argv + 1;
		const char * const *argEnd = argv + argc;
		for(; argIt != argEnd; ++argIt){
			const std::string arg(*argIt);

			if(arg[0] == '-'){
				if(arg == "-r"){
					includesSubEntriesInTarget_ = true;
				}
				else if (arg == "-d"){
					includesDirectoryInTarget_ = true;
				}
				else if (arg == "-db"){
					if(++argIt == argEnd){
						std::cerr << arg << " <DB filename>" << std::endl;
						return false;
					}
					dbFile_ = *argIt;
				}
				else if(arg == "-e"){
					if(++argIt == argEnd){
						std::cerr << arg << " <command>" << std::endl;
						return false;
					}
					commandChanged_ = *argIt;
				}
				else if(arg == "-m"){
					if (++argIt == argEnd){
						std::cerr << arg << " <checking method name(0-2)>" << std::endl;
						return false;
					}
					checkingMethod_ = *argIt;
				}
				else if (arg == "-ext"){
					if (++argIt == argEnd){
						std::cerr << arg << " <target extension>" << std::endl;
						return false;
					}
					targetExtensions_.push_back(*argIt);
				}
				else{
					std::cerr << "Unknown option: " << arg << std::endl;
					return false;
				}
			}
			else{
				targets_.push_back(arg);
			}
		}

		if(dbFile_.empty()){
			std::cerr << "-db <DB filename>を指定してください。" << std::endl;
			return false;
		}
		return true;
	}
};


class CheckingMethod
{
	bool changed_;
protected:
	const CommandLine &cmdline_;
	CheckingMethod(const CommandLine &cmdline)
		: cmdline_(cmdline)
		, changed_(false)
	{}
	void setChanged(){ changed_ = true; }
	bool getChanged() const { return changed_; }

	bool isEntryTarget(const DirectoryEntry &entry) const
	{
		if (entry.getFileType() == FILETYPE_ERROR){
			std::cerr << "ファイル'" << entry.getPath() << "'の情報を取得できませんでした。" << std::endl;
		}
		return entry.isDirectory() && cmdline_.optIncludesDirectoryInTarget()
			|| entry.isRegularFile() && cmdline_.matchTargetExtension(entry.getFilename());
	}
public:
	virtual bool check() = 0;
	virtual void readDB() = 0;
	virtual void writeDB() = 0;
};

class CheckingMethodFactory
{
public:
	typedef CheckingMethod *(*MethodFactoryFun)(const CommandLine &);
private:
	typedef std::map<std::string, MethodFactoryFun> MethodNameMap;
	static MethodNameMap &getMethodNameMap()
	{
		static MethodNameMap mnm;
		return mnm;
	}
public:
	static MethodFactoryFun getMethod(const std::string &name)
	{
		auto it = getMethodNameMap().find(name);
		return it == getMethodNameMap().end() ? nullptr : it->second;
	}
	template<typename T>
	struct Reg
	{
		Reg(const char * const name)
		{
			getMethodNameMap()[name] = create;
		}
		static CheckingMethod *create(const CommandLine &cmdline)
		{
			return new T(cmdline);
		}
	};
};

/**
 * DBファイルより新しいターゲットが存在すれば変更されたと見なすアルゴリズムです。
 */
class CheckingMethod0 : public CheckingMethod
{
	FileTime dbTime_;
public:
	CheckingMethod0(const CommandLine &cmdline)
		: CheckingMethod(cmdline)
		, dbTime_(0)
	{}

	virtual bool check()
	{
		for(auto target : cmdline_.getTargets()){
			if(checkPath(target)){
				setChanged();
				return true;
			}
		}
		return false;
	}

private:
	bool checkPath(const PathString &path)
	{
		return checkEntry(getPathDirectoryEntry(path));
	}
	bool checkEntry(const DirectoryEntry &entry)
	{
		if (isEntryTarget(entry)){
			if (checkTargetEntry(entry)){
				return true;
			}
		}

		if(entry.isDirectory() && cmdline_.optIncludesSubEntriesInTarget()){
			if(checkDirectorySubEntries(entry.getPath())){
				return true;
			}
		}
		return false;
	}
	bool checkDirectorySubEntries(const PathString &dir)
	{
		DirectoryEntryEnumerator etor(dir);
		for(; !etor.isEnd(); etor.increment()){
			const DirectoryEntry &entry = etor.getEntry();
			if(checkEntry(entry)){
				return true;
			}
		}
		return false;
	}
	bool checkTargetEntry(const DirectoryEntry &entry)
	{
		return entry.getLastWriteTime() > getDBModifiedTime();
	}

public:
	virtual void readDB()
	{
		dbTime_ = getPathLastWriteTime(cmdline_.getDBFile());
	}

	virtual void writeDB()
	{
		std::ofstream ofs(cmdline_.getDBFile().c_str());
	}
private:
	FileTime getDBModifiedTime()
	{
		return dbTime_;
	}
};
static CheckingMethodFactory::Reg<CheckingMethod0> reg0_0("0");
static CheckingMethodFactory::Reg<CheckingMethod0> reg0_1("fast");


/**
 * ディレクトリ毎の総ターゲット数、総ファイルサイズ、最新更新日時が変わっていれば変化したと見なすアルゴリズムです。
 * コマンドラインで直接指定したターゲット(トップレベルターゲット)は、まとめて一つのディレクトリ下にあるものとして判定します。
 *
 * 全てのターゲットの情報をスキャンする必要があるので、CheckingMethod0より時間がかかりますが、より正確です。
 * 保存・比較する情報が少ないので、CheckingMethod2より高速・低消費容量ですが、より不正確です。
 */
class CheckingMethod1 : public CheckingMethod
{
	struct DirSummary
	{
		typedef unsigned int FileCount;
		FileCount totalFileCount;
		FileSize totalFileSize;
		FileTime latestFileTime;
		///@todo Add filename hash (sorted by filename?)
		DirSummary(FileCount totalFileCount_ = 0, FileSize totalFileSize_ = 0, FileTime latestFileTime_ = 0)
			: totalFileCount(totalFileCount_), totalFileSize(totalFileSize_), latestFileTime(latestFileTime_)
		{}
		void add(const DirectoryEntry &entry)
		{
			++totalFileCount;
			totalFileSize += entry.getFileSize();
			if (entry.getLastWriteTime() > latestFileTime){
				latestFileTime = entry.getLastWriteTime();
			}
		}
		bool operator==(const DirSummary &rhs) const
		{
			return totalFileCount == rhs.totalFileCount &&
				totalFileSize == rhs.totalFileSize &&
				latestFileTime == rhs.latestFileTime;
		}
		bool operator!=(const DirSummary &rhs) const
		{
			return !operator==(rhs);
		}
	};
	std::vector<std::pair<PathString, DirSummary>> dirs_;
	std::map<PathString, DirSummary> dirsPrev_;
	DirSummary topLevel_;
	DirSummary topLevelPrev_;
public:
	CheckingMethod1(const CommandLine &cmdline)
		: CheckingMethod(cmdline)
	{}

	bool check()
	{
		for (auto target : cmdline_.getTargets()){
			checkTopLevelEntry(getPathDirectoryEntry(target));
		}
		if(topLevel_ != topLevelPrev_){
			setChanged();
		}
		if (!dirsPrev_.empty()){ // found deleted directory
			setChanged();
		}
		return getChanged();
	}
private:
	void checkTopLevelEntry(const DirectoryEntry &entry)
	{
		if (isEntryTarget(entry)){
			topLevel_.add(entry);
		}
		checkEntry(entry);
	}
	void checkEntry(const DirectoryEntry &entry)
	{
		if (entry.isDirectory() && cmdline_.optIncludesSubEntriesInTarget()){
			checkDirectorySubEntries(entry.getPath());
		}
	}
	void checkDirectorySubEntries(const PathString &dir)
	{
		DirSummary dirSummary;

		DirectoryEntryEnumerator etor(dir);
		for (; !etor.isEnd(); etor.increment()){
			const DirectoryEntry entry = etor.getEntry();
			checkEntry(entry);

			if (isEntryTarget(entry)){
				dirSummary.add(entry);
			}
		}

		dirs_.push_back(std::pair<PathString, DirSummary>(dir, dirSummary));
		auto it = dirsPrev_.find(dir);
		if (it == dirsPrev_.end()){
			// new directory
			setChanged();
		}
		else{
			if(it->second != dirSummary){
				setChanged();
			}
			dirsPrev_.erase(it);
		}
	}
public:
	static const unsigned int DB_MAGIC = 'd'|('f'<<8)|('c'<<16)|('1'<<24);
	virtual void readDB()
	{
		std::ifstream ifs(cmdline_.getDBFile().c_str(), std::ios::binary);
		if(!ifs){
			return; //cannot open.
		}
		if (readBinary<unsigned int>(ifs) != DB_MAGIC){
			return;
		}
		DirSummary topLevel = readDirSummary(ifs);
		const std::size_t dirCount = readBinary<std::size_t>(ifs);
		if (!ifs){
			return;
		}
		std::map<PathString, DirSummary> dirs;
		for (std::size_t i = 0; i < dirCount; ++i){
			PathString dirName = readStringBinary(ifs);
			DirSummary dirSummary = readDirSummary(ifs);
			if (!ifs){
				return;
			}
			dirs.insert(std::pair<PathString, DirSummary>(dirName, dirSummary));
		}

		topLevelPrev_ = topLevel;
		dirsPrev_.swap(dirs);
	}
	static DirSummary readDirSummary(std::istream &ifs)
	{
		const DirSummary::FileCount totalFileCount = readBinary<DirSummary::FileCount>(ifs);
		const FileSize totalFileSize = readBinary<FileSize>(ifs);
		const FileTime latestFileTime = readBinary<FileTime>(ifs);
		if (!ifs){
			return DirSummary();
		}
		else{
			return DirSummary(totalFileCount, totalFileSize, latestFileTime);
		}
	}
	virtual void writeDB()
	{
		std::ofstream ofs(cmdline_.getDBFile().c_str(), std::ios::binary);
		if (!ofs){
			std::cerr << "出力ファイル'" << cmdline_.getDBFile() << "'が開けませんでした。" << std::endl;
			return;
		}
		writeBinary(ofs, DB_MAGIC);
		writeDirSummary(ofs, topLevel_);
		writeBinary(ofs, dirs_.size());
		for (auto dirNameSummary : dirs_){
			writeStringBinary(ofs, dirNameSummary.first);
			writeDirSummary(ofs, dirNameSummary.second);
		}
	}
	static void writeDirSummary(std::ostream &os, DirSummary &s)
	{
		writeBinary(os, s.totalFileCount);
		writeBinary(os, s.totalFileSize);
		writeBinary(os, s.latestFileTime);
	}
};
static CheckingMethodFactory::Reg<CheckingMethod1> reg1_0("1");
static CheckingMethodFactory::Reg<CheckingMethod1> reg1_1("dirsummary");


/**
 * エントリーの情報(タイプ、サイズ、更新日時)が変化したり、追加や削除があったときに変化したと見なすアルゴリズムです。
 */
class CheckingMethod2 : public CheckingMethod
{
	std::vector<DirectoryEntry> targets_;
	std::map<PathString, DirectoryEntry> targetsPrev_;
public:
	CheckingMethod2(const CommandLine &cmdline)
		: CheckingMethod(cmdline)
	{}

	bool check()
	{
		for(auto target : cmdline_.getTargets()){
			checkPath(target);
		}

		if(!targetsPrev_.empty()){ //found deleted files
			setChanged();
		}
		return getChanged();
	}

private:
	void checkPath(const PathString &path)
	{
		checkEntry(getPathDirectoryEntry(path));
	}
	void checkEntry(const DirectoryEntry &entry)
	{
		if (isEntryTarget(entry)){
			checkTargetEntry(entry);
		}
		if (entry.isDirectory() && cmdline_.optIncludesSubEntriesInTarget()){
			checkDirectorySubEntries(entry.getPath());
		}
	}
	void checkDirectorySubEntries(const PathString &dir)
	{
		DirectoryEntryEnumerator etor(dir);
		for(; !etor.isEnd(); etor.increment()){
			checkEntry(etor.getEntry());
		}
	}
	void checkTargetEntry(const DirectoryEntry &entry)
	{
		targets_.push_back(entry);

		auto it = targetsPrev_.find(entry.getPath());
		if(it == targetsPrev_.end()){
			// new file
			setChanged();
		}
		else{
			if(entry.getFileType() != it->second.getFileType()
			|| entry.getLastWriteTime() != it->second.getLastWriteTime()
			|| entry.getFileSize() != it->second.getFileSize()){
				// changed
				setChanged();
			}
			else{
				// may be not changed
			}
			targetsPrev_.erase(it);
		}
	}

public:
	static const unsigned int DB_MAGIC = 'd'|('f'<<8)|('c'<<16)|('2'<<24);
	virtual void readDB()
	{
		std::ifstream ifs(cmdline_.getDBFile().c_str(), std::ios::binary);
		if(!ifs){
			return; //cannot open.
		}
		if (readBinary<unsigned int>(ifs) != DB_MAGIC){
			return;
		}
		const std::size_t targetCount = readBinary<std::size_t>(ifs);
		if(!ifs){
			return; //failed to read targetCount.
		}

		std::map<PathString, DirectoryEntry> targets;

		for(std::size_t i = 0; i < targetCount; ++i){
			const PathString path = readStringBinary(ifs);
			const FileType fileType = readBinary<FileType>(ifs);
			const FileSize fileSize = readBinary<FileSize>(ifs);
			const FileTime lastWriteTime = readBinary<FileTime>(ifs);
			if(!ifs){
				return; //failed to read a target information.
			}

			targets.insert(std::pair<PathString, DirectoryEntry>(path, DirectoryEntry(
				getPathDirectoryPart(path),
				getPathFileNamePart(path),
				fileType, fileSize, lastWriteTime)));
		}

		targetsPrev_.swap(targets);
	}

	virtual void writeDB()
	{
		std::ofstream ofs(cmdline_.getDBFile().c_str(), std::ios::binary);
		if(!ofs){
			std::cerr << "出力ファイル'" << cmdline_.getDBFile() << "'が開けませんでした。" << std::endl;
			return;
		}
		writeBinary(ofs, DB_MAGIC);
		writeBinary(ofs, targets_.size());
		for(const DirectoryEntry &entry : targets_){
			writeStringBinary(ofs, entry.getPath());
			writeBinary(ofs, entry.getFileType());
			writeBinary(ofs, entry.getFileSize());
			writeBinary(ofs, entry.getLastWriteTime());
		}
	}
};
static CheckingMethodFactory::Reg<CheckingMethod2> reg2_0("2");
static CheckingMethodFactory::Reg<CheckingMethod2> reg2_1("filestat");
static CheckingMethodFactory::Reg<CheckingMethod2> reg2_2(""); //default

}//namespace detfc



int main(int argc, char *argv[])
{
	using namespace detfc;

	CommandLine cmdline;
	if(!cmdline.parse(argc, argv)){
		return -1;
	}

	CheckingMethodFactory::MethodFactoryFun creator = CheckingMethodFactory::getMethod(cmdline.getCheckingMethod());
	if(!creator){
		std::cerr << "Unknown checking method name '" << cmdline.getCheckingMethod() << "' specified." << std::endl;
		return -1;
	}
	const std::unique_ptr<CheckingMethod> checker(creator(cmdline));
	checker->readDB();

	if(checker->check()){
		checker->writeDB();

		if(!cmdline.getCommandChanged().empty()){
			std::system(cmdline.getCommandChanged().c_str());
		}
	}
	return 0;
}

