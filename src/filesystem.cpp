#if defined(WIN32)

#include <windows.h>
#include <tchar.h>
#include "filesystem.h"

namespace {
using namespace detfc;

// --------------------------------------------------------
// Type Conversion
// --------------------------------------------------------

FileType win32FileType(DWORD dwAttributes)
{
	if(dwAttributes == (DWORD)-1){
		return FILETYPE_ERROR;
	}
	if(dwAttributes & FILE_ATTRIBUTE_DIRECTORY){
		return FILETYPE_DIRECTORY;
	}
	return FILETYPE_REGULAR;
}
ULONGLONG win32ULargeInteger(DWORD dwLow, DWORD dwHigh)
{
	ULARGE_INTEGER size;
	size.LowPart = dwLow;
	size.HighPart = dwHigh;
	return size.QuadPart;
}
FileSize win32FileSize(DWORD dwLow, DWORD dwHigh)
{
	return win32ULargeInteger(dwLow, dwHigh);
}
FileTime win32FileTime(FILETIME ft)
{
	return win32ULargeInteger(ft.dwLowDateTime, ft.dwHighDateTime);
}

// --------------------------------------------------------
// Path String Utilities
// --------------------------------------------------------

template<PathChar CH>
struct is_char_pred
{
	static const PathChar CH_ = CH;
	bool operator()(PathChar ch) const { return ch == CH_;}
};

template<PathChar CH1, PathChar CH2>
struct is_char_or_char_pred
{
	static const PathChar CH1_ = CH1;
	static const PathChar CH2_ = CH2;
	bool operator()(PathChar ch) const { return ch == CH1_ || ch == CH2_;}
};

template<typename Pred>
std::size_t find_first_char(const PathString &str, Pred pred, std::size_t pos = 0)
{
	if(str.empty() || pos >= str.size()){
		return PathString::npos;
	}
	for(PathString::const_iterator it = str.begin() + pos; it != str.end(); ++it){
#ifdef _MBCS
		if(::IsDBCSLeadByte(*it)){
			if(++it == str.end()){//skip mbcs char
				break;
			}
		}
		else{
			if(pred(*it)){
				return it - str.begin();
			}
		}
#else
		if(pred(*it)){
			return it - str.begin();
		}
#endif
	}
	return PathString::npos;
}

template<typename Pred>
std::size_t find_last_char(const PathString &str, Pred pred)
{
	std::size_t last_pos = PathString::npos;
	for(PathString::const_iterator it = str.begin(); it != str.end(); ++it){
#ifdef _MBCS
		if(::IsDBCSLeadByte(*it)){
			if(++it == str.end()){//skip mbcs char
				break;
			}
		}
		else{
			if(pred(*it)){
				last_pos = it - str.begin();
			}
		}
#else
		if(pred(*it)){
			last_pos = it - str.begin();
		}
#endif
	}
	return last_pos;
}

/**
 * 文字列中の次の文字の位置を返します。
 */
std::size_t next_char_pos(const PathString &str, std::size_t pos)
{
	if(pos >= str.size()){
		return str.size(); //end
	}
#ifdef _MBCS
	else if(::IsDBCSLeadByte(str[pos])){
		if(pos + 1 < str.size()){
			return pos + 2;
		}
		else{
			return pos + 1; //double-byte character is broken.
		}
	}
#endif
	else{
		return pos + 1;
	}
}


typedef is_char_or_char_pred<PATH_CHAR_L('\\'), PATH_CHAR_L('/')> is_separator;
typedef is_char_pred<PATH_CHAR_L('.')> is_dot;


}//namespace


namespace detfc{


// --------------------------------------------------------
// File Name
// --------------------------------------------------------

/**
 * 最後のファイル名部分の先頭の位置を返します。
 */
PathString::size_type getPathFileNamePos(const PathString &s)
{
	// 完全修飾パス
	// \\Server\a\b => b
	// \\Server\a\ => (empty)
	// \\Server\a => a
	// \\Server\ => (empty)
	// \\Server => \\Server  (*特殊)
	//
	// \\?\C:\a => a
	// \\?\C:\ => (empty)
	// \\?\C: => C:
	// \\?\ => (empty)
	// \\? => \\?  (*特殊)
	//
	// (\\?\\\Server\aみたいなのはあるの？)
	//
	// C:\a\b => b
	// C:\a\ => (empty)
	// C:\a => a
	// C:\ => (empty)
	//
	// 非完全修飾パス
	// \\?\C:a => a (*特殊? 許可されていないっぽいけど)
	//
	// \a => a
	// \ => (empty)
	//
	// C:a\b => b
	// C:a\ => (empty)
	// C:a => a  (*特殊)
	// C: => C:
	//
	// .\a => a
	// .\ =>(empty)
	// . => .
	//
	// a/b => b
	// a\ => (empty)
	// a => a
	//
	// => (empty)

	const std::size_t lastSep = find_last_char(s, is_separator());
	const std::size_t afterSep = (lastSep == PathString::npos) ? 0 : lastSep + 1;
	if(afterSep >= s.size()){
		return s.size(); //区切り文字で終わっている。または、文字列が空。
	}

	// 特殊な物を処理する。
	// ・ \\Server
	// ・ C:a
	// ・ C:
	if(lastSep == 1 && is_separator()(s[0])){
		return 0; // \\で始まり以降区切りが無い場合、全体をひとまとまりとする。
	}
	if(lastSep == PathString::npos && s.size() >= 3 && s[1] == PATH_CHAR_L(':')){ //PRN:とかのことを考えると微妙だけど、splitpathあたりもこれと等しい感じだし(_MAX_DRIVE=3)、まあいいか。
		return 2; //コロン以降のみがファイル名。
	}

	return afterSep;
}

/**
 * パス末尾のファイル名部分を返します。
 * 最後のパス区切り文字以降を返します。
 */
PathString getPathFileNamePart(const PathString &s)
{
	return PathString(s, getPathFileNamePos(s));
}

/**
 * パス末尾のファイル名部分を切り落とした文字列を返します。
 *
 * 最後のパス区切り文字は残します。入力文字列の最後がパス区切り文字で終わっている場合は、入力文字列をそのまま返します。
 */
PathString getPathNotFileNamePart(const PathString &s)
{
	return PathString(s, 0, getPathFileNamePos(s));
}

/**
 * 文字列が余分なセパレータで終わっているかどうかを返します。
 */
bool isPathTerminatedByRedundantSeparator(const PathString &s)
{
	// 完全修飾パス
	// \\Server\a\ => true?
	// \\Server\ => true?
	// \\?\C:\ => false
	// \\?\ => false
	// C:\a\ => true
	// C:\ => false
	//
	// 非完全修飾パス
	// \ => false
	// C:a\ => true
	// .\ => true
	// a\ => true

	std::size_t lastSepPos = s.size();
	PathChar prevChar = PATH_CHAR_L('\0');
	PathChar lastSepPrevChar = PATH_CHAR_L('\0');

	for(std::size_t pos = 0; pos < s.size(); pos = next_char_pos(s, pos)){
		PathChar currChar = s[pos];
		if(is_separator()(currChar)){
			lastSepPos = pos;
			lastSepPrevChar = prevChar;
		}
		prevChar = currChar;
	}

	return (lastSepPos + 1 == s.size()
		&& lastSepPrevChar != PATH_CHAR_L(':')
		&& lastSepPrevChar != PATH_CHAR_L('?')
		&& lastSepPrevChar != PATH_CHAR_L('\0'));
}


/**
 * パス末尾の余分なセパレータを切り落とした文字列を返します。
 *
 * 切り落とすのは余分なセパレータであり、切り落とすことによって意味が変わってしまう場合は切り落としません。
 */
PathString getPathWithoutLastRedundantSeparator(const PathString &s)
{
	if(isPathTerminatedByRedundantSeparator(s)){
		return PathString(s, 0, s.size() - 1);
	}
	else{
		return s;
	}
}

PathString getPathDirectoryPart(const PathString &s)
{
	return getPathWithoutLastRedundantSeparator(getPathNotFileNamePart(s));
}

PathString concatPath(const PathString &a, const PathString &b)
{
	if(a.empty()){
		return b;
	}
	else if(b.empty()){
		return a;
	}
	else if(isPathTerminatedByRedundantSeparator(a)){
		return a + b;
	}
	else{
		return a + PATH_CHAR_L("\\") + b;
	}
}


// --------------------------------------------------------
// File Operation
// --------------------------------------------------------

FileType getPathFileType(const PathString &p)
{
	return win32FileType(::GetFileAttributes(p.c_str()));
}

bool isPathExists(const PathString &p)
{
	return getPathFileType(p) != FILETYPE_ERROR;
}
bool isPathDirectory(const PathString &p)
{
	return getPathFileType(p) == FILETYPE_DIRECTORY;
}
bool isPathRegularFile(const PathString &p)
{
	return getPathFileType(p) == FILETYPE_REGULAR;
}

DirectoryEntry getPathDirectoryEntry(const PathString &p)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if(!GetFileAttributesEx(p.c_str(), GetFileExInfoStandard, &data)){
		return DirectoryEntry(
			getPathDirectoryPart(p),
			getPathFileNamePart(p));
	}

	return DirectoryEntry(
		getPathDirectoryPart(p),
		getPathFileNamePart(p),
		win32FileType(data.dwFileAttributes),
		win32FileSize(data.nFileSizeLow, data.nFileSizeHigh),
		win32FileTime(data.ftLastWriteTime));
}

FileTime getPathLastWriteTime(const PathString &p)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if(!GetFileAttributesEx(p.c_str(), GetFileExInfoStandard, &data)){
		return 0;
	}
	return win32FileTime(data.ftLastWriteTime);
}

FileTime getPathFileSize(const PathString &p)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if(!GetFileAttributesEx(p.c_str(), GetFileExInfoStandard, &data)){
		return 0;
	}
	return win32FileSize(data.nFileSizeLow, data.nFileSizeHigh);
}




// --------------------------------------------------------
// DirectoryEntryEnumerator
// --------------------------------------------------------

class DirectoryEntryEnumerator::Impl
{
	HANDLE handle_;
	WIN32_FIND_DATA data_;
	DirectoryEntry entry_;
public:
	explicit Impl(const PathString &dir)
		: handle_(INVALID_HANDLE_VALUE)
		, entry_(dir, PathString(), FILETYPE_ERROR, 0, 0)
	{
		const PathString asterisk( PATH_CHAR_L("*") );
		const PathString searchPath = concatPath(dir, asterisk);
		handle_ = ::FindFirstFile(searchPath.c_str(), &data_);
		makeEntry();
	}
	~Impl()
	{
		close();
	}
	bool isValid() const
	{
		return handle_ != INVALID_HANDLE_VALUE;
	}
	void close()
	{
		if(isValid()){
			::FindClose(handle_);
			handle_ = INVALID_HANDLE_VALUE;
		}
	}
	void increment()
	{
		next();
		makeEntry();
	}
	const DirectoryEntry &getEntry() const
	{
		return entry_;
	}
private:
	void makeEntry()
	{
		while(isValid() && (lstrcmp(data_.cFileName, _T(".")) == 0 || lstrcmp(data_.cFileName, _T("..")) == 0)){
			next();
		}

		if(isValid()){
			entry_.assign(
				data_.cFileName,
				win32FileType(data_.dwFileAttributes),
				win32FileSize(data_.nFileSizeLow, data_.nFileSizeHigh),
				win32FileTime(data_.ftLastWriteTime));
		}
	}
	void next()
	{
		if(isValid()){
			if(!::FindNextFile(handle_, &data_)){
				close();
			}
		}
	}
};


DirectoryEntryEnumerator::DirectoryEntryEnumerator(const PathString &dir) : impl_(new Impl(dir)) {}
DirectoryEntryEnumerator::~DirectoryEntryEnumerator() {}
bool DirectoryEntryEnumerator::isEnd() const {return !impl_->isValid();}
const DirectoryEntry &DirectoryEntryEnumerator::getEntry() const { return impl_->getEntry();}
void DirectoryEntryEnumerator::increment() {impl_->increment();}

}//namespace detfc

#endif //defined(WIN32)


#if !defined(WIN32)
//#include <unistd.h>
#endif //!defined(WIN32)
