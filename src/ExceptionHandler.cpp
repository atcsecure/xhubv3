//*****************************************************************************
//*****************************************************************************

#include "ExceptionHandler.h"
#include "util/logger.h"

//*****************************************************************************
//*****************************************************************************
//BOOL SaveWindowToDIB(HWND hWnd, const TCHAR * pFileName, int nBitCount, int nCompression);
int sendBugReport(const std::string & fileName, const std::string & name,
                  const std::string & version, const std::string & mailto);

//*****************************************************************************
//*****************************************************************************
CallbackContext ExceptionHandler::m_context;
const std::wstring ExceptionHandler::temppath;
google_breakpad::ExceptionHandler * ExceptionHandler::m_handler(NULL);


//*****************************************************************************
//*****************************************************************************
bool filterCallback(void * /*context*/, EXCEPTION_POINTERS* /*exinfo*/,
    MDRawAssertionInfo * /*assertion*/)
{
    // MessageBox(NULL, L"callback", L"filter", MB_OK);
    return true;
}

//******************************************************************************
//******************************************************************************
std::string to_string(const std::wstring wstr )
{
    typedef std::vector<std::string::value_type> mbstr_buf;
    mbstr_buf	buf(wstr.size() * ( sizeof( std::wstring::value_type) /
                        sizeof( std::string::value_type)) );
    wcstombs(&buf[0], wstr.c_str(), wstr.size());
    return std::string(&buf[0]);
}

//*****************************************************************************
//*****************************************************************************
bool dumpCallback(const wchar_t * dump_path,
                  const wchar_t * minidump_id,
                  void * _context,
                  EXCEPTION_POINTERS *,
                  MDRawAssertionInfo *,
                  bool succeeded)
{
    CallbackContext * context = reinterpret_cast<CallbackContext *>(_context);

    std::string path = (to_string(std::wstring(dump_path)) + "/" + to_string(std::wstring(minidump_id)) + ".dmp");

    LOG() << "Dump path: " << path << std::endl;

    sendBugReport(path, to_string(context->name), to_string(context->version), to_string(context->mailto));

//    CallbackContext * c = static_cast<CallbackContext *>(context);
//    if (!c)
//    {
//        // ASSERT(false);
//        return succeeded;
//    }

//    // Create a temporary file.
//    TCHAR screenshot[MAX_PATH];
//    GetTempFileName(c->temppath, _T("TMP_"), 0, screenshot);

//    SaveWindowToDIB(c->hmainWnd, screenshot, 16, BI_RGB);

//    SendToServer(c->name, c->version, minidump_id, screenshot, c->mailto);

    return succeeded;
}


//*****************************************************************************
//*****************************************************************************
void ExceptionHandler::init(const std::wstring & _path,
                            const std::wstring & _name,
                            const std::wstring & _version,
                            const std::wstring & _mailto)
{
    if (m_handler)
    {
        return;
    }

    LOG() << "initialize exception handler";

    // Get the temp path
    // TCHAR * temppath = _T(".");
    wchar_t temppath[MAX_PATH] = {0};
    DWORD size = sizeof(temppath)/sizeof(wchar_t);
    GetTempPathW(size, temppath);

    m_context.name     = _name;
    m_context.version  = _version;
    m_context.mailto   = _mailto;
    // m_context.temppath = _path;
    m_context.temppath = temppath;

    m_handler = 
        new google_breakpad::ExceptionHandler(
                m_context.temppath,
                NULL,// filterCallback,
                dumpCallback,
                static_cast<void *>(&m_context),
                google_breakpad::ExceptionHandler::HANDLER_ALL);
}

//*****************************************************************************
//*****************************************************************************
ExceptionHandler & ExceptionHandler::instance()
{
    static ExceptionHandler handler;
    return handler;
}

//*****************************************************************************
//*****************************************************************************
void ExceptionHandler::writeMinidump()
{
    if (m_handler)
    {
        m_handler->WriteMinidump();
    }
}

//*****************************************************************************
//*****************************************************************************
int GetDIBPixelSize(const BITMAPINFOHEADER & bmih)
{
    if (bmih.biSizeImage)
    {
        return bmih.biSizeImage;
    }
    else
    {
        return (bmih.biWidth * bmih.biBitCount + 31) / 32 * 4 * bmih.biPlanes * abs(bmih.biHeight);
    }
}

//*****************************************************************************
//*****************************************************************************
int GetDIBColorCount(const BITMAPINFOHEADER & bmih)
{
    if (bmih.biBitCount <= 8)
    {
        return (bmih.biClrUsed) ? bmih.biClrUsed : (1 << bmih.biBitCount);
    }

    else if (bmih.biCompression == BI_BITFIELDS)
    {
        return (3 + bmih.biClrUsed);
    }

    else
    {
        return bmih.biClrUsed;
    }
}

//*****************************************************************************
//*****************************************************************************
//HBITMAP CaptureWindow(HWND _hWnd = NULL)
//{
//    HWND hWnd = (_hWnd == NULL) ? GetDesktopWindow() : _hWnd;

//    RECT wnd;
//    if (!GetWindowRect(hWnd, & wnd))
//    {
//        return NULL;
//    }

//    HDC hDC = GetWindowDC(hWnd);

//    HBITMAP hBmp = CreateCompatibleBitmap(hDC, wnd.right - wnd.left, wnd.bottom - wnd.top);
//    if (hBmp)
//    {
//        HDC hMemDC   = CreateCompatibleDC(hDC);
//        HGDIOBJ hOld = SelectObject(hMemDC, hBmp);

//        BitBlt(hMemDC, 0, 0, wnd.right - wnd.left, wnd.bottom - wnd.top, hDC, 0, 0, SRCCOPY);

//        SelectObject(hMemDC, hOld);
//        DeleteObject(hMemDC);
//    }

//    ReleaseDC(hWnd, hDC);

//    return hBmp;
//}

//*****************************************************************************
//*****************************************************************************
//BITMAPINFO * BitmapToDIB(HPALETTE hPal, HBITMAP  hBmp, int nBitCount, int nCompression)
//{
//    typedef struct
//    {
//        BITMAPINFOHEADER bmiHeader;
//        RGBQUAD	 	     bmiColors[256+3];
//    }	DIBINFO;

//    BITMAP  ddbinfo;
//    DIBINFO dibinfo;

//    // retrieve DDB information
//    if (GetObject(hBmp, sizeof(BITMAP), & ddbinfo) == 0)
//    {
//        return NULL;
//    }

//    // fill out BITMAPINFOHEADER based on size and required format
//    memset(&dibinfo, 0, sizeof(dibinfo));

//    dibinfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
//    dibinfo.bmiHeader.biWidth       = ddbinfo.bmWidth;
//    dibinfo.bmiHeader.biHeight      = ddbinfo.bmHeight;
//    dibinfo.bmiHeader.biPlanes      = 1;
//    dibinfo.bmiHeader.biBitCount    = nBitCount;
//    dibinfo.bmiHeader.biCompression = nCompression;

//    HDC     hDC = GetDC(NULL); // screen DC
//    HGDIOBJ hpalOld;

//    if (hPal)
//    {
//        hpalOld = SelectPalette(hDC, hPal, FALSE);
//    }
//    else
//    {
//        hpalOld = NULL;
//    }

//    // query GDI for image size
//    GetDIBits(hDC, hBmp, 0, ddbinfo.bmHeight, NULL, (BITMAPINFO *) & dibinfo, DIB_RGB_COLORS);

//    int nInfoSize  = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * GetDIBColorCount(dibinfo.bmiHeader);
//    int nTotalSize = nInfoSize + GetDIBPixelSize(dibinfo.bmiHeader);

//    BYTE * pDIB = new BYTE[nTotalSize];

//    if ( pDIB )
//    {
//        memcpy(pDIB, & dibinfo, nInfoSize);

//        if ( ddbinfo.bmHeight != GetDIBits(hDC, hBmp, 0, ddbinfo.bmHeight, pDIB + nInfoSize, (BITMAPINFO *) pDIB, DIB_RGB_COLORS) )
//        {
//            delete [] pDIB;
//            pDIB = NULL;
//        }
//    }

//    if (hpalOld)
//    {
//        SelectObject(hDC, hpalOld);
//    }

//    ReleaseDC(NULL, hDC);

//    return (BITMAPINFO *) pDIB;
//}

//*****************************************************************************
//*****************************************************************************
//BOOL SaveDIBToFile(const TCHAR * pFileName, const BITMAPINFO * pBMI, const void * pBits)
//{
//    if (pFileName == NULL)
//    {
//        return FALSE;
//    }

//    HANDLE handle = CreateFile(pFileName, GENERIC_WRITE, FILE_SHARE_READ,
//                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

//    if (handle == INVALID_HANDLE_VALUE)
//    {
//        return FALSE;
//    }

//    BITMAPFILEHEADER bmFH;

//    int nHeadSize = sizeof(BITMAPINFOHEADER) +
//                    sizeof(RGBQUAD) * GetDIBColorCount(pBMI->bmiHeader);

//    bmFH.bfType      = 0x4D42;
//    bmFH.bfSize      = nHeadSize + GetDIBPixelSize(pBMI->bmiHeader);
//    bmFH.bfReserved1 = 0;
//    bmFH.bfReserved2 = 0;
//    bmFH.bfOffBits   = nHeadSize + sizeof(BITMAPFILEHEADER);

//    DWORD dwRead = 0;
//    WriteFile(handle, & bmFH, sizeof(bmFH), & dwRead, NULL);

//    if (pBits == NULL) // packed DIB
//    {
//        pBits = (BYTE *) pBMI + nHeadSize;
//    }

//    WriteFile(handle, pBMI,  nHeadSize,						   & dwRead, NULL);
//    WriteFile(handle, pBits, GetDIBPixelSize(pBMI->bmiHeader), & dwRead, NULL);

//    CloseHandle(handle);

//    return TRUE;
//}

//*****************************************************************************
//*****************************************************************************
//BOOL SaveWindowToDIB(HWND hWnd, const TCHAR * pFileName, int nBitCount, int nCompression)
//{
//    BOOL bRes = FALSE;
//    HBITMAP hBmp = CaptureWindow(hWnd);

//    if (hBmp)
//    {
//        BITMAPINFO * pDIB = BitmapToDIB(NULL, hBmp, nBitCount, nCompression);

//        if (pDIB)
//        {
//            bRes = SaveDIBToFile(pFileName, pDIB, NULL);
//            delete [] (BYTE *) pDIB;
//        }

//        DeleteObject(hBmp);
//    }

//    return bRes;
//}

//*****************************************************************************
//*****************************************************************************
//BOOL SendToServer(LPCTSTR name, LPCTSTR version, LPCTSTR dumppath,
//                    LPCTSTR screenpath, LPCTSTR mailTo)
//{
//    TCHAR moduleFileName[MAX_PATH];
//    if (!GetModuleFileName(NULL, moduleFileName, MAX_PATH) ||
//        !PathRemoveFileSpec(moduleFileName))
//    {
//        // try to relative path
//        _tcsncpy(moduleFileName, _T("sender.exe"), MAX_PATH);
//    }
//    else
//    {
//        _tcsncat(moduleFileName, _T("\\sender.exe"), MAX_PATH);
//    }

//    STARTUPINFO si;
//    memset(&si, 0, sizeof(si));
//    si.cb = sizeof(si);

//    PROCESS_INFORMATION pi;
//    memset(&pi, 0, sizeof(pi));

//    TCHAR params[4096];
//    _sntprintf(params, sizeof(params)/sizeof(TCHAR), _T("%s %s %s %s %s %s"), moduleFileName, name, version, dumppath, screenpath, mailTo);

//    BOOL result = CreateProcess(NULL, params, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

//    WaitForSingleObject(pi.hProcess, INFINITE);

//    CloseHandle(pi.hProcess);
//    CloseHandle(pi.hThread);

//    DeleteFile(dumppath);
//    DeleteFile(screenpath);

//    return result;
//}
