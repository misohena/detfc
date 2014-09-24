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
 * �����񒆂̎��̕����̈ʒu��Ԃ��܂��B
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
 * �Ō�̃t�@�C���������̐擪�̈ʒu��Ԃ��܂��B
 */
PathString::size_type getPathFileNamePos(const PathString &s)
{
	// ���S�C���p�X
	// \\Server\a\b => b
	// \\Server\a\ => (empty)
	// \\Server\a => a
	// \\Server\ => (empty)
	// \\Server => \\Server  (*����)
	//
	// \\?\C:\a => a
	// \\?\C:\ => (empty)
	// \\?\C: => C:
	// \\?\ => (empty)
	// \\? => \\?  (*����)
	//
	// (\\?\\\Server\a�݂����Ȃ̂͂���́H)
	//
	// C:\a\b => b
	// C:\a\ => (empty)
	// C:\a => a
	// C:\ => (empty)
	//
	// �񊮑S�C���p�X
	// \\?\C:a => a (*����? ������Ă��Ȃ����ۂ�����)
	//
	// \a => a
	// \ => (empty)
	//
	// C:a\b => b
	// C:a\ => (empty)
	// C:a => a  (*����)
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
		return s.size(); //��؂蕶���ŏI����Ă���B�܂��́A�����񂪋�B
	}

	// ����ȕ�����������B
	// �E \\Server
	// �E C:a
	// �E C:
	if(lastSep == 1 && is_separator()(s[0])){
		return 0; // \\�Ŏn�܂�ȍ~��؂肪�����ꍇ�A�S�̂��ЂƂ܂Ƃ܂�Ƃ���B
	}
	if(lastSep == PathString::npos && s.size() >= 3 && s[1] == PATH_CHAR_L(':')){ //PRN:�Ƃ��̂��Ƃ��l����Ɣ��������ǁAsplitpath�����������Ɠ�������������(_MAX_DRIVE=3)�A�܂��������B
		return 2; //�R�����ȍ~�݂̂��t�@�C�����B
	}

	return afterSep;
}

/**
 * �p�X�����̃t�@�C����������Ԃ��܂��B
 * �Ō�̃p�X��؂蕶���ȍ~��Ԃ��܂��B
 */
PathString getPathFileNamePart(const PathString &s)
{
	return PathString(s, getPathFileNamePos(s));
}

/**
 * �p�X�����̃t�@�C����������؂藎�Ƃ����������Ԃ��܂��B
 *
 * �Ō�̃p�X��؂蕶���͎c���܂��B���͕�����̍Ōオ�p�X��؂蕶���ŏI����Ă���ꍇ�́A���͕���������̂܂ܕԂ��܂��B
 */
PathString getPathNotFileNamePart(const PathString &s)
{
	return PathString(s, 0, getPathFileNamePos(s));
}

/**
 * �����񂪗]���ȃZ�p���[�^�ŏI����Ă��邩�ǂ�����Ԃ��܂��B
 */
bool isPathTerminatedByRedundantSeparator(const PathString &s)
{
	// ���S�C���p�X
	// \\Server\a\ => true?
	// \\Server\ => true?
	// \\?\C:\ => false
	// \\?\ => false
	// C:\a\ => true
	// C:\ => false
	//
	// �񊮑S�C���p�X
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
 * �p�X�����̗]���ȃZ�p���[�^��؂藎�Ƃ����������Ԃ��܂��B
 *
 * �؂藎�Ƃ��̂͗]���ȃZ�p���[�^�ł���A�؂藎�Ƃ����Ƃɂ���ĈӖ����ς���Ă��܂��ꍇ�͐؂藎�Ƃ��܂���B
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
