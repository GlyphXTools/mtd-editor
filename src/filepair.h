//
// This file defines class that represents a MTD/TGA file pair
//
#ifndef FILEPAIR_H
#define FILEPAIR_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <map>

#include <FreeImage.h>

struct FileInfo
{
	unsigned long x, y;
	unsigned long w, h;
	unsigned char used;
};

typedef std::map<std::wstring,FileInfo> FileMap;

class FilePair
{
	class FilePairImpl;
	FilePairImpl* pimpl;

public:
	const std::wstring& getIndexFilename() const;
	const std::wstring& getImageFilename() const;
	const FileInfo*     getFileInfo(const std::wstring& filename) const;
	const FileInfo*     getSelected() const;			// Get the currently selected file
	const FileMap&      getFiles() const;			// Get a list of the files
	unsigned int	    getNumFiles() const;			// How many files are in the directory?
	bool                isModified() const;			// Has the pair been modified?
	bool                isUnnamed() const;			// Does the pair have a name?
	bool                isReadOnly() const;          // Is this file read only?

	// Blit the selected file to the Device Context at specified coordinates
	BOOL BltSelected(HDC hdcDest, int nXDest, int nYDest);
	bool setSelected(const std::wstring filename);

	// Directory manipulation
	void insertFiles(std::vector<std::wstring>& filenames);
	bool renameFile(const std::wstring& filename, const std::wstring& target);
	void extractFile( const std::wstring& filename, const std::wstring& target, FREE_IMAGE_FORMAT format = FIF_UNKNOWN );
	void deleteFile( const std::wstring& filename );

	// Save both or either file
	void save(FREE_IMAGE_FORMAT format = FIF_UNKNOWN);
	void saveIndex(const std::wstring& filename);
	void saveImage(const std::wstring& filename, FREE_IMAGE_FORMAT format = FIF_UNKNOWN);

	// Open an MTD and TGA file
	FilePair(const std::wstring& mtdFilename, const std::wstring& tgaFilename);

	// Create an empty image and directory
	FilePair(unsigned int width, unsigned int height);

	~FilePair();
};

#endif