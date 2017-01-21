//
// This file contains the administration of the MTD and TGA file pair.
//
// An important thing to keep in mind is that all images mentioned in the MTD
// are actually 2px by 2px larger because of the 1px border that surrounds each
// image. So, if the MTD indicates (14,14,50,50) as x,y,w,h, the image really
// occupies 52x52 pixels at coordinates (13,13).
// The 1px border is simply a copy of the border from the actual image.
//
// It's a guess, but I think this is to prevent parts of images from showing
// up at the wrong places due to rasterization rounding errors when using the
// image as a texture for the GUI primitives.
//
#include <algorithm>
#include <fstream>

#include "filepair.h"
#include "freeimage.h"
#include "freearea.h"
#include "exceptions.h"
#include "Utils.h"
#include "resource.h"
using namespace std;

#pragma pack(1)
struct FILEINFO
{
	char name[64];
	uint32_t x, y, w, h;
	uint8_t used;
};
#pragma pack()

static const int IMAGE = 1;
static const int INDEX = 2;

class FilePair::FilePairImpl
{
	// Read a bitmap file
	static FIBITMAP* ReadBitmapFile( const wstring& filename );

	// Make sure to call this AFTER ReadBitmapFile; it uses the size of bitmap.bitmap to validate the index values
	void ReadIndexFile( const wstring& filename );

public:
	FileInfo* selected;			// Currently selected file
	wstring    indexFilename;	// MTD filename
	wstring    imageFilename;	// TGA filename
	FreeArea  freearea;			// Free/Used rectangle administration
	FIBITMAP* bitmap;			// The bitmap
	bool      readOnly;			// Is the file read-only?
	int       modified;			// bit0 = image has been modified, bit1 = index has been modified

	map<wstring, FileInfo> files;

	// Saving
	void saveIndex(const std::wstring& filename);
	void saveImage(const std::wstring& filename, FREE_IMAGE_FORMAT format);
	void saveBitmapFile(const FileInfo& fi, FIBITMAP* bitmap, const wstring& filename, FREE_IMAGE_FORMAT format);

	// Insertion
	void insertFiles( vector<wstring>& filenames );

	FilePairImpl( unsigned int width, unsigned int height);
	FilePairImpl( const wstring& filename1, const wstring& filename2);
	~FilePairImpl();
};

void FilePair::FilePairImpl::saveImage(const std::wstring& filename, FREE_IMAGE_FORMAT format)
{
	// Save the texture
	if (format == FIF_UNKNOWN)
	{
		format = FreeImage_GetFIFFromFilenameU( filename.c_str() );
	}

	if (!FreeImage_SaveU(format, bitmap, filename.c_str(), 0 ))
	{
        throw wruntime_error(LoadString(IDS_ERROR_IMAGE_SAVE));
	}

	modified &= ~IMAGE;
	imageFilename = filename;
}

void FilePair::FilePairImpl::saveIndex(const std::wstring& filename)
{
	// Save the index file
	HANDLE hFile = CreateFile(filename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		throw wruntime_error(LoadString(IDS_ERROR_FILE_CREATE));
	}

	DWORD written;
	uint32_t count = htolel((uint32_t)files.size());
	if (!WriteFile(hFile, &count, sizeof count, &written, NULL) || written != sizeof count)
	{
        CloseHandle(hFile);
	    throw wruntime_error(LoadString(IDS_ERROR_FILE_WRITE));
    }

    // Dump the files
    for (map<wstring,FileInfo>::const_iterator i = files.begin(); i != files.end(); i++)
    {
	    const FileInfo& fi = i->second;

	    FILEINFO output;
	    output.x = htolel(fi.x);
	    output.y = htolel(fi.y);
	    output.w = htolel(fi.w);
	    output.h = htolel(fi.h);
	    output.used = fi.used;

	    memset(output.name, 0, 64);
	    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, i->first.c_str(), (int)i->first.length(), output.name, 64, "_", NULL);

	    if (!WriteFile(hFile, &output, sizeof output, &written, NULL) || written != sizeof output)
	    {
            CloseHandle(hFile);
		    throw wruntime_error(LoadString(IDS_ERROR_FILE_WRITE));
	    }
    }
    CloseHandle(hFile);

    indexFilename = filename;
    modified &= ~INDEX;
}

void FilePair::FilePairImpl::saveBitmapFile(const FileInfo& fi, FIBITMAP* bitmap, const wstring& filename, FREE_IMAGE_FORMAT format)
{
	wstring name = filename;
	if (format == FIF_UNKNOWN)
	{
		if (name.find_last_of('.') == wstring::npos)
		{
			name.append(L".TGA");
		}
		format = FreeImage_GetFIFFromFilenameU( name.c_str() );
	}

	FIBITMAP* dib = FreeImage_Copy(bitmap, fi.x, fi.y, fi.x + fi.w, fi.y + fi.h);
	if (dib == NULL)
	{
		throw wruntime_error(LoadString(IDS_ERROR_BITMAP_COPY));
	}
	FreeImage_SaveU(format, dib, name.c_str(), 0 );
	FreeImage_Unload(dib);
}

static inline unsigned long GetArea( FIBITMAP* bitmap )
{
	return FreeImage_GetWidth(bitmap) * FreeImage_GetHeight(bitmap);
}

// When we insert a large batch of files, sort them by descending area. This way,
// allocation of free rectangles will be more efficient.
static void QuickSortAreaDesc( vector<wstring>& filenames, vector<FIBITMAP*>& bitmaps, int start, int end)
{
	unsigned long pivot = GetArea(bitmaps[(start + end) / 2]);
	int k = start, m = end;
	do
	{
		while (GetArea(bitmaps[k]) > pivot) k++;
		while (GetArea(bitmaps[m]) < pivot) m--;
		if (k <= m)
		{
			swap(filenames[k], filenames[m]);
			swap(bitmaps[k++], bitmaps[m--]);
		}
	} while (k <= m);
	if (start < m) QuickSortAreaDesc(filenames, bitmaps, start, m);
	if (k < end)   QuickSortAreaDesc(filenames, bitmaps, k, end);
}

void FilePair::FilePairImpl::insertFiles( vector<wstring>& filenames )
{
	if (readOnly)
	{
		return;
	}

	vector<FIBITMAP*> bitmaps;

	try
	{
		// Read the files
		size_t i;
		for (i = 0; i < filenames.size(); i++)
		{
			FIBITMAP* dib = ReadBitmapFile( filenames[i] );
			bitmaps.push_back( dib );
		}

		// Sort them by area, descending
		QuickSortAreaDesc( filenames, bitmaps, 0, (int)filenames.size() - 1);

		vector<FreeArea::RECT> areas;

		// Allocate space
		FIBITMAP*     newBitmap = NULL;
		unsigned long newWidth  = FreeImage_GetWidth(bitmap);
		unsigned long newHeight = FreeImage_GetHeight(bitmap);

		FreeArea backup = freearea;
		for (i = 0; i < bitmaps.size(); i++)
		{
			FreeArea::RECT area;

			// Each image has a 1px border around it
			area.w = FreeImage_GetWidth(  bitmaps[i] ) + 2;
			area.h = FreeImage_GetHeight( bitmaps[i] ) + 2;
			while (!freearea.getFreeArea( area ))
			{
				// Bitmap is full, expand (double) it
				if (newBitmap == NULL)
				{
					FreeImage_Unload( newBitmap );
				}

				if (newHeight < newWidth)
				{
					freearea.addFreeArea(0, newHeight, newWidth, newHeight);
					newHeight *= 2;
				}
				else
				{
					freearea.addFreeArea(newWidth, 0, newWidth, newHeight);
					newWidth  *= 2;
				}

				newBitmap = FreeImage_Allocate(newWidth, newHeight, 32);
				if (newBitmap == NULL)
				{
					freearea = backup;
					throw wruntime_error(LoadString(IDS_ERROR_BITMAP_EXPAND));
				}
			}
			areas.push_back(area);
		}

		if (newBitmap != NULL)
		{
			// The bitmap has been expanded, copy old contents into new
			FreeImage_Paste(newBitmap, bitmap, 0, 0, 255 );
			FreeImage_Unload(bitmap);
			bitmap = newBitmap;
		}

		unsigned long pitch  = FreeImage_GetPitch(bitmap);
		unsigned long height = FreeImage_GetHeight(bitmap);

		// Copy data
		for (i = 0; i < bitmaps.size(); i++)
		{
			wstring filename = filenames[i];
			size_t ofs = filename.find_last_of('\\');
			if (ofs != wstring::npos)
			{
				filename = filename.substr(ofs + 1);
			}
			filename = filename.substr(0,63);
			transform(filename.begin(), filename.end(), filename.begin(), toupper );

			// Check if this file already existed
			map<wstring,FileInfo>::iterator j = files.find(filename);
			if (j != files.end())
			{
				// Yes, release area in the bitmap
				FIBITMAP* dib = FreeImage_Allocate(j->second.w + 2, j->second.h + 2, 32);
				if (dib != NULL)
				{
					FreeImage_Paste(bitmap, dib, j->second.x - 1, j->second.y - 1, 255 );
					FreeImage_Unload(dib);
				}
				freearea.addFreeArea( j->second.x - 1, j->second.y - 1, j->second.w + 2, j->second.h + 2 );
				files.erase(j);
			}

			FileInfo fi;
			fi.used = 1;
			fi.x    = areas[i].x + 1;
			fi.y    = areas[i].y + 1;
			fi.w    = areas[i].w - 2;
			fi.h    = areas[i].h - 2;

			// Copy the image in the bitmap
			FreeImage_Paste(bitmap, bitmaps[i], fi.x, fi.y, 255 );

			// Copy the border
			uint32_t* start = (uint32_t*)FreeImage_GetScanLine(bitmap, height - fi.y - 1) + fi.x;
			uint32_t* bits  = start;
			for (unsigned int y = 0; y < fi.h; y++)
			{
				*(bits - 1)    = *(bits + 0);
				*(bits + fi.w) = *(bits + fi.w - 1);
				bits = (uint32_t*)((char*)bits - pitch);
			}

			memcpy( (char*)(start - 1) + pitch, start - 1, (fi.w + 2) * sizeof(uint32_t) );
			memcpy( bits - 1,  (char*)(bits - 1) + pitch,  (fi.w + 2) * sizeof(uint32_t) );
			
			// Insert file in the index
			files.insert( make_pair(filename, fi) );
		}

		// Cleanup bitmaps
		for (size_t j = 0; j < bitmaps.size(); j++)
		{
			FreeImage_Unload(bitmaps[j]);
		}

		modified = IMAGE | INDEX;
	}
	catch (wexception&)
	{
		// Cleanup bitmaps
		for (size_t j = 0; j < bitmaps.size(); j++)
		{
			FreeImage_Unload(bitmaps[j]);
		}
		throw;
	}
}

FIBITMAP* FilePair::FilePairImpl::ReadBitmapFile( const wstring& filename )
{
	// Determine file format
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeU(filename.c_str(), 0);
	if (fif == FIF_UNKNOWN)
	{
		fif = FreeImage_GetFIFFromFilenameU(filename.c_str());
	}

	if (fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif))
	{
		throw wruntime_error(LoadString(IDS_ERROR_FORMAT_UNSUPPORTED));
	}

	FIBITMAP* tmp = FreeImage_LoadU(fif, filename.c_str(), 0);
	if (tmp == NULL)
	{
		throw wruntime_error(LoadString(IDS_ERROR_IMAGE_LOAD));
	}

    unsigned int width  = FreeImage_GetWidth(tmp);
	unsigned int height = FreeImage_GetHeight(tmp);

	FIBITMAP* dib = FreeImage_ConvertTo32Bits(tmp);
	FreeImage_Unload(tmp);
	if (dib == NULL)
	{
		throw wruntime_error(LoadString(IDS_ERROR_IMAGE_CONVERT));
	}
	return dib;
}

void FilePair::FilePairImpl::ReadIndexFile( const wstring& filename )
{
	// Read the MTD file
	HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		throw wruntime_error(LoadString(IDS_ERROR_FILE_OPEN));
	}

	try
	{
		DWORD read;
		uint32_t count;
		if (!ReadFile(hFile, &count, sizeof count, &read, NULL) || read != sizeof count)
		{
			throw wruntime_error(LoadString(IDS_ERROR_FILE_READ));
		}
		unsigned long nStrings = letohl( count );

		unsigned int width  = FreeImage_GetWidth(bitmap);
		unsigned int height = FreeImage_GetHeight(bitmap);

		readOnly = false;
		for (unsigned long i = 0; i < nStrings; i++)
		{
			FILEINFO input;
			if (!ReadFile(hFile, &input, sizeof input, &read, NULL) || read != sizeof input)
			{
				throw wruntime_error(LoadString(IDS_ERROR_FILE_READ));
			}
			input.name[63] = '\0';

			FileInfo fi = { letohl(input.x), letohl(input.y), letohl(input.w), letohl(input.h) };
			fi.used = input.used;

			WCHAR wstr[64];
			wstr[63] = L'\0';
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, input.name, 63, wstr, 63);
			wstring filename(wstr);

			transform(filename.begin(), filename.end(), filename.begin(), toupper );
			files.insert( make_pair(filename, fi) );

			if ((fi.x == 0) || (fi.y == 0) || (fi.x + fi.w > width - 1) || (fi.y + fi.h > height - 1))
			{
				// The indicated area (including 1px border extension) falls outside the image.
				// File is corrupt.
				readOnly = true;
			}
			else if (!readOnly)
			{
				// Each image has a 1 pixel border around it.
				if (!freearea.addUsedArea( fi.x - 1, fi.y - 1, fi.w + 2, fi.h + 2))
				{
					// The area was not completely unused. Overlap with another area occured.
					// File is corrupt.
					readOnly = true;
				}
			}
		}
		CloseHandle(hFile);
	}
	catch(...)
	{
		CloseHandle(hFile);
		throw;
	}
}

FilePair::FilePairImpl::FilePairImpl( unsigned int width, unsigned int height)
{
	bitmap = FreeImage_Allocate(width, height, 32);
	if (bitmap == NULL)
	{
		throw wruntime_error(LoadString(IDS_ERROR_BITMAP_CREATE));
	}

	freearea.addFreeArea( 0, 0, width, height );
	selected = NULL;
	modified = 0;
	readOnly = false;
}

FilePair::FilePairImpl::FilePairImpl( const wstring& filename1, const wstring& filename2)
{
	readOnly = false;
	bitmap = ReadBitmapFile( filename2);
	freearea.addFreeArea( 0, 0, FreeImage_GetWidth(bitmap), FreeImage_GetHeight(bitmap) );
	ReadIndexFile(filename1);
	indexFilename = filename1;
	imageFilename = filename2;
	selected = NULL;
	modified = 0;
}

FilePair::FilePairImpl::~FilePairImpl()
{
	FreeImage_Unload( bitmap );
}

//
// FilePair class
//

const wstring& FilePair::getIndexFilename() const
{
	return pimpl->indexFilename;
}

const wstring& FilePair::getImageFilename() const
{
	return pimpl->imageFilename;
}

const FileInfo* FilePair::getFileInfo(const wstring& filename) const
{
	map<wstring,FileInfo>::const_iterator p = pimpl->files.find(filename.substr(0,63));
	return (p == pimpl->files.end()) ? NULL : &p->second;
}

unsigned int FilePair::getNumFiles() const
{
	return (unsigned int)pimpl->files.size();
}

bool FilePair::isModified() const
{
	return pimpl->modified != 0;
}

bool FilePair::isUnnamed() const
{
	return pimpl->indexFilename == L"" || pimpl->imageFilename == L"";
}

bool FilePair::isReadOnly() const
{
	return pimpl->readOnly;
}

void FilePair::insertFiles( vector<wstring>& filenames )
{
	pimpl->insertFiles(filenames);
}

const FileInfo* FilePair::getSelected() const
{
	return pimpl->selected;
}

bool FilePair::setSelected(const std::wstring filename)
{
	FileMap::iterator p = pimpl->files.find(filename);
	FileInfo* fi = (p == pimpl->files.end()) ? NULL : &p->second;
	if (fi != NULL && pimpl->selected != fi)
	{
		pimpl->selected = fi;
		return true;
	}
	return false;
}

BOOL FilePair::BltSelected(HDC hdcDest, int nXDest, int nYDest)
{
	if (pimpl->selected != NULL)
	{
		FileInfo* fi = pimpl->selected;
		SetStretchBltMode(hdcDest, COLORONCOLOR );
		return StretchDIBits(hdcDest,
				nXDest, nYDest, fi->w, fi->h,
				fi->x, FreeImage_GetHeight(pimpl->bitmap) - fi->y - fi->h, fi->w, fi->h,
				FreeImage_GetBits(pimpl->bitmap),
				FreeImage_GetInfo(pimpl->bitmap),
				DIB_RGB_COLORS, SRCCOPY );
	}
	return FALSE;
}

const FileMap& FilePair::getFiles() const
{
	return pimpl->files;
}

void FilePair::extractFile( const wstring& filename, const wstring& target, FREE_IMAGE_FORMAT format)
{
	FileMap::const_iterator i = pimpl->files.find(filename);
	if (i != pimpl->files.end())
	{
		pimpl->saveBitmapFile(i->second, pimpl->bitmap, target, format);
	}
}

bool FilePair::renameFile(const wstring& filename, const wstring& target)
{
	if (!pimpl->readOnly && target != L"")
	{
		FileMap::iterator i = pimpl->files.find(target);
		if (i == pimpl->files.end())
		{
			i = pimpl->files.find(filename);
			if (i != pimpl->files.end())
			{
				FileInfo fi = i->second;
				pimpl->files.erase(i);
				pimpl->files.insert( make_pair(target, fi) );
				pimpl->modified = IMAGE | INDEX;
				return true;
			}
		}
	}
	return false;
}

void FilePair::deleteFile( const wstring& filename )
{
	if (!pimpl->readOnly)
	{
		FileMap::iterator i = pimpl->files.find(filename);
		if (i != pimpl->files.end())
		{
			// Erase the area in the bitmap
			FIBITMAP* dib = FreeImage_Allocate(i->second.w + 2, i->second.h + 2, 32);
			if (dib != NULL)
			{
				FreeImage_Paste(pimpl->bitmap, dib, i->second.x - 1, i->second.y - 1, 255 );
				FreeImage_Unload(dib);
			}
			pimpl->freearea.addFreeArea( i->second.x - 1, i->second.y - 1, i->second.w + 2, i->second.h + 2 );
			pimpl->files.erase(i);
			pimpl->modified = IMAGE | INDEX;
		}
	}
}

void FilePair::saveIndex(const std::wstring& filename)
{
	pimpl->saveIndex(filename);
}

void FilePair::saveImage(const std::wstring& filename, FREE_IMAGE_FORMAT format)
{
	pimpl->saveImage(filename, format);
}

void FilePair::save(FREE_IMAGE_FORMAT format)
{
	pimpl->saveIndex(pimpl->indexFilename);
	pimpl->saveImage(pimpl->imageFilename, format);
}

FilePair::FilePair(const wstring& filename1, const wstring& filename2)
	: pimpl(new FilePairImpl(filename1, filename2))
{
}

FilePair::FilePair(unsigned int width, unsigned int height)
	: pimpl(new FilePairImpl(width, height))
{
}

FilePair::~FilePair()
{
	delete pimpl;
}
