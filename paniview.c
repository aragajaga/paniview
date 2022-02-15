#include "precomp.h"
#include "resource.h"

#include "dlnklist.h"

#define WM_SENDNUDES  WM_USER + 1
#define WM_FILENEXT   WM_USER + 2
#define WM_FILEPREV   WM_USER + 3
#define WM_ZOOMOUT    WM_USER + 4
#define WM_ZOOMIN     WM_USER + 5
#define WM_ACTUALSIZE WM_USER + 6
#define WM_FIT        WM_USER + 7

#define SAFE_RELEASE(obj) \
if (obj) { \
  ((IUnknown *)(obj))->lpVtbl->Release((IUnknown *)(obj)); \
  (obj) = NULL; \
}

#ifndef ASSERT
#ifndef NDEBUG
#define ASSERT(b) \
do { \
  if (!(b)) { \
    OutputDebugString(L"Assert: " #b "\n"); \
  } \
} while(0)
#else
#define ASSERT(b)
#endif
#endif

const FLOAT DEFAULT_DPI = 96.f;

typedef struct _tagMAINFRAMEDATA {
  HWND hRenderer;
  HWND hToolbar;
} MAINFRAMEDATA, *LPMAINFRAMEDATA;

typedef struct _tagRENDERCTLDATA {

  ID2D1Factory *m_pD2DFactory;
  ID2D1HwndRenderTarget *m_pRenderTarget;
  ID2D1Bitmap *m_pD2DBitmap;
  IWICImagingFactory *m_pIWICFactory;
  IWICFormatConverter *m_pConvertedSourceBitmap;

  ID2D1SolidColorBrush *m_pLightSlateGrayBrush;
  ID2D1SolidColorBrush *m_pCornflowerBlueBrush;

  WCHAR szPath[MAX_PATH];
  BOOL bFit;
  HWND m_hWnd;
} RENDERCTLDATA, *LPRENDERCTLDATA;

enum {
  RENDERER_D2D = 1,
  RENDERER_GDI = 2,
};

enum {
  TITLEPATH_NONE = 1,
  TITLEPATH_FILE = 2,
  TITLEPATH_FULL = 3,
};

enum {
  TOOLBARTHEME_DEFAULT_16PX = 1,
  TOOLBARTHEME_DEFAULT_24PX = 2,
  TOOLBARTHEME_DEFAULT_32PX = 3,
};

enum {
  MIME_IMAGE_PNG = 1,
  MIME_IMAGE_JPG = 2,
  MIME_IMAGE_GIF = 3,
};

typedef struct _tagSETTINGS {
  unsigned char magic[4];
  unsigned long nVersion;
  unsigned long checksum;
  BOOL bEulaAccepted;
  BOOL bNaviLoop;
  BOOL bNaviAnyFile;
  int nPathInTitleType;
  int nRendererType;
  int nToolbarTheme;
} SETTINGS;

typedef struct _tagNAVIASSOCENTRY {
  WCHAR szExtension[80];
  int nMimeType;
} NAVIASSOCENTRY;

typedef struct _tagPANIVIEWAPP {
  HWND hWndMain;
  SETTINGS m_settings;
  PWSTR m_appDataSite;
  PWSTR m_appCfgPath;
} PANIVIEWAPP;

HINSTANCE g_hInst;

static const char g_cfgMagic[4] = { 'P', 'N', 'V', '\xE5' };

const WCHAR szPaniView[] = L"PaniView";
const WCHAR szPaniViewClassName[] = L"PaniView_Main";
const WCHAR szRenderCtlClassName[] = L"_D2DRenderCtl78";

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int);

BOOL InitInstance(HINSTANCE);
void PopupError(DWORD, HWND);
BOOL PopupEULADialog();
BOOL GetApplicationDataSitePath(PWSTR*);

unsigned long crc32(unsigned char *, size_t);

/* Direct2D Utility functions forward declaration */
static inline D2D1_MATRIX_3X2_F D2DUtilMatrixIdentity();
static inline D2D1_MATRIX_3X2_F D2DUtilMakeTranslationMatrix(D2D1_SIZE_F);
static inline D2D1_MATRIX_3X2_F D2DUtilMatrixMultiply(D2D1_MATRIX_3X2_F *,
    D2D_MATRIX_3X2_F *);
static inline D2D1_COLOR_F D2DColorFromRGBAndAlpha(COLORREF, float);
static inline D2D1_COLOR_F D2DColorFromRGB(COLORREF);

/* COM aggregate return types fixes */
typedef VOID(*fnID2D1HwndRenderTarget_GetSize)(ID2D1RenderTarget *,
    D2D1_SIZE_F *);
typedef VOID(*fnID2D1Bitmap_GetSize)(ID2D1Bitmap *, D2D1_SIZE_F *);
static inline D2D1_SIZE_F ID2D1HwndRenderTarget_GetSize$Fix(
    ID2D1HwndRenderTarget *);
static inline D2D1_SIZE_F ID2D1Bitmap_GetSize$Fix(ID2D1Bitmap *);

BOOL Settings_LoadFile(SETTINGS *, PWSTR);
BOOL Settings_SaveFile(SETTINGS *, PWSTR);
BOOL Settings_LoadDefault(SETTINGS *);

/* Application object methods forward declarations */
static inline PANIVIEWAPP *PaniViewApp_GetInstance();
BOOL PaniViewApp_Initialize(PANIVIEWAPP *);
PWSTR PaniVewApp_GetAppDataSitePath(PANIVIEWAPP *);
PWSTR PaniViewApp_GetSettingsFilePath(PANIVIEWAPP *);
BOOL PaniViewApp_CreateAppDataSite(PANIVIEWAPP *);
BOOL PaniViewApp_LoadSettings(PANIVIEWAPP *);
BOOL PaniViewApp_SaveSettings(PANIVIEWAPP *);
BOOL PaniViewApp_LoadDefaultSettings(PANIVIEWAPP *);

/* Application window forward-declared methods */
BOOL PaniView_RegisterClass(HINSTANCE);
LRESULT CALLBACK PaniView_WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL PaniView_OnCreate(HWND, LPCREATESTRUCT);
void PaniView_OnSize(HWND, UINT, int, int);
void PaniView_OnCommand(HWND, int, HWND, UINT);
void PaniView_OnPaint(HWND);
void PaniView_OnDestroy(HWND);
void PaniView_SetTitle(HWND);

/* Renderer control forward-declared methods */
BOOL RenderCtl_RegisterClass(HINSTANCE);
LRESULT CALLBACK RenderCtl_WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL RenderCtl_OnCreate(HWND, LPCREATESTRUCT);
void RenderCtl_OnSize(HWND, UINT, int, int);
void RenderCtl_OnPaint(HWND);
void RenderCtl_OnDestroy(HWND);
void RenderCtl_OnSendNudes(HWND);
void RenderCtl_OnFilePrev(HWND);
void RenderCtl_OnFileNext(HWND);
void RenderCtl_OnFitCmd(HWND);
HRESULT RenderCtl_CreateDeviceResources(LPRENDERCTLDATA);
void RenderCtl_DiscardDeviceResources(LPRENDERCTLDATA);
HRESULT RenderCtl_LoadFromFile(LPRENDERCTLDATA, LPWSTR);

void InvokeFileOpenDialog(LPWSTR*);

BOOL NextFileInDir(LPRENDERCTLDATA, BOOL, LPWSTR);

INT_PTR CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK EULADlgProc(HWND, UINT, WPARAM, LPARAM);

/*
 * wWinMain
 * Main entry point for the PaniView aplication
 *
 * There all starts and ends.
 * After main application window being closed, the `GetMessage` Windows API
 * call will catch a `WM_QUIT` message and break the application loop.
 *
 * After application loop break the process of shutdowing the application
 * begins, evolving settings data on disk: window position, recent file list,
 * etc.
 */
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  MSG msg = { 0 };
  DWORD dwError;
  HRESULT hr = S_OK;

  /* Initialize COM API */
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  dwError = GetLastError();
  if (FAILED(hr)) {
    PopupError(dwError, NULL);
    return -1;
  }

  /* Initialize common controls */
  BOOL bStatus = FALSE;
  INITCOMMONCONTROLSEX iccex = { 0 };
  iccex.dwSize = sizeof(iccex);
  iccex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
  bStatus = InitCommonControlsEx(&iccex);
  if (!bStatus) {
    dwError = GetLastError();
    assert(FALSE);
    PopupError(dwError, NULL);
  }

  PANIVIEWAPP *app = PaniViewApp_GetInstance();
  if (!PaniViewApp_Initialize(app)) {
    return -1;
  }

  InitInstance(hInstance); /* Initialize the application's instance — register
                              classes, lock single instance mutexes, etc. */

  if (!app->m_settings.bEulaAccepted)
  {
    BOOL bAccepted = PopupEULADialog();
    if (bAccepted) {
      app->m_settings.bEulaAccepted = TRUE;
    }
    else {
      return -1;
    }
  }

  HWND hWndMain = CreateWindowEx(0, szPaniViewClassName, szPaniView,
      WS_OVERLAPPEDWINDOW, /* Top-level window */
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, /* Use default position and size */
      NULL, /* No parent window */
      NULL, /* No menu */
      hInstance,
      NULL); /* No extra window data */

  if (!hWndMain)
  {
    dwError = GetLastError();
    assert(FALSE);
    PopupError(dwError, NULL);
    return -1;
  }

  ShowWindow(hWndMain, nCmdShow);
  UpdateWindow(hWndMain); /* Force window contents paint inplace after
                             creating*/

  /*
   * Application loop
   *
   * Will be pumped till WM_QUIT message will noticed.
   * Then application uninitialization and shutdown begins.
   */
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg); /* Translate key messages into proper keycodes */
    DispatchMessage(&msg);  /* Proceed message into dispatcher */
  }

  CoUninitialize();

  if (!PaniViewApp_SaveSettings(app)) {
    MessageBox(NULL, L"Unable to save settings data", NULL, MB_OK | MB_ICONERROR);
  }

  /* Application shutdown */
  return (int) msg.wParam;
}

/*
 * InitInstance
 * Application instantiating routine. Registers the application's main window
 * class, sharing the instance handle access.
 */
BOOL InitInstance(HINSTANCE hInstance)
{
  PaniView_RegisterClass(hInstance);
  RenderCtl_RegisterClass(hInstance);
  return TRUE;
}

/*
 * PopupError
 * Popping up a message box with detailed error message formatted in
 * human-readable format
 *
 * The `dwError` is an system error code gathered via `GetErrorMessage()`
 * call, or returned from API calls HRESULT value.
 *
 * Specifying the `hParent` parameter, he message box might be tied to parent
 * window and grab it's focus.
 */
void PopupError(DWORD dwError, HWND hParent)
{
  LPWSTR lpszMessage;
  LPWSTR lpszSysMessage = NULL;

  DWORD dwFmtError = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, /* No source. Message from system. */
      dwError, /* Error code */
      0, /* No line width restrictions */
      (LPWSTR) &lpszSysMessage, /* Output buffer */
      0, /* minimum count of character to allocate */
      NULL); /* No arguments for inserts */
  if (!dwFmtError) {
    assert(FALSE);
  }

  /* Get the length of the system message */
  size_t lenSysMessage;
  StringCchLength(lpszSysMessage, STRSAFE_MAX_CCH, &lenSysMessage);

  /* WARNING! Read the comment bellow before changing this string */
  const WCHAR szFmt[] = L"Error 0x%08x: %s";

  /* Resulted formatted string will be longer than format specifier when
   * expanded. As it constant, better to keep it by-hand value.
   * We assume that resulted string would be:
   * "Error 0x00000000: " (At least), so there is 18 character length. */
  static const size_t lenFmt = 18;

  /* +1 for null-terminating character */
  size_t lenMessage = lenFmt + lenSysMessage + 1;

  /* Allocate buffer for our message string */
  lpszMessage = (LPWSTR) calloc(1, lenMessage * sizeof(WCHAR));
  assert(lpszMessage);

  /* Print formatted message */
  StringCchPrintf(lpszMessage, lenMessage, szFmt, dwError, lpszSysMessage);
  LocalFree(lpszSysMessage);  /* Free system error message string */

  /* Show popup */
  MessageBox(hParent, lpszMessage, NULL, MB_OK | MB_ICONERROR);

  free(lpszMessage);
}

BOOL PopupEULADialog()
{
  HMODULE hMsftEdit = LoadLibrary(L"Msftedit.dll");
  if (!hMsftEdit)
    return FALSE;

  int iStatus = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_EULA), NULL,
      (DLGPROC)EULADlgProc);

  FreeLibrary(hMsftEdit);

  return iStatus == IDOK;
}

/*
 *  GetApplicationDataSitePath
 *  Construct the AppData site string. Usually should be called once.
 *
 *  NOTE: The caller manages the memory deallocation
 */
BOOL GetApplicationDataSitePath(PWSTR* ppStr)
{
  static const WCHAR szDataSiteDir[] = L"Aragajaga\\PaniView";

  HRESULT hr = S_OK;
  BOOL bStatus = FALSE;

  PWSTR pszAppDataRoot = NULL;

  if (!ppStr) {
    assert(FALSE);
    return FALSE;
  }

  *ppStr = NULL;

  /* Get current user's %APPDATA%\Local path */
  hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData,
      KF_FLAG_DEFAULT,
      NULL, /* No access token, current user's AppData */
      &pszAppDataRoot);
  if (FAILED(hr)) {
    assert(FALSE);
    PopupError(hr, NULL);
    goto fail;
  }

  size_t lenAppDataSite = wcslen(pszAppDataRoot) + ARRAYSIZE(szDataSiteDir) + 1;
  PWSTR pszAppDataSite = calloc(1, (lenAppDataSite + 1) * sizeof(WCHAR));
  if (!pszAppDataSite) {
    assert(FALSE);
    goto fail;
  }

  StringCchCopy(pszAppDataSite, lenAppDataSite, pszAppDataRoot);
  PathCchAppend(pszAppDataSite, lenAppDataSite, szDataSiteDir);

  /* Succeed */
  bStatus = TRUE;
  *ppStr = pszAppDataSite;

fail:
  CoTaskMemFree(pszAppDataRoot);

  if (bStatus) {
    return bStatus;
  }

  free(pszAppDataSite);
  return bStatus;
}

/*
 * crc32
 * Cycle Redundancy Check hashing algorithm
 */
unsigned long crc32(unsigned char *data, size_t length)
{
  static unsigned long table[256];

  if (!*table) {
    for (unsigned long i = 0; i < 256; ++i) {
      unsigned long r = i;

      for (int j = 0; j < 8; ++j) {
        r = (r & 1 ? 0 : 0xEDB88320UL) ^ r >> 1;
      }

      table[i] = r ^ 0xFF000000UL;
    }
  }

  unsigned int checksum = 0;

  for (size_t i = 0; i < length; ++i) {
    checksum = table[(unsigned char)(checksum) ^ ((unsigned char *)data)[i]] ^ checksum >> 8;
  }

  return checksum;
}

static inline D2D1_MATRIX_3X2_F D2DUtilMatrixIdentity() {
  D2D1_MATRIX_3X2_F mat = { 0 };
  mat._11 = 1.f;
  mat._22 = 1.f;

  return mat;
}

static inline D2D1_MATRIX_3X2_F D2DUtilMakeTranslationMatrix(D2D1_SIZE_F size)
{
  D2D1_MATRIX_3X2_F mat = { 0 };
  mat._11 = 1.0;
  mat._12 = 0.0;
  mat._21 = 0.0;
  mat._22 = 1.0;
  mat._31 = size.width;
  mat._32 = size.height;

  return mat;
}

static inline D2D1_MATRIX_3X2_F D2DUtilMatrixMultiply(D2D1_MATRIX_3X2_F *a,
    D2D1_MATRIX_3X2_F *b)
{
  D2D1_MATRIX_3X2_F mat = { 0 };
  mat._11 = a->_11 * b->_11 + a->_12 * b->_21;
  mat._12 = a->_11 * b->_12 + a->_12 * b->_22;
  mat._21 = a->_21 * b->_11 + a->_22 * b->_21;
  mat._22 = a->_21 * b->_12 + a->_22 * b->_22;
  mat._31 = a->_31 * b->_11 + a->_32 * b->_21 + b->_31;
  mat._32 = a->_31 * b->_12 + a->_32 * b->_22 + b->_32;

  return mat;
}

static inline D2D1_COLOR_F D2DColorFromRGBAndAlpha(COLORREF color, FLOAT alpha)
{
  D2D1_COLOR_F d2dColor;
  d2dColor.b = (float)((color >> 16) & 0xFF) / 255.f;
  d2dColor.g = (float)((color >> 8 ) & 0xFF) / 255.f;
  d2dColor.r = (float)(color & 0xff) / 255.f;
  d2dColor.a = alpha;

  return d2dColor;
}

static inline D2D1_COLOR_F D2DColorFromRGB(COLORREF color)
{
  return D2DColorFromRGBAndAlpha(color, 1.f);
}

/* It seems that MS had wrecked an agregate returns for all COM methods called
 * via C. 🙄
 *
 * https://blog.airesoft.co.uk/2014/12/direct2d-scene-of-the-accident/
 * https://sourceforge.net/p/mingw-w64/mailman/message/36859011/
 * https://github.com/fanc999/d2d-sample-mostly-c/issues/1
 *
 * Hacking it. (Thanx @jacoblusk)
 *
 * Also, this need test on 32bit os because of different calling conventions.
 * (it is trivial for x64 to have a single __fastcall)
 */

static inline D2D1_SIZE_F ID2D1HwndRenderTarget_GetSize$Fix(
    ID2D1HwndRenderTarget *_this)
{
  /* It is normal for compilers to notify about undesirable implicit
   * conversions between non-compatible pointer types, but here this is
   * a planned scenario. So we do it through a pointer to pointer
   * dereference to suppress compiler warnings.
   */
  D2D1_SIZE_F size;

  fnID2D1HwndRenderTarget_GetSize fnGetSize =
    *(fnID2D1HwndRenderTarget_GetSize *)
    &((ID2D1RenderTarget *) _this)->lpVtbl->GetSize;
  (*fnGetSize)((ID2D1RenderTarget *)_this, &size);

  return size;
}

static inline D2D1_SIZE_F ID2D1Bitmap_GetSize$Fix(ID2D1Bitmap *_this)
{
  D2D1_SIZE_F size;

  fnID2D1Bitmap_GetSize fnGetSize =
    *(fnID2D1Bitmap_GetSize *) &_this->lpVtbl->GetSize;
  (*fnGetSize)(_this, &size);

  return size;
}

BOOL Settings_LoadFile(SETTINGS *settings, PWSTR pszPath)
{
  if (!(settings && pszPath)) {
    assert(FALSE);
    return FALSE;
  }

  FILE *pfd = _wfopen(pszPath, L"rb");
  if (!pfd) {
    return FALSE;
  }

  BOOL bStatus = FALSE;
  SETTINGS *tmpCfg = calloc(1, sizeof(SETTINGS));
  if (tmpCfg) {
    unsigned char magic[4];
    fread(magic, sizeof(g_cfgMagic), 1, pfd);

    if (!memcmp(magic, g_cfgMagic, sizeof(g_cfgMagic)))
    {
      fseek(pfd, 0, SEEK_SET);
      fread(tmpCfg, sizeof(SETTINGS), 1, pfd);

      unsigned long fileChecksum = tmpCfg->checksum;
      tmpCfg->checksum = 0xFFFFFFFFUL;
      unsigned long calcChecksum = crc32((unsigned char *)tmpCfg, sizeof(SETTINGS));
      tmpCfg->checksum = fileChecksum;

      if (fileChecksum == calcChecksum) {
        memcpy(settings, &tmpCfg, sizeof(SETTINGS));
        bStatus = TRUE;
      }
    }
  }

  free(tmpCfg);
  fclose(pfd);
  return bStatus;
}

BOOL Settings_SaveFile(SETTINGS *settings, PWSTR pszPath)
{
  if (!(settings && pszPath)) {
    return FALSE;
  }

  FILE *pfd = _wfopen(pszPath, L"wb");
  if (!pfd) {
    assert(FALSE);
    return FALSE;
  }

  memcpy(&settings->magic, g_cfgMagic, sizeof(g_cfgMagic));
  settings->checksum = 0xFFFFFFFF;
  settings->checksum = crc32((unsigned char *)settings, sizeof(SETTINGS));

  fwrite(settings, sizeof(SETTINGS), 1, pfd);
  fclose(pfd);
  return TRUE;
}

BOOL Settings_LoadDefault(SETTINGS *pSettings)
{
  if (!pSettings) {
    return FALSE;
  }

  ZeroMemory(pSettings, sizeof(SETTINGS));
  pSettings->bEulaAccepted = FALSE;
  pSettings->bNaviLoop = TRUE;
  pSettings->nRendererType = RENDERER_D2D;
  pSettings->nToolbarTheme = TOOLBARTHEME_DEFAULT_24PX;

  return TRUE;
}

static inline PANIVIEWAPP *PaniViewApp_GetInstance()
{
  static PANIVIEWAPP *s_gApp = NULL;

  if (!s_gApp) {
    s_gApp = calloc(1, sizeof(PANIVIEWAPP));
  }

  return s_gApp;
}

BOOL PaniViewApp_Initialize(PANIVIEWAPP *app)
{
  if (!PaniViewApp_LoadSettings(app)) {
    if (!PaniViewApp_LoadDefaultSettings(app)) {
      return FALSE;
    }
  }

  return TRUE;
}

PWSTR PaniViewApp_GetAppDataSitePath(PANIVIEWAPP *app) {
  if (!app->m_appDataSite) {
    GetApplicationDataSitePath(&app->m_appDataSite);
  }

  return app->m_appDataSite;
}

PWSTR PaniViewApp_GetSettingsFilePath(PANIVIEWAPP *app) {
  static const WCHAR szCfgFileName[] = L"settings.dat";

  PWSTR pszCfgPath = NULL;

  if (!app->m_appCfgPath) {
    PWSTR pszSite = PaniViewApp_GetAppDataSitePath(app);

    if (pszSite) {
      size_t lenCfgPath = wcslen(pszSite) + ARRAYSIZE(szCfgFileName) + 1;

      pszCfgPath = calloc(1, lenCfgPath * sizeof(WCHAR));
      if (pszCfgPath) {
        StringCchCopy(pszCfgPath, lenCfgPath, pszSite);
        PathCchAppend(pszCfgPath, lenCfgPath, szCfgFileName);
      }
    }
  }

  return pszCfgPath;
}

BOOL PaniViewApp_CreateAppDataSite(PANIVIEWAPP *app) {
  PWSTR pszSite;

  pszSite = PaniViewApp_GetAppDataSitePath(app);
  if (!pszSite) {
    return FALSE;
  }

  /* TODO: MAKE RESEARCH, MAY BE CAPPED AT MAX_PATH */
  int iStatus = SHCreateDirectoryEx(NULL, (PCWSTR)pszSite, NULL);
  if (iStatus != ERROR_SUCCESS && iStatus != ERROR_ALREADY_EXISTS) {
    assert(FALSE);
    PopupError(iStatus, NULL);
    return FALSE;
  }

  return TRUE;
}

BOOL PaniViewApp_LoadSettings(PANIVIEWAPP *app)
{
  if (!app) {
    return FALSE;
  }

  PWSTR pszCfgPath = PaniViewApp_GetSettingsFilePath(app);
  if (!pszCfgPath) {
    return FALSE;
  }

  return Settings_LoadFile(&app->m_settings, pszCfgPath);
}

BOOL PaniViewApp_SaveSettings(PANIVIEWAPP *app)
{
  if (!app) {
    return FALSE;
  }

  PWSTR pszCfgPath = PaniViewApp_GetSettingsFilePath(app);
  if (!pszCfgPath) {
    return FALSE;
  }

  return Settings_SaveFile(&app->m_settings, pszCfgPath);
}

BOOL PaniViewApp_LoadDefaultSettings(PANIVIEWAPP *app)
{
  if (!app) {
    return FALSE;
  }

  return Settings_LoadDefault(&app->m_settings);
}

BOOL PaniView_RegisterClass(HINSTANCE hInstance)
{
  WNDCLASSEX wcex = { 0 };

  g_hInst = hInstance;  /* Store application instance globally */

  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_VREDRAW | CS_HREDRAW; /* Redraw window on vertical and
                                           horizontal sizing */
  wcex.lpfnWndProc = (WNDPROC) PaniView_WndProc;  /* Window message dispatcher
                                                     procedure */
  wcex.cbWndExtra = sizeof(LPMAINFRAMEDATA);
  wcex.hInstance = hInstance; /* Assign image base instance */
  wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1); /* Default gray
                                                        background */
  wcex.lpszMenuName = MAKEINTRESOURCE(IDM_MAIN);  /* Load application frame
                                                     menu*/
  wcex.lpszClassName = szPaniViewClassName;

  return RegisterClassEx(&wcex);
}

/*
 * PaniView_WndProc
 * Message dispatching procedure callback for main window class
 *
 * An application loop DispatchMessage` call leads here trough a callback from
 * the system, when the particular event been fired.
 */
LRESULT CALLBACK PaniView_WndProc(HWND hWnd, UINT message,WPARAM wParam,
    LPARAM lParam)
{
  /*
   * Please do not write an inline switch code
   *
   * Create a superior method instead, like `WindowClass_OnPaint`, etc. and
   * put it's call here only.
   * Follow the MFC's message map flow.
   *
   * Also you can try a luck using special macros - message crackers from
   * windowsx.h. They starts with `HANDLE_` and proceeding with window
   * message macro name, like HANDLE_WM_CREATE. The last parameter is a
   * function callback, other is a common parameters for this message.
   * See MSDN for function declaraction on each specific message.
   *
   */
  switch (message)
  {
    HANDLE_MSG(hWnd, WM_CREATE, PaniView_OnCreate);
    HANDLE_MSG(hWnd, WM_COMMAND, PaniView_OnCommand);
    HANDLE_MSG(hWnd, WM_SIZE, PaniView_OnSize);
    HANDLE_MSG(hWnd, WM_DESTROY, PaniView_OnDestroy);
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

/*
 * PaniView_OnCreate
 * The procedure of creating and initializing child controls, and initializing
 * of window itself by allocating and assigning a custom data structure
 * pointer to internal window data filed. So we can grab custom window data
 * structure each time by it's handle.
 */
BOOL PaniView_OnCreate(HWND hWnd, LPCREATESTRUCT lpcs)
{
  UNREFERENCED_PARAMETER(lpcs);

  LPMAINFRAMEDATA wndData = (LPMAINFRAMEDATA) calloc(1, sizeof(MAINFRAMEDATA));
  assert(wndData);
  SetWindowLongPtr(hWnd, 0, (LONG_PTR)wndData);

  /* Create paging toolbar control */
  HWND hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME,
      NULL, /* No text, name or title */
      WS_CHILD | TBSTYLE_WRAPABLE | TBSTYLE_FLAT | TBSTYLE_LIST |
          TBSTYLE_TOOLTIPS | CCS_NODIVIDER | CCS_NORESIZE,
      0, 0, 0, 0, /* Will be sized on first WM_SIZE */
      hWnd,
      NULL, /* No menu nor control id */
      g_hInst,
      NULL); /* No custom window data */
  if (!hWndToolbar) {
    assert(FALSE);
    return FALSE;
  }

  SendMessage(hWndToolbar, TB_SETEXTENDEDSTYLE, 0,
      (LPARAM)TBSTYLE_EX_MIXEDBUTTONS);

  /* Obsolete API (Loads only 16 colors images) */
#if 0
  /* Imagelist contains paging toolbar icons */
  HIMAGELIST hImageList = ImageList_LoadBitmap(
      g_hInst,
      MAKEINTRESOURCE(IDB_TOOLBARSTRIP24),
      24, /* Width */
      0,  /* Do not reserve room for growing */
      0);  /* Magenta transparency key color */
  assert(hImageList);
#endif

  HIMAGELIST hImageList = ImageList_LoadImage(
      g_hInst,
      MAKEINTRESOURCE(IDB_TOOLBARSTRIP24),
      24, /* Width */
      0,  /* Do not reserve space for growth */
      CLR_DEFAULT, /* No transparency mask */
      IMAGE_BITMAP, /* Type of image to load */
      LR_CREATEDIBSECTION | LR_DEFAULTCOLOR);
  assert(hImageList);


  /* Redundant HBITMAP loading
   * Is it recommended way? But above method ocassionaly works well too.
   * */
#if 0
  HIMAGELIST hImageList = ImageList_Create(
      24, /* Width */
      24, /* Height */
      ILC_COLOR32 | ILC_MASK,
      6,  /* Number of images the list initially contains */
      0); /* Do not reserve for growth */
  assert(hImageList);

  HBITMAP hbmToolbarStrip = LoadImage(
      g_hInst,
      MAKEINTRESOURCE(IDB_TOOLBARSTRIP24),
      IMAGE_BITMAP,
      0,  /* Width (use resource specified width) */
      0,  /* Height (use resource specified height) */
      LR_CREATEDIBSECTION);
  assert(hbmToolbarStrip);

  ImageList_Add(hImageList, hbmToolbarStrip,
      NULL); /* No mask */
#endif

  /*
   *  Select the imagelist into toolbar
   *
   *  Q: Is there a toolbar macro exists for this?
   *  A: No. Probably no toolbar macros exist (basing on MSDN reference list)
   *  Q: But maybe they forgot to add it, and there is in official SDK headers?
   *  TODO: Need check
   *  */
  SendMessage(hWndToolbar, TB_SETIMAGELIST,
      (WPARAM) 0,  /* Image list id */
      (LPARAM) hImageList);

  /* Initialize button info */
  TBBUTTON tbButtons[] = {
    { 0, IDM_PREV, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR) L"Previous" },
    { 1, IDM_NEXT, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR) L"Next" },
    { 2, IDM_ZOOMOUT, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR) L"Zoom Out" },
    { 3, IDM_ZOOMIN, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR) L"Zoom In" },
    { 4, IDM_ACTUALSIZE, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR) L"Actual Size" },
    { 5, IDM_FITSIZE, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR) L"Fit" },
  };

  /* Add buttons */
  SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)6, (LPARAM)&tbButtons);

  /* Resize the toolbar, and then show it. */
  SendMessage(hWndToolbar, TB_AUTOSIZE, 0, 0);
  ShowWindow(hWndToolbar, TRUE);

  HWND hWndRenderer = CreateWindowEx(0, szRenderCtlClassName, NULL,
      WS_CHILD | WS_VISIBLE,
      10, 10, 320, 240,
      hWnd, NULL, g_hInst, NULL);
  assert(hWndRenderer);

  wndData->hRenderer = hWndRenderer;
  wndData->hToolbar = hWndToolbar;

  return TRUE;
}

/*
 *  PaniView_OnSize
 *
 *  Fired on window being resized. Updates all child content.
 *  The render area will fill whore client rect, except toolbar
 *  on the bottom. So there did some calculations how to arrange
 *  the toolbar on bottom center and how render control will fit.
 */
void PaniView_OnSize(HWND hWnd, UINT state, int cx, int cy)
{
  UNREFERENCED_PARAMETER(state);  /* Do not handle maximized/minimized state */

  LPMAINFRAMEDATA wndData = (LPMAINFRAMEDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

#if 0
  RECT rcToolbar = { 0 };
  SendMessage(wndData->hToolbar, WM_SIZE, 0, 0);
  GetWindowRect(wndData->hToolbar, &rcToolbar);
#endif

  SIZE tbSize;
  SendMessage(wndData->hToolbar, TB_GETMAXSIZE, 0, (LPARAM)&tbSize);

  SetWindowPos(wndData->hToolbar, NULL,
      (cx - tbSize.cx) / 2, /* Center horizontally */
      cy - (tbSize.cy), /* Snap to bottom */
      tbSize.cx, /* Apply maximum toolbar size */
      tbSize.cy,
      SWP_NOZORDER);

  SetWindowPos(wndData->hRenderer, NULL,
      0, 0,
      cx,
      cy - (tbSize.cy),  /* Subtract the toolbar's heigth */
      SWP_NOZORDER);
}

void PaniView_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{
  UNREFERENCED_PARAMETER(hwndCtl);
  UNREFERENCED_PARAMETER(codeNotify);

  LPMAINFRAMEDATA wndData = (LPMAINFRAMEDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  switch (id) {

    case IDM_FILE_OPEN:
      {
      SendMessage(wndData->hRenderer, WM_SENDNUDES, 0, 0);
      InvalidateRect(hWnd, NULL, FALSE);
      }
      break;

    case IDM_PREV:
      SendMessage(wndData->hRenderer, WM_FILEPREV, 0, 0);
      InvalidateRect(hWnd, NULL, FALSE);
      break;

    case IDM_NEXT:
      SendMessage(wndData->hRenderer, WM_FILENEXT, 0, 0);
      InvalidateRect(hWnd, NULL, FALSE);
      break;

    case IDM_FITSIZE:
      SendMessage(wndData->hRenderer, WM_FIT, 0, 0);
      InvalidateRect(hWnd, NULL, FALSE);
      break;

    case IDM_SETTINGS:
      DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS),
          hWnd, (DLGPROC)SettingsDlgProc);
      break;

    case IDM_ABOUT:
      DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT),
          hWnd, (DLGPROC)AboutDlgProc);
      break;

    case IDM_FILE_EXIT:
      PostQuitMessage(0);
      break;
  }
}

/*
 *  PaniView_OnPaint
 */
void PaniView_OnPaint(HWND hWnd)
{
  UNREFERENCED_PARAMETER(hWnd);
}

/*
 *  PaniView_OnDestroy
 *  Process the application frame shuthown. Usually it means that user exit
 *  the application, so this call should call proper application termination. 
 */
void PaniView_OnDestroy(HWND hWnd)
{
  UNREFERENCED_PARAMETER(hWnd);

  /* TODO: Save data */
  PostQuitMessage(0);
}

/*
 *  RenderCtl_RegisterClass
 *  Register the renderer control window class
 */
BOOL RenderCtl_RegisterClass(HINSTANCE hInstance)
{
  WNDCLASSEX wcex = { 0 };

  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_VREDRAW | CS_HREDRAW;
  wcex.lpfnWndProc = (WNDPROC) RenderCtl_WndProc;
  wcex.cbWndExtra = sizeof(LPRENDERCTLDATA);
  wcex.hInstance = hInstance;
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  /* I think there is no need to specify GDI background brush since all screen
   * is drawn by D2D */
  wcex.lpszClassName = szRenderCtlClassName;

  return RegisterClassEx(&wcex);
}

LRESULT CALLBACK RenderCtl_WndProc(HWND hWnd, UINT message, WPARAM wParam,
    LPARAM lParam)
{
  switch (message)
  {
    HANDLE_MSG(hWnd, WM_CREATE, RenderCtl_OnCreate);
    HANDLE_MSG(hWnd, WM_SIZE, RenderCtl_OnSize);
    HANDLE_MSG(hWnd, WM_PAINT, RenderCtl_OnPaint);
    HANDLE_MSG(hWnd, WM_DESTROY, RenderCtl_OnDestroy);

    case WM_SENDNUDES:
      RenderCtl_OnSendNudes(hWnd);
      return 0;

    case WM_FILEPREV:
      RenderCtl_OnFilePrev(hWnd);
      return 0;

    case WM_FILENEXT:
      RenderCtl_OnFileNext(hWnd);
      return 0;

    case WM_FIT:
      RenderCtl_OnFitCmd(hWnd);
      return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

BOOL RenderCtl_OnCreate(HWND hWnd, LPCREATESTRUCT lpcs)
{
  UNREFERENCED_PARAMETER(lpcs);

  HRESULT hr = S_OK;
  LPRENDERCTLDATA wndData;

  wndData = (LPRENDERCTLDATA) calloc(1, sizeof(RENDERCTLDATA));
  assert(wndData);

  wndData->m_hWnd = hWnd;

  hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
      &IID_IWICImagingFactory, (LPVOID)&wndData->m_pIWICFactory);
  if (FAILED(hr))
  {
    PopupError(hr, NULL);
    assert(FALSE);
    return FALSE;
  }

  /* !!! TAKE CARE !!!
   * IID_ID2D1Factory is used, but IID_ID2D1Factory1 with 'one' in the end
   * exist, but mingw can't find it (by the way it is defined in d2d1_1.h,
   * probably that header should to be included instead of just d2d1.h)
   * === MAKE RESEARCH === */
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1,
      NULL, (LPVOID)&(wndData->m_pD2DFactory));
  if (FAILED(hr)) {
    assert(FALSE);
    /* will wndData be freed on WM_DESTROY after `return` returns FALSE? */
    return FALSE;
  }

  SetWindowLongPtr(hWnd, 0, (LONG_PTR)wndData);
  return TRUE;
}

void RenderCtl_OnSize(HWND hWnd, UINT state, int cx, int cy)
{
  UNREFERENCED_PARAMETER(hWnd);
  UNREFERENCED_PARAMETER(state);

  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  if (wndData->m_pRenderTarget) {
    wndData->m_pRenderTarget->lpVtbl->Resize(wndData->m_pRenderTarget,
        &(D2D1_SIZE_U){ cx, cy });
  }
}

void RenderCtl_OnPaint(HWND hWnd)
{
  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  HRESULT hr = S_OK;

  hr = RenderCtl_CreateDeviceResources(wndData);
  if (SUCCEEDED(hr)) {

    wndData->m_pRenderTarget->lpVtbl->Base.BeginDraw(
        (ID2D1RenderTarget *)wndData->m_pRenderTarget);

    D2D1_MATRIX_3X2_F mat = D2DUtilMatrixIdentity();

    wndData->m_pRenderTarget->lpVtbl->Base.SetTransform(
        (ID2D1RenderTarget *)wndData->m_pRenderTarget,
        &mat);

    D2D1_COLOR_F clearColor = D2DColorFromRGB(RGB(0xFF, 0xFF, 0xFF));

    wndData->m_pRenderTarget->lpVtbl->Base.Clear(
        (ID2D1RenderTarget *)wndData->m_pRenderTarget,
        &clearColor);

    /* Retrieve the size of the drawing area. */
    D2D1_SIZE_F rtSize;
    rtSize = ID2D1HwndRenderTarget_GetSize$Fix(wndData->m_pRenderTarget);

    /* Draw a grid background by using a loop and the render target's DrawLine
     * method to draw a series of lines. */
    /*
    int width = rtSize.width;
    int height = rtSize.height;
    */

    /*
    D2D1_RECT_F imgRect = {
      0.0f, 0.0f,
      rtSize.width, rtSize.height
    };
    */

    /*  D2DBitmap may have been released due to device loss.
     *  If so, re-create it from the source bitmap */
    if (wndData->m_pConvertedSourceBitmap && !wndData->m_pD2DBitmap)
    {
      wndData->m_pRenderTarget->lpVtbl->Base.CreateBitmapFromWicBitmap(
          (ID2D1RenderTarget *)wndData->m_pRenderTarget,
          (IWICBitmapSource *)wndData->m_pConvertedSourceBitmap,
          NULL, /* No bitmap properties */
          &wndData->m_pD2DBitmap);
    }

    /* Draw an image and scale it to the current window size */
    if (wndData->m_pD2DBitmap)
    {
      D2D1_MATRIX_3X2_F matAnchor;
      D2D1_MATRIX_3X2_F matPosition;
      D2D1_MATRIX_3X2_F mat;

      D2D1_SIZE_F bmpSize;
      bmpSize = ID2D1Bitmap_GetSize$Fix(wndData->m_pD2DBitmap);

      if (wndData->bFit) {
        if (bmpSize.width > rtSize.width) {
          float ratio = bmpSize.height/ bmpSize.width;
          bmpSize.width = rtSize.width;
          bmpSize.height = rtSize.width * ratio;
        }

        if (bmpSize.height > rtSize.height) {
          float ratio = bmpSize.width / bmpSize.height;
          bmpSize.width = rtSize.height * ratio;
          bmpSize.height = rtSize.height;
        }
      }

      matAnchor = D2DUtilMakeTranslationMatrix((D2D1_SIZE_F){
            -(bmpSize.width / 2),
            -(bmpSize.height / 2)});

      matPosition = D2DUtilMakeTranslationMatrix((D2D1_SIZE_F){
            rtSize.width / 2,
            rtSize.height / 2});

      mat = D2DUtilMatrixMultiply(&matAnchor, &matPosition);

      wndData->m_pRenderTarget->lpVtbl->Base.SetTransform(
          (ID2D1RenderTarget *)wndData->m_pRenderTarget, &mat);

      D2D1_RECT_F imgRect = {
        0.0f, 0.0f,
        bmpSize.width, bmpSize.height
      };

      wndData->m_pRenderTarget->lpVtbl->Base.DrawBitmap(
          (ID2D1RenderTarget *)wndData->m_pRenderTarget,
          wndData->m_pD2DBitmap,
          &imgRect, /* Destination rectangle */
          1.0f, /* Opacity */
          D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
          NULL);  /* No source rectangle (full image) */
    }

#if 0
    /* Simple D2D Drawing */

    /* 10px col spacing */
    for (int x = 0; x < width; x += 10) {
      D2D1_POINT_2F p1 = {x, 0.0f};
      D2D1_POINT_2F p2 = {x, rtSize.height};

      wndData->m_pRenderTarget->lpVtbl->Base.DrawLine(
          (ID2D1RenderTarget *)wndData->m_pRenderTarget,
          p1,
          p2,
          (ID2D1Brush *)wndData->m_pLightSlateGrayBrush,
          0.5f,
          NULL);  /* No stroke style */
    }

    /* 10px row spacing */
    for (int y = 0; y < height; y += 10) {
      D2D1_POINT_2F p1 = {0.0f, y};
      D2D1_POINT_2F p2 = {rtSize.width, y};

      wndData->m_pRenderTarget->lpVtbl->Base.DrawLine(
          (ID2D1RenderTarget *)wndData->m_pRenderTarget,
          p1,
          p2,
          (ID2D1Brush *)wndData->m_pCornflowerBlueBrush,
          0.5f,
          NULL);  /* No stroke style */
    }

    /* Draw two rectangles */
    D2D1_RECT_F rectangle1 = {
        rtSize.width/2 - 50.0f, rtSize.height/2 - 50.0f, rtSize.width/2 + 50.f,
        rtSize.height/2 + 50.f,
    };

    D2D1_RECT_F rectangle2 = {
        rtSize.width/2 - 100.0f,
        rtSize.height/2 - 100.0f,
        rtSize.width/2 + 100.0f,
        rtSize.height/2 + 100.0f,
    };

    /* Fill the interior of the first rectangle with the gray brush */
    wndData->m_pRenderTarget->lpVtbl->Base.FillRectangle(
        (ID2D1RenderTarget *)wndData->m_pRenderTarget,
        &rectangle1,
        (ID2D1Brush *)wndData->m_pLightSlateGrayBrush);

    /* Paint the outline of the second rectangle */
    wndData->m_pRenderTarget->lpVtbl->Base.DrawRectangle(
        (ID2D1RenderTarget *)wndData->m_pRenderTarget,
        &rectangle2,
        (ID2D1Brush *)wndData->m_pCornflowerBlueBrush,
        1.0,
        NULL);
#endif /* if 0 */

    hr = wndData->m_pRenderTarget->lpVtbl->Base.EndDraw(
        (ID2D1RenderTarget *)wndData->m_pRenderTarget,
        NULL,
        NULL);
    if (hr == (HRESULT)D2DERR_RECREATE_TARGET) {
      hr = S_OK;
      RenderCtl_DiscardDeviceResources(wndData);
    }
  }

  ValidateRect(hWnd, NULL);
}

void RenderCtl_OnSendNudes(HWND hWnd)
{
  HRESULT hr = S_OK;

  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  LPWSTR szFilePath;
  InvokeFileOpenDialog(&szFilePath);

  hr = RenderCtl_LoadFromFile(wndData, szFilePath);

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
  }
}

void RenderCtl_OnFilePrev(HWND hWnd)
{
  HRESULT hr = S_OK;

  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  WCHAR szNextFile[MAX_PATH] = { 0 };
  NextFileInDir(wndData, FALSE, szNextFile);

  hr = RenderCtl_LoadFromFile(wndData, szNextFile);

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
  }
}

void RenderCtl_OnFileNext(HWND hWnd)
{
  HRESULT hr = S_OK;

  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  WCHAR szNextFile[MAX_PATH] = { 0 };
  NextFileInDir(wndData, TRUE, szNextFile);

  hr = RenderCtl_LoadFromFile(wndData, szNextFile);

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
  }
}

void RenderCtl_OnFitCmd(HWND hWnd)
{
  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  wndData->bFit = !wndData->bFit;
  InvalidateRect(wndData->m_hWnd, NULL, FALSE);
}

void RenderCtl_OnDestroy(HWND hWnd)
{
  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  SAFE_RELEASE(wndData->m_pD2DFactory);
  SAFE_RELEASE(wndData->m_pRenderTarget);
  SAFE_RELEASE(wndData->m_pLightSlateGrayBrush);
  SAFE_RELEASE(wndData->m_pCornflowerBlueBrush);

  free(wndData);
}

HRESULT RenderCtl_CreateDeviceResources(LPRENDERCTLDATA wndData)
{
  HRESULT hr = S_OK;

  if (!wndData->m_pRenderTarget) {
    RECT rcClient;
    D2D1_SIZE_U size;

    GetClientRect(wndData->m_hWnd, &rcClient);
    size = (D2D1_SIZE_U){
      rcClient.right - rcClient.left,
      rcClient.bottom - rcClient.top
    };

    /* TODO: Research */
    D2D1_RENDER_TARGET_PROPERTIES rtProp = { 0 };
    rtProp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtProp.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
    rtProp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_UNKNOWN;
    rtProp.dpiX = DEFAULT_DPI;
    rtProp.dpiY = DEFAULT_DPI;
    rtProp.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    rtProp.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    D2D1_HWND_RENDER_TARGET_PROPERTIES rtHwndProp = { 0 };
    rtHwndProp.hwnd = wndData->m_hWnd;
    rtHwndProp.pixelSize = size; /* CHECK */
    rtHwndProp.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

    /* Create a Direct2D render target */
    hr = wndData->m_pD2DFactory->lpVtbl->CreateHwndRenderTarget(
        wndData->m_pD2DFactory,
        &rtProp,
        &rtHwndProp,
        &wndData->m_pRenderTarget);

    /*
     * Code from D2D Drawing demo
     * It still may be usable if we planned to add annotation features
     * */
#if 0
    /* Create brushes for drawing */
    if (SUCCEEDED(hr))
    {
      D2D1_COLOR_F color = D2DColorFromRGB(RGB(0x77, 0x88, 0x99));

      /* Create a gray brush */
      hr = wndData->m_pRenderTarget->lpVtbl->Base.CreateSolidColorBrush(
          (ID2D1RenderTarget *)wndData->m_pRenderTarget,
          &color,
          NULL,
          &wndData->m_pLightSlateGrayBrush);
    }

    if (SUCCEEDED(hr))
    {
      D2D1_COLOR_F color = D2DColorFromRGB(RGB(0x64, 0x95, 0xED));

      /* Create a blue brush */
      hr = wndData->m_pRenderTarget->lpVtbl->Base.CreateSolidColorBrush(
          (ID2D1RenderTarget *)wndData->m_pRenderTarget,
          &color,
          NULL,
          &wndData->m_pCornflowerBlueBrush);
    }
#endif /* if 0 */
  }

  return hr;
}

void RenderCtl_DiscardDeviceResources(LPRENDERCTLDATA wndData)
{
  SAFE_RELEASE(wndData->m_pRenderTarget);
  /*
  SAFE_RELEASE(wndData->m_pLightSlateGrayBrush);
  SAFE_RELEASE(wndData->m_pCornflowerBlueBrush);
  */
}

COMDLG_FILTERSPEC c_rgReadTypes[] = {
  {L"Common picture file formats", L"*.jpg;*.jpeg;*.png;*.bmp;*.gif"},
  {L"Windows/OS2 Bitmap", L"*.bmp"},
  {L"Portable Network Graphics", L"*.png"},
  {L"Joint Picture Expert Group", L"*.jpg;*.jpeg"},
  {L"CompuServe Graphics Interchange Format", L"*.gif"},
};

void InvokeFileOpenDialog(LPWSTR *szPath)
{
  HRESULT hr = S_OK;

  IFileDialog *pFileDialog = NULL;
  IShellItem *pShellItemResult = NULL;

  hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
      &IID_IFileDialog, (LPVOID)&pFileDialog);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(hr);
    goto error;
  }

  hr = pFileDialog->lpVtbl->SetFileTypes( pFileDialog,
      ARRAYSIZE(c_rgReadTypes), c_rgReadTypes);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(hr);
    goto error;
  }

  hr = pFileDialog->lpVtbl->Show(pFileDialog, NULL);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(hr);
    goto error;
  }

  hr = pFileDialog->lpVtbl->GetResult(pFileDialog, &pShellItemResult);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(hr);
    goto error;
  }

  hr = pShellItemResult->lpVtbl->GetDisplayName(pShellItemResult,
      SIGDN_FILESYSPATH, szPath);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(hr);
    goto error;
  }

error:
  SAFE_RELEASE(pShellItemResult);
  SAFE_RELEASE(pFileDialog);
}

HRESULT RenderCtl_LoadFromFile(LPRENDERCTLDATA wndData, LPWSTR pszPath)
{
  HRESULT hr = S_OK;

  IWICBitmapDecoder *pDecoder = NULL;
  IWICBitmapFrameDecode *pFrame = NULL;
  IWICFormatConverter *pConvertedSourceBitmap = NULL;
  ID2D1Bitmap *pD2DBitmap = NULL;

  /* Request image decoder COM object */
  hr = wndData->m_pIWICFactory->lpVtbl->CreateDecoderFromFilename(
      wndData->m_pIWICFactory,
      pszPath,  /* Path to the image to be decoded */
      NULL, /* Not preferring any particular vendor */
      GENERIC_READ, /* Desired read access to the file */
      WICDecodeMetadataCacheOnDemand, /* Cache metadata when needed */
      &pDecoder);

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  /* Decode image */
  hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  /* Convert the frame to 32bppPBGRA */
  hr = wndData->m_pIWICFactory->lpVtbl->CreateFormatConverter(
      wndData->m_pIWICFactory,
      &pConvertedSourceBitmap);

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  hr = pConvertedSourceBitmap->lpVtbl->Initialize(
      pConvertedSourceBitmap,
      (IWICBitmapSource *)pFrame, /* Input bitmap to convert */
      &GUID_WICPixelFormat32bppPBGRA, /* Destination pixel format */
      WICBitmapDitherTypeNone,  /* No dither pattern */
      NULL, /* Do not specify particular color pallete */
      0.f,  /* Alpha threshold */
      WICBitmapPaletteTypeCustom);  /* Palette transform type */

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  hr = wndData->m_pRenderTarget->lpVtbl->Base.CreateBitmapFromWicBitmap(
      (ID2D1RenderTarget *)wndData->m_pRenderTarget,
      (IWICBitmapSource *)pConvertedSourceBitmap,
      NULL,
      &pD2DBitmap);

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  /* Done successfully. Actually do the changes into structure. */
  SAFE_RELEASE(wndData->m_pConvertedSourceBitmap);
  SAFE_RELEASE(wndData->m_pD2DBitmap);

  wndData->m_pConvertedSourceBitmap = pConvertedSourceBitmap;
  wndData->m_pD2DBitmap = pD2DBitmap;

  pConvertedSourceBitmap = NULL;
  pD2DBitmap = NULL;

  StringCchCopy(wndData->szPath, MAX_PATH, pszPath);

fail:
  SAFE_RELEASE(pConvertedSourceBitmap);
  SAFE_RELEASE(pD2DBitmap);
  SAFE_RELEASE(pDecoder);
  SAFE_RELEASE(pFrame);

  return hr;
}

BOOL WstringComparator(void *str1, void *str2)
{
  return wcscmp(str1, str2) > 0;
}

BOOL NextFileInDir(LPRENDERCTLDATA pRenderCtl, BOOL fNext, LPWSTR lpPathOut) {
  WCHAR szDir[MAX_PATH] = { 0 };
  WCHAR szMask[MAX_PATH] = { 0 };
  WCHAR szPath[MAX_PATH] = { 0 };
  LPWSTR pszFileName = NULL;

  /* Get file directory */
  GetFullPathName(pRenderCtl->szPath, MAX_PATH, szDir, &pszFileName);
  PathCchRemoveFileSpec(szDir, MAX_PATH);

  /* Prepare search filter mask string */
  StringCchCopy(szMask, MAX_PATH, szDir);
  PathCchAppend(szMask, MAX_PATH, L"*.*");

  /* Search files in folder by specified mask */
  WIN32_FIND_DATA ffd = { 0 };

  HANDLE hSearch;
  hSearch = FindFirstFile(szMask, &ffd);

  DOUBLE_LINK_LIST dirList = { 0 };
  dirList.pfnSort = WstringComparator;

  if (hSearch == INVALID_HANDLE_VALUE)
    return FALSE;

  do {
    /* Skip special paths */
    if (!(wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L"..")))
      continue;

    /* Concatenate base path and filename */
    StringCchCopy(szPath, MAX_PATH, szDir);
    PathCchAppend(szPath, MAX_PATH, ffd.cFileName);

    /* Make path string shared */
    LPWSTR lpszPath = calloc(1, sizeof(szPath));
    StringCchCopy(lpszPath, MAX_PATH, szPath);

    /* Append path to list */
    DoubleLinkList_AppendFront(&dirList, lpszPath, TRUE);
    // MessageBox(NULL, lpszPath, L"Dir file list", MB_OK);

  } while (FindNextFile(hSearch, &ffd));

  FindClose(hSearch);

  /* Sort the filenames in alphabetical order */
  DoubleLinkList_Sort(&dirList);

  void *pNextPathData = NULL;
  for (DOUBLE_LINK_NODE *node = dirList.pBegin; node; node = node->pNext) {
    if (!(wcscmp(node->pData, pRenderCtl->szPath))) {
      if (fNext) {
        if (node->pNext) {
          pNextPathData = node->pNext->pData;
        }
        else {
          pNextPathData = dirList.pBegin;
        }
      }
      else {
        if (node->pPrev) {
          pNextPathData = node->pPrev->pData;
        }
        else {
          pNextPathData = dirList.pEnd;
        }
      }

      break;
    }
  }

  if (pNextPathData) {
    StringCchCopy(lpPathOut, MAX_PATH, pNextPathData);
  }

  /* Destroy the list */
  for (DOUBLE_LINK_NODE *node = dirList.pBegin; node;) {
    if (node->pNext) {
      node->pNext->pPrev = NULL;
    }

    if (node->pData) {
      free(node->pData);
    }

    DOUBLE_LINK_NODE *next = node->pNext;
    free(node);
    node = next;
  }

  return TRUE;
}

INT_PTR CALLBACK AboutDlgProc(HWND hWnd, UINT message, WPARAM wParam,
    LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);

  switch (message)
  {
    case WM_NOTIFY:
      {
        switch (((LPNMHDR)lParam)->code)
        {
          case NM_CLICK:
          case NM_RETURN:
            {
              PNMLINK pNMLink = (PNMLINK)lParam;
              LITEM item = pNMLink->item;

              if (((LPNMHDR)lParam)->idFrom == IDC_ABOUTTEXT) {
                ShellExecute(NULL, L"open", item.szUrl, NULL, NULL, SW_SHOW);
              }
            }
            break;
        }
      }
      return TRUE;

    case WM_COMMAND:
      {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
          EndDialog(hWnd, 0);
          return TRUE;
        }
      }
      break;
  }

  return FALSE;
}

INT_PTR CALLBACK SettingsDlgProc(HWND hWnd, UINT message, WPARAM wParam,
    LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);

  switch (message)
  {
    case WM_INITDIALOG:
      {
        HWND hList = GetDlgItem(hWnd, IDC_NAVIASSOCLIST);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_DOUBLEBUFFER |
            LVS_EX_FULLROWSELECT | LVS_EX_JUSTIFYCOLUMNS);

        LVCOLUMN lvc = { 0 };
        int iCol = 0;

        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvc.fmt = LVCFMT_LEFT;
        lvc.cx = 100;

        lvc.pszText = L"Extension";
        lvc.iSubItem = iCol;
        ListView_InsertColumn(hList, iCol++, &lvc);

        lvc.pszText = L"Type";
        lvc.iSubItem = iCol;
        ListView_InsertColumn(hList, iCol++, &lvc);
      }
      return TRUE;

    case WM_COMMAND:
      {
        if (LOWORD(wParam) == IDC_CLEARSETTINGS) {
          int iAnswer;
          iAnswer = MessageBox(hWnd, L"Clear all settings?", L"Confirmation",
              MB_YESNO | MB_ICONQUESTION);

          if (iAnswer == IDYES) {
            MessageBox(hWnd, L"Cleared", L"Info", MB_ICONINFORMATION);
          }
        }
        else if (LOWORD(wParam) == IDOK ||
            LOWORD(wParam) == IDCANCEL)
        {
          EndDialog(hWnd, 0);
          return TRUE;
        }
      }
      break;
  }

  return FALSE;
}

INT_PTR CALLBACK EULADlgProc(HWND hWnd, UINT message, WPARAM wParam,
    LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);

  switch (message)
  {
    case WM_COMMAND:
      {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
          EndDialog(hWnd, LOWORD(wParam));
          return TRUE;
        }
      }
      break;
  }

  return FALSE;
}
