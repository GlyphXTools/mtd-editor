//
// This file contains the entry point and the GUI interaction
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <cderr.h>
#include <shlobj.h>
#include <afxres.h>
#include "resource.h"

#include <algorithm>
#include <sstream>
#include <vector>
#include "exceptions.h"
#include "filepair.h"
#include "Utils.h"
using namespace std;

// New images are always this size
static const unsigned int DEFAULT_WIDTH  = 256;
static const unsigned int DEFAULT_HEIGHT = 256;

typedef vector<pair<wstring, pair<wstring, FREE_IMAGE_FORMAT> > > ExtensionMap;

struct ApplicationInfo
{
	HINSTANCE  hInstance;
    HWND       hMainWnd;
	HWND       hListView;
	HWND       hGroupBox;
	HWND       hRenderWnd;
	HWND       hLabels[4];
    bool       editingLabel;

	ExtensionMap SupportedExtsRead;
	ExtensionMap SupportedExtsWrite;

	FilePair* openfile;

	ApplicationInfo()
	{
        hMainWnd     = NULL;
		openfile     = NULL;
        editingLabel = false;
	}

	~ApplicationInfo()
	{
		delete openfile;
        DestroyWindow(hMainWnd);
	}
};

static wstring GetFilterString( const ExtensionMap& extensions )
{
	wstring filter;
	for (ExtensionMap::const_iterator i = extensions.begin(); i != extensions.end(); i++)
	{
		filter += wstring(i->first.c_str(), i->first.length() + 1);
		filter += wstring(i->second.first.c_str(), i->second.first.length() + 1);
	}
	filter += wstring(L"\0", 1);
	return filter;
}

static void SetWindowTitle( HWND hWnd, const wstring& name )
{
	// Enter name in window title
	TCHAR text[MAX_PATH];
	GetWindowText(hWnd, text, MAX_PATH);
	wstring title = text;
	size_t ofs = title.find_first_of('-');
	if (ofs != wstring::npos) title = title.substr(0, ofs + 1);
	else title = title + L" - ";
	title += L" [" + name + L"]";
	SetWindowText(hWnd, title.c_str() );
}

static bool DoSaveFile( ApplicationInfo* info, bool saveas = false )
{
	if (info->openfile->isUnnamed() || saveas)
	{
		// Query for save locations
		TCHAR filename1[MAX_PATH]; wcsncpy_s(filename1, MAX_PATH, info->openfile->getIndexFilename().c_str(), MAX_PATH );
		TCHAR filename2[MAX_PATH]; wcsncpy_s(filename2, MAX_PATH, info->openfile->getImageFilename().c_str(), MAX_PATH );

        wstring filter = LoadString(IDS_FILES_MTD) + wstring(L" (*.mtd)\0*.MTD\0", 15)
                       + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

        OPENFILENAME ofn;
		memset(&ofn, 0, sizeof(OPENFILENAME));
		ofn.lStructSize  = sizeof(OPENFILENAME);
		ofn.hwndOwner    = info->hMainWnd;
		ofn.hInstance    = info->hInstance;
        ofn.lpstrFilter  = filter.c_str();
		ofn.nFilterIndex = 1;
		ofn.lpstrDefExt  = L"mtd";
		ofn.lpstrFile    = filename1;
		ofn.nMaxFile     = MAX_PATH;
		ofn.Flags        = OFN_PATHMUSTEXIST | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
		if (GetSaveFileName( &ofn ) == 0)
		{
			return false;
		}

		try
		{
			info->openfile->saveIndex(filename1);
		}
		catch (wexception& e)
		{
			MessageBox(info->hMainWnd, e.what(), NULL, MB_OK | MB_ICONHAND);
			return false;
		}

		filter = GetFilterString( info->SupportedExtsWrite );
		memset(&ofn, 0, sizeof(OPENFILENAME));
		ofn.lStructSize  = sizeof(OPENFILENAME);
		ofn.hwndOwner    = info->hMainWnd;
		ofn.hInstance    = info->hInstance;
        ofn.lpstrFilter  = filter.c_str();
		ofn.nFilterIndex = (DWORD)info->SupportedExtsWrite.size() - 1;	// Second to last "All image files"
		ofn.lpstrDefExt  = L"tga";
		ofn.lpstrFile    = filename2;
		ofn.nMaxFile     = MAX_PATH;
		ofn.Flags        = OFN_PATHMUSTEXIST | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
		if (GetSaveFileName( &ofn ) == 0)
		{
			return false;
		}

		try
		{
			info->openfile->saveImage(filename2, info->SupportedExtsWrite[ofn.nFilterIndex - 1].second.second);
		}
		catch (wexception& e)
		{
			MessageBox(info->hMainWnd, e.what(), NULL, MB_OK | MB_ICONHAND);
			return false;
		}
		SetWindowTitle( info->hMainWnd, filename1 );
	}
	else
	{
		info->openfile->save(FIF_UNKNOWN);
	}
	return true;
}

static bool DoCheckCloseFile( ApplicationInfo* info )
{
	if (info->openfile != NULL && !info->openfile->isReadOnly() && info->openfile->isModified())
	{
        switch (MessageBox(info->hMainWnd, LoadString(IDS_QUERY_SAVE_MODIFICATIONS).c_str(), LoadString(IDS_WARNING).c_str(), MB_YESNOCANCEL | MB_ICONWARNING ))
		{
			case IDYES: return DoSaveFile( info );
			case IDNO:  return true;
		}
		return false;
	}
	return true;
}

static void DoCloseFile( ApplicationInfo* info )
{
	if (info->openfile != NULL)
	{
		// Clear the file
		delete info->openfile;
		info->openfile = NULL;

		// Reset the GUI
		ListView_DeleteAllItems(info->hListView);

		for (int i = 0; i < 4; i++)
		{
			SetWindowText(info->hLabels[i], L"" );
		}

		ShowWindow(info->hRenderWnd, SW_HIDE);
		InvalidateRect(info->hRenderWnd, NULL, TRUE);
		InvalidateRect(info->hMainWnd, NULL, TRUE);
		UpdateWindow(info->hRenderWnd);
		UpdateWindow(info->hMainWnd);

		// Enter name in window title
		SetWindowTitle(info->hMainWnd, LoadString(IDS_UNNAMED));

		// Disable menu items
		HMENU hMenuBar = GetMenu(info->hMainWnd);
		EnableMenuItem( GetSubMenu(hMenuBar, 0), ID_FILE_SAVE,        MF_BYCOMMAND );
		EnableMenuItem( GetSubMenu(hMenuBar, 0), ID_FILE_SAVEAS,      MF_BYCOMMAND );
		EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_INSERTFILE,  MF_BYCOMMAND );
		EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_EXTRACTFILE, MF_BYCOMMAND | MF_GRAYED );
		EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_RENAMEFILE,  MF_BYCOMMAND | MF_GRAYED );
		EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_DELETEFILE,  MF_BYCOMMAND | MF_GRAYED );
		DrawMenuBar(info->hMainWnd);

		// Enable Edit Labels ability again
		SetWindowLong(info->hListView, GWL_STYLE, GetWindowLong(info->hListView, GWL_STYLE) | LVS_EDITLABELS);
	}
}

static void DoNewFile( ApplicationInfo* info )
{
	if (DoCheckCloseFile(info))
	{
		DoCloseFile(info);
		info->openfile = new FilePair(DEFAULT_WIDTH, DEFAULT_HEIGHT );
	}
}

static void DoOpenFile( ApplicationInfo* info )
{
	TCHAR filename1[MAX_PATH]; filename1[0] = L'\0';
	TCHAR filename2[MAX_PATH]; filename2[0] = L'\0';

    wstring filter = LoadString(IDS_FILES_MTD) + wstring(L" (*.mtd)\0*.MTD\0", 15)
                   + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

    // Query for the index file
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize  = sizeof(OPENFILENAME);
	ofn.hwndOwner    = info->hMainWnd;
	ofn.hInstance    = info->hInstance;
	ofn.lpstrFilter  = filter.c_str();
	ofn.nFilterIndex = 1;
	ofn.lpstrFile    = filename1;
	ofn.nMaxFile     = MAX_PATH;
	ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileName( &ofn ) == 0)
	{
		return;
	}
	
	// Query for the image file
	filter = GetFilterString( info->SupportedExtsRead );
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize  = sizeof(OPENFILENAME);
	ofn.hwndOwner    = info->hMainWnd;
	ofn.hInstance    = info->hInstance;
	ofn.lpstrFilter  = filter.c_str();
	ofn.nFilterIndex = (DWORD)info->SupportedExtsRead.size() - 1;	// Second to last "All image files"
	ofn.lpstrFile    = filename2;
	ofn.nMaxFile     = MAX_PATH;
	ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileName( &ofn ) == 0)
	{
		return;
	}

	try
	{
		FilePair* old = info->openfile;
		info->openfile = new FilePair(filename1, filename2);
		delete old;
	}
	catch (wexception& e)
	{
		MessageBox( info->hMainWnd, e.what(), NULL, MB_OK | MB_ICONERROR );
		return;
	}

	bool readonly = info->openfile->isReadOnly();

	// Hide render window
	ShowWindow(info->hRenderWnd, SW_HIDE);
	InvalidateRect(info->hMainWnd, NULL, TRUE );
	UpdateWindow(info->hMainWnd);

	// Enter name in window title
	SetWindowTitle(info->hMainWnd, info->openfile->getIndexFilename() );

	// Reset the listbox
	ListView_DeleteAllItems(info->hListView);
	const FileMap& files = info->openfile->getFiles();
	for (FileMap::const_iterator i = files.begin(); i != files.end(); i++)
	{
		LVITEM item;
		item.mask     = LVIF_TEXT;
		item.pszText  = (TCHAR*)i->first.c_str();
		item.iItem    = 0;
		item.iSubItem = 0;
		ListView_InsertItem(info->hListView, &item);
	}

	// Enable/disable menu items
	HMENU hMenuBar = GetMenu(info->hMainWnd);
	EnableMenuItem( GetSubMenu(hMenuBar, 0), ID_FILE_SAVE,        MF_BYCOMMAND | (readonly ? MF_GRAYED : 0) );
	EnableMenuItem( GetSubMenu(hMenuBar, 0), ID_FILE_SAVEAS,      MF_BYCOMMAND | (readonly ? MF_GRAYED : 0) );
	EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_INSERTFILE,  MF_BYCOMMAND | (readonly ? MF_GRAYED : 0) );
	EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_EXTRACTFILE, MF_BYCOMMAND | MF_GRAYED );
	EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_RENAMEFILE,  MF_BYCOMMAND | MF_GRAYED );
	EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_DELETEFILE,  MF_BYCOMMAND | MF_GRAYED );
	DrawMenuBar(info->hMainWnd);

	if (readonly)
	{
		// Clear Edit Labels ability in read only mode
		SetWindowLong(info->hListView, GWL_STYLE, GetWindowLong(info->hListView, GWL_STYLE) & ~LVS_EDITLABELS);

		// Notify user of read-only-ness
		MessageBox( info->hMainWnd, LoadString(IDS_ERROR_CORRUPT_ARCHIVE).c_str(), LoadString(IDS_WARNING).c_str(), MB_OK | MB_ICONWARNING );
	}

	SetFocus(info->hListView);
}

static void DoInsertFiles(ApplicationInfo* info)
{
	static const size_t BUFFER_SIZE = 1048576;

	wstring filter = GetFilterString( info->SupportedExtsRead );

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize  = sizeof(OPENFILENAME);
	ofn.hwndOwner    = info->hMainWnd;
	ofn.hInstance    = info->hInstance;
	ofn.lpstrFilter  = filter.c_str();
	ofn.nFilterIndex = (DWORD)info->SupportedExtsRead.size() - 1;	// Second to last "All image files"
	ofn.lpstrFile    = new TCHAR[BUFFER_SIZE]; ofn.lpstrFile[0] = L'\0';
	ofn.nMaxFile     = BUFFER_SIZE - 1;
	ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	if (GetOpenFileName( &ofn ) == 0)
	{
		if (CommDlgExtendedError() == FNERR_BUFFERTOOSMALL)
		{
			MessageBox(NULL, LoadString(IDS_ERROR_FILE_COUNT).c_str(), NULL, MB_OK | MB_ICONERROR );
		}
		delete[] ofn.lpstrFile;
		return;
	}

	wstring directory = ofn.lpstrFile;
	vector<wstring> filenames;

	TCHAR* str = ofn.lpstrFile + wcslen(ofn.lpstrFile) + 1;
	while (*str != '\0')
	{
		filenames.push_back( directory + L"\\" + str );
		str += wcslen(str) + 1;
	}
	delete[] ofn.lpstrFile;

	if (filenames.size() == 0)
	{
		// User selected only one file, we interpreted it as the directory
		filenames.push_back( directory );
	}

	// Check uniqueness of filenames
	for (size_t i = 0; i < filenames.size(); i++)
	{
		// Uppercase filename
		size_t ofs  = filenames[i].find_last_of('\\');
		transform(filenames[i].begin(), filenames[i].end(), filenames[i].begin(), toupper);

		wstring name = filenames[i];
		if (ofs != wstring::npos)
		{
			name = filenames[i].substr(ofs + 1);
		}
		name = name.substr(0,63);

		if (info->openfile->getFileInfo(name) != NULL)
		{
			wstring message = LoadString(IDS_WARNING_INSERT_OVERWRITE, name.c_str());
			int ret = MessageBox(info->hMainWnd, message.c_str(), LoadString(IDS_TITLE_REPLACE).c_str(), MB_YESNOCANCEL | MB_ICONWARNING );
			if (ret == IDCANCEL)
			{
				return;
			}

			if (ret == IDNO)
			{
				filenames.erase( filenames.begin() + i);
				i--;
			}
		}
	}

	if (filenames.size() > 0)
	{
		try
		{
			info->openfile->insertFiles( filenames );

			int index = 0;
			for (size_t i = 0; i < filenames.size(); i++)
			{
				wstring filename = filenames[i];
				size_t ofs = filename.find_last_of(L'\\');
				if (ofs != wstring::npos)
				{
					filename = filename.substr(ofs + 1);
				}
				filename = filename.substr(0,63);

				LVFINDINFO lvfi;
				lvfi.flags = LVFI_STRING;
				lvfi.psz   = filename.c_str();
				if ((index = ListView_FindItem( info->hListView, -1, &lvfi)) == -1)
				{
					LVITEM item;
					item.mask     = LVIF_TEXT;
					item.pszText  = (TCHAR*)filename.c_str();
					item.iItem    = 0;
					item.iSubItem = 0;
					index = ListView_InsertItem(info->hListView, &item);
				}
			}

			// Focus last inserted
			ListView_SetItemState(info->hListView, index, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED );
			ListView_EnsureVisible(info->hListView, index, FALSE );
			SetFocus(info->hListView);
		}
		catch (wexception& e)
		{
			MessageBox(info->hMainWnd, e.what(), NULL, MB_OK | MB_ICONERROR );
		}
	}
}

static void DoExtractFiles(ApplicationInfo* info)
{
	// Get the selected files
	vector<wstring> files;

	int index = -1;
	while ((index = ListView_GetNextItem(info->hListView, index, LVNI_SELECTED )) != -1)
	{
		TCHAR text[MAX_PATH];
		ListView_GetItemText(info->hListView, index, 0, text, MAX_PATH );
		files.push_back( text );
	}

    const wstring title = LoadString(IDS_TITLE_EXTRACT_TARGET);

	// Query target directory
	BROWSEINFO bi;
	bi.hwndOwner      = info->hMainWnd;
	bi.pidlRoot       = NULL;
	bi.pszDisplayName = NULL;
	bi.lpszTitle      = title.c_str();
	bi.ulFlags        = BIF_RETURNONLYFSDIRS;
	bi.lpfn           = NULL;
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl == NULL)
	{
		return;
	}
	wstring directory;
	TCHAR path[MAX_PATH];
	if (SHGetPathFromIDList( pidl, path ))
	{
		directory = path;
	}
	CoTaskMemFree(pidl);

	// Check for overwrite
	for (size_t i = 0; i < files.size(); i++)
	{
		wstring filename = directory + L"\\" + files[i];
		HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
            wstring message = LoadString(IDS_WARNING_EXTRACT_OVERWRITE, files[i].c_str());
            int ret = MessageBox(info->hMainWnd, message.c_str(), LoadString(IDS_OVERWRITE_TITLE).c_str(), MB_YESNOCANCEL | MB_ICONWARNING);
			if (ret == IDCANCEL)
			{
				return;
			}
			if (ret == IDNO)
			{
				files.erase( files.begin() + i);
			}
			CloseHandle(hFile);
		}
	}

	if (files.size() > 0)
	{
		// Save them
		try
		{
			for (size_t i = 0; i < files.size(); i++)
			{
				info->openfile->extractFile( files[i], directory + L"\\" + files[i] );
			}
			MessageBox(info->hMainWnd, LoadString(IDS_INFO_EXTRACTED).c_str(), LoadString(IDS_INFORMATION).c_str(), MB_OK | MB_ICONINFORMATION );
		}
		catch (wexception& e)
		{
			MessageBox(info->hMainWnd, e.what(), NULL, MB_OK | MB_ICONERROR );
		}
	}
	else
	{
		MessageBox(info->hMainWnd, LoadString(IDS_INFO_NONE_EXTRACTED).c_str(), LoadString(IDS_INFORMATION).c_str(), MB_OK | MB_ICONINFORMATION );
	}
}

// Rename the focused file
static void DoRenameFile(ApplicationInfo* info, int iItem, const TCHAR* newText)
{
	wstring filename = newText;
	transform(filename.begin(), filename.end(), filename.begin(), toupper);

	TCHAR text[MAX_PATH];
	ListView_GetItemText(info->hListView, iItem, 0, text, MAX_PATH);
	info->openfile->renameFile(text, filename);

	ListView_DeleteItem(info->hListView, iItem);
	
	LVITEM item;
	item.mask     = LVIF_TEXT | LVIF_STATE;
	item.iItem    = 0;
	item.iSubItem = 0;
	item.state    = LVIS_SELECTED | LVIS_FOCUSED;
	item.pszText  = (TCHAR*)filename.c_str();

	int index = ListView_InsertItem(info->hListView, &item);
	ListView_EnsureVisible(info->hListView, index, FALSE );
}

// Show the selected file
static void DoSelect(ApplicationInfo* info, const wstring& name )
{
	if (info->openfile->setSelected( name ))
	{
		const FileInfo* fi = info->openfile->getFileInfo(name);
		int values[4] = { fi->x, fi->y, fi->w, fi->h };
		for (int i = 0; i < 4; i++)
		{
			wstringstream str;
			str << values[i];
			SetWindowText(info->hLabels[i], str.str().c_str() );
		}

		// Resize, invalidate and update affected windows
		SetWindowPos(info->hRenderWnd, NULL, 0, 0, fi->w, fi->h, SWP_NOZORDER | SWP_NOMOVE );
		ShowWindow(info->hRenderWnd, SW_SHOW);
		InvalidateRect(info->hRenderWnd, NULL, TRUE);
		InvalidateRect(info->hMainWnd, NULL, TRUE);
		UpdateWindow(info->hRenderWnd);
		UpdateWindow(info->hMainWnd);
	}
}

// Delete the selected files
static void DoDeleteFile(ApplicationInfo* info)
{
	int index;
	while ((index = ListView_GetNextItem(info->hListView, -1, LVNI_SELECTED)) != -1)
	{
		TCHAR text[MAX_PATH];
		ListView_GetItemText(info->hListView, index, 0, text, MAX_PATH );
		info->openfile->deleteFile( text );
		ListView_DeleteItem(info->hListView, index );
	}
}

INT_PTR CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ApplicationInfo* info = (ApplicationInfo*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
        case WM_INITDIALOG:
        {
            info = (ApplicationInfo*)lParam;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)info);

            info->hMainWnd   = hWnd;
            info->hRenderWnd = GetDlgItem(hWnd, IDC_RENDER1);
            info->hListView  = GetDlgItem(hWnd, IDC_LIST1);
            info->hGroupBox  = GetDlgItem(hWnd, IDC_GROUPBOX1);
            info->hLabels[0] = GetDlgItem(hWnd, IDC_STATIC_X);
            info->hLabels[1] = GetDlgItem(hWnd, IDC_STATIC_Y);
            info->hLabels[2] = GetDlgItem(hWnd, IDC_STATIC_WIDTH);
            info->hLabels[3] = GetDlgItem(hWnd, IDC_STATIC_HEIGHT);

            const wstring filename = LoadString(IDS_FILENAME);

		    LVCOLUMN column;
		    column.mask    = LVCF_WIDTH | LVCF_TEXT;
		    column.cx      = 390;
		    column.pszText = (LPWSTR)filename.c_str();
		    ListView_InsertColumn(info->hListView, 0, &column);
		    ListView_SetExtendedListViewStyle( info->hListView, LVS_EX_FULLROWSELECT );
            SetWindowLongPtr(info->hRenderWnd, GWLP_USERDATA, (LONG_PTR)info);
            break;
        }

		case WM_SETFOCUS:
			SetFocus(info->hListView);
			break;

		case WM_COMMAND:
			if (info != NULL)
			{
				// Menu and control notifications
				WORD code = HIWORD(wParam);
				WORD id   = LOWORD(wParam);
				if (lParam == 0)
				{
					// Menu or accelerator
					switch (id)
					{
                    case ID_FILE_NEW:
							DoNewFile(info);
							break;

						case ID_FILE_OPEN:
							if (DoCheckCloseFile(info))
							{
								DoOpenFile( info );
							}
							break;

						case ID_FILE_SAVE:
							if (!info->openfile->isReadOnly())
							{
								DoSaveFile(info);	
							}
							break;

						case ID_FILE_SAVEAS:
							if (!info->openfile->isReadOnly())
							{
								DoSaveFile(info, true);
							}
							break;

						case ID_FILE_EXIT:
							if (DoCheckCloseFile(info))
							{
								PostQuitMessage(0);
							}
							break;

						case ID_EDIT_SELECT_ALL:
							ListView_SetItemState(info->hListView, -1, LVIS_SELECTED, LVIS_SELECTED);
							break;

						case ID_EDIT_INSERTFILE:
							if (!info->openfile->isReadOnly())
							{
								DoInsertFiles(info);
							}
							break;

						case ID_EDIT_RENAMEFILE:
						{
							if (!info->openfile->isReadOnly())
							{
								int index = ListView_GetNextItem(info->hListView, -1, LVNI_SELECTED);
								if (index != -1)
								{
									ListView_EditLabel(info->hListView, index);
								}
							}
							break;
						}

						case ID_EDIT_EXTRACTFILE:
							DoExtractFiles(info);
							break;

						case ID_EDIT_DELETEFILE:
                            if (!info->openfile->isReadOnly())
							{
								DoDeleteFile(info);
							}
							break;

						case ID_HELP_ABOUT:
							string message = "Mega-Texture Editor, version 1.4.\nCopyright (C) 2008, Mike Lankamp\n\n";
                            message = message + "FreeImage " + FreeImage_GetVersion() + ":\n";
							message = message + FreeImage_GetCopyrightMessage();
							MessageBoxA(NULL, message.c_str(), "About", MB_OK | MB_ICONINFORMATION );
							break;
					}
				}
			}
			break;

		case WM_NOTIFY:
			if (info != NULL)
			{
				NMHDR* nmhdr = (NMHDR*)lParam;
				switch (nmhdr->code)
				{
					case NM_RCLICK:
					{
						// Show the popup menu on a right-click
						int index;
						if ((index = ListView_GetNextItem(info->hListView, -1, LVNI_SELECTED)) != -1)
						{
							POINT cursor;
							GetCursorPos(&cursor);
							TrackPopupMenu( GetSubMenu(GetMenu(info->hMainWnd), 1), TPM_LEFTALIGN | TPM_TOPALIGN, cursor.x, cursor.y, 0, info->hMainWnd, NULL); 
						}
					}

					case LVN_ITEMCHANGED:
					{
						// State of an item has changed
						NMLISTVIEW* nmlv = (NMLISTVIEW*)nmhdr;
						if (nmlv->uNewState & LVIS_FOCUSED)
						{
							TCHAR text[MAX_PATH];
							ListView_GetItemText(info->hListView, nmlv->iItem, 0, text, MAX_PATH);
							DoSelect( info, text );
						}

						if (nmlv->uChanged & LVIF_STATE)
						{
							unsigned long rstate = (nmlv->uNewState & LVIS_SELECTED) ? MF_ENABLED : MF_GRAYED;
							unsigned long wstate = (nmlv->uNewState & LVIS_SELECTED && !info->openfile->isReadOnly()) ? MF_ENABLED : MF_GRAYED;
							// Disable menu items
							HMENU hMenuBar = GetMenu(info->hMainWnd);
							EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_EXTRACTFILE, MF_BYCOMMAND | rstate );
							EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_RENAMEFILE,  MF_BYCOMMAND | wstate );
							EnableMenuItem( GetSubMenu(hMenuBar, 1), ID_EDIT_DELETEFILE,  MF_BYCOMMAND | wstate );
							DrawMenuBar(info->hMainWnd);
						}
						break;
					}

					case LVN_BEGINLABELEDIT:
                        info->editingLabel = true;
                        break;

					case LVN_ENDLABELEDIT:
					{
						// Label edit operation has finished. Update directory
						NMLVDISPINFO* nmdi = (NMLVDISPINFO*)nmhdr;
						if (nmdi->item.pszText != NULL)
						{
							DoRenameFile(info, nmdi->item.iItem, nmdi->item.pszText );
						}
                        info->editingLabel = false;
						break;
					}
				}
			}
			break;

		case WM_SIZE:
			if (info != NULL)
			{
				// Resize children as well
				RECT client, list, group;
				GetClientRect(hWnd, &client);
                GetWindowRect(info->hListView, &list);
                GetWindowRect(info->hGroupBox, &group);
                
                POINT size = {client.right, client.bottom};
                ClientToScreen(hWnd, &size);

                SetWindowPos(info->hListView, NULL, 0, 0, list.right - list.left,      size.y - list.top - 4,  SWP_NOMOVE | SWP_NOZORDER);
				SetWindowPos(info->hGroupBox, NULL, 0, 0, size.x     - group.left - 4, size.y - group.top - 4, SWP_NOMOVE | SWP_NOZORDER);
			}
			break;

		case WM_SIZING:
		{
			// Restrict window size
			const int MIN_WIDTH  = 650;
			const int MIN_HEIGHT = 350;

			RECT* rect = (RECT*)lParam;
			bool left  = (wParam == WMSZ_BOTTOMLEFT) || (wParam == WMSZ_LEFT) || (wParam == WMSZ_TOPLEFT);
			bool top   = (wParam == WMSZ_TOPLEFT)    || (wParam == WMSZ_TOP)  || (wParam == WMSZ_TOPRIGHT);
			if (rect->right - rect->left < MIN_WIDTH)
			{
				if (left) rect->left  = rect->right - MIN_WIDTH;
				else      rect->right = rect->left  + MIN_WIDTH;
			}
			if (rect->bottom - rect->top < MIN_HEIGHT)
			{
				if (top) rect->top    = rect->bottom - MIN_HEIGHT;
				else     rect->bottom = rect->top    + MIN_HEIGHT;
			}
			break;
		}

		case WM_CLOSE:
    		// Close if we can
			if (DoCheckCloseFile(info))
			{
				PostQuitMessage(0);
			}
			return 0;
	}

	return FALSE;
}

// Callback of the window that shows the currently selected picture
static LRESULT CALLBACK RenderWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ApplicationInfo* info = (ApplicationInfo*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_PAINT:
		{
			// On a paint, we simply blit the bitmap
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			int x      = ps.rcPaint.left;
			int y      = ps.rcPaint.top;
			int width  = ps.rcPaint.right  - ps.rcPaint.left;
			int height = ps.rcPaint.bottom - ps.rcPaint.top;
			if (info != NULL && info->openfile != NULL)
			{
				info->openfile->BltSelected(ps.hdc, 0, 0);
			}
			else
			{
				BitBlt(ps.hdc, x, y, width, height, NULL, 0, 0, BLACKNESS);
			}
			EndPaint(hWnd, &ps);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Main application
void main( ApplicationInfo* info, const vector<wstring>& argv )
{
	// Message loop
	ShowWindow(info->hMainWnd, SW_SHOW);
	HACCEL hAccel = LoadAccelerators( info->hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) != 0)
	{
        // Accelerators only matter when we're not editing a label
		if (info->editingLabel || !TranslateAccelerator(info->hMainWnd, hAccel, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

// Create the main window and its child windows
static bool InitializeUI( HINSTANCE hInstance )
{
	WNDCLASSEX wcx;
	wcx.cbSize        = sizeof wcx;
	wcx.style		  = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc	  = RenderWindowProc;
	wcx.cbClsExtra	  = 0;
	wcx.cbWndExtra    = 0;
	wcx.hInstance     = hInstance;
	wcx.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wcx.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName  = NULL;
	wcx.lpszClassName = L"MTDEditorRenderer";
	wcx.hIconSm       = NULL;
	if (RegisterClassEx(&wcx) == 0)
    {
        return false;
	}
    return true;
}

// Parse the command line into a argv-style vector
vector<wstring> ParseCommandLine()
{
	vector<wstring> argv;
	TCHAR* cmdline = GetCommandLine();

	bool quoted = false;
	wstring arg;
	for (TCHAR* p = cmdline; p == cmdline || *(p - 1) != '\0'; p++)
	{
		if (*p == L'\0' || (*p == L' ' && !quoted))
		{
			if (arg != L"")
			{
				argv.push_back(arg);
				arg = L"";
			}
		}
		else if (*p == L'"') quoted = !quoted;
		else arg += *p;
	}
	return argv;
}

// Get a list of filters that FreeImage supports
// Two lists are returned; one for reading and one for writing.
static void GetFilterStrings(ExtensionMap& readingExts, ExtensionMap& writingExts)
{
	wstring rexts, wexts;

	for (int i = 0; i < FreeImage_GetFIFCount(); i++)
	{
		FREE_IMAGE_FORMAT fif = (FREE_IMAGE_FORMAT)i;
	
		wstring desc, exts;
		wstring  extlist = AnsiToWide( FreeImage_GetFIFExtensionList(fif) );
		size_t ofs, pos = 0;
		do
		{
			ofs = extlist.find_first_of(',', pos);
			if (ofs == wstring::npos) ofs = extlist.length();
			wstring token = extlist.substr(pos, ofs - pos);
			desc = desc + L"*." + token;
			exts = exts + L"*." + token;
			if (ofs != extlist.length())
			{
				exts += L";";
				desc += L", ";
			}
			pos = ofs + 1;
		} while (ofs != extlist.length());

		// Check if this format can be read
		if (FreeImage_FIFSupportsReading(fif))
		{
			rexts += exts + L";";
			readingExts.push_back( make_pair(AnsiToWide(FreeImage_GetFIFDescription(fif)) + L" (" + desc + L")", make_pair(exts, fif)) );
		}

		// Check if this format can write 32-bit bitmaps
		if (FreeImage_FIFSupportsWriting(fif) && FreeImage_FIFSupportsExportType(fif, FIT_BITMAP) && FreeImage_FIFSupportsExportBPP(fif,32))
		{
			wexts += exts + L";";
			writingExts.push_back( make_pair(AnsiToWide(FreeImage_GetFIFDescription(fif)) + L" (" + desc + L")", make_pair(exts, fif)) );
		}
	}

    // Sort the supported formats
    sort(readingExts.begin(), readingExts.end());
    sort(writingExts.begin(), writingExts.end());

	// Also add "All ..." filters
	if (readingExts.size() > 0)
	{
		rexts = rexts.substr(0, rexts.length() - 1);
		readingExts.push_back( make_pair(LoadString(IDS_FILES_IMAGE), make_pair(rexts, FIF_UNKNOWN)) );
	}

	if (writingExts.size() > 0)
	{
		wexts = wexts.substr(0, wexts.length() - 1);
		writingExts.push_back( make_pair(LoadString(IDS_FILES_IMAGE), make_pair(wexts, FIF_UNKNOWN)) );
	}

	readingExts.push_back( make_pair(LoadString(IDS_FILES_ALL) + L" (*.*)", make_pair(L"*.*", FIF_UNKNOWN)) );
	writingExts.push_back( make_pair(LoadString(IDS_FILES_ALL) + L" (*.*)", make_pair(L"*.*", FIF_UNKNOWN)) );
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    FreeImage_Initialise();
	try
	{
		ApplicationInfo info;

        if (!InitializeUI(hInstance))
        {
            throw wruntime_error(LoadString(IDS_ERROR_UI_INITIALIZATION));
        }

		// Query FreeImage about its R/W capabilities and get filter strings from that
		GetFilterStrings(info.SupportedExtsRead, info.SupportedExtsWrite);

        // Create the main window
        info.hInstance = hInstance;
        HWND hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_MAINWINDOW), NULL, MainWindowProc, (LPARAM)&info);
		
		// Create a blank file
		DoNewFile(&info);

		main( &info, ParseCommandLine() );
	}
	catch (wexception& e)
	{
		MessageBox(NULL, e.what(), NULL, MB_OK );
	}
	catch (exception& e)
	{
		MessageBoxA(NULL, e.what(), NULL, MB_OK );
	}
    FreeImage_DeInitialise();
	return 0;
}