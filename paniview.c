/*
 *  PaniView
 *  A lightweight image viewer for Windows
 * 
 *  Copyright (c) 2021-2023 Aragajaga, Philosoph228 <philosoph228@gmail.com>
 */

#include "precomp.h"
#include "resource.h"

#include "dlnklist.h"

#include <GL/glew.h>
#include <GL/wglew.h>

/* Custom window message definitions for render control */
#define WM_SENDNUDES  WM_USER + 1
#define WM_FILENEXT   WM_USER + 2
#define WM_FILEPREV   WM_USER + 3
#define WM_ZOOMOUT    WM_USER + 4
#define WM_ZOOMIN     WM_USER + 5
#define WM_ACTUALSIZE WM_USER + 6
#define WM_FIT        WM_USER + 7

/* COM object releaser */
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

typedef struct _tagRENDERCTLDATA RENDERCTLDATA, *LPRENDERCTLDATA;
typedef struct _tagRENDERERCONTEXT RENDERERCONTEXT, *LPRENDERERCONTEXT;

/* Base renderer context data structure */
struct _tagRENDERERCONTEXT {
  void (*Release)(LPRENDERERCONTEXT pRendererContext);
  void (*Resize)(LPRENDERERCONTEXT pRendererContext, int cx, int cy);
  void (*Draw)(LPRENDERERCONTEXT pRendererContext, LPRENDERCTLDATA pRenderCtl, HWND hWnd);
  void (*LoadWICBitmap)(LPRENDERERCONTEXT pRendererContext, IWICBitmapSource* pIWICBitmapSource);
};

/* Render control data */
struct _tagRENDERCTLDATA {
  WCHAR szPath[MAX_PATH];
  HWND m_hWnd;
  LPRENDERERCONTEXT m_rendererContext;
  IWICImagingFactory* m_pIWICFactory;
  IWICFormatConverter* m_pConvertedSourceBitmap;
};

/*
 *  Renderer type enum
 *  Used when loading/saving renderer type from settings
 */
enum {
  RENDERER_D2D = 1,
  RENDERER_OPENGL = 2,
  RENDERER_GDI = 3,
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
  TOOLBARTHEME_MODERN_24PX = 4,
  TOOLBARTHEME_FUGUEICONS_24PX = 5,
};

enum {
  MIME_UNKNOWN = 0,
  MIME_IMAGE_PNG = 1,
  MIME_IMAGE_JPG = 2,
  MIME_IMAGE_GIF = 3,
  MIME_IMAGE_WEBP = 4,
  MIME_IMAGE_PGM = 5,
};

/*
 *  Settings data struct
 *  This will be saved as binary to the settings.dat
 */
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
  BOOL bFit;
} SETTINGS, *LPSETTINGS;

typedef struct _tagNAVIASSOCENTRY {
  WCHAR szExtension[80];
  int nMimeType;
} NAVIASSOCENTRY;

typedef struct _tagPANIVIEWAPP {
  HWND hWndMain;
  SETTINGS m_settings;
  PWSTR m_appDataSite;
  PWSTR m_appCfgPath;
} PANIVIEWAPP, *LPPANIVIEWAPP;

HINSTANCE g_hInst;

static const char g_cfgMagic[4] = { 'P', 'N', 'V', 0xE5 };

const unsigned char g_pngMagic[] = { '‰', 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
const unsigned char g_gifMagic[] = { 'G', 'I', 'F', '8'};
const unsigned char g_jpgMagic[] = { 0xFF, 0xD8, 0xFF };
const unsigned char g_webpMagic[] = { 'R', 'I', 'F', 'F', 'W', 'E', 'B', 'P' };
const unsigned char g_pgmMagic[] = { 'P', '5' };

const WCHAR szPaniView[] = L"PaniView";
const WCHAR szPaniViewClassName[] = L"PaniView_Main";
const WCHAR szRenderCtlClassName[] = L"_D2DRenderCtl78";

PWSTR g_pszAboutString;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow);

BOOL InitInstance(HINSTANCE);
void PopupError(DWORD, HWND);
BOOL PopupEULADialog();
BOOL GetApplicationDataSitePath(PWSTR*);

unsigned long crc32(unsigned char *, size_t);

/* Direct2D Utility functions forward declaration */
static inline D2D1_MATRIX_3X2_F D2DUtilMatrixIdentity();
static inline D2D1_MATRIX_3X2_F D2DUtilMakeTranslationMatrix(D2D1_SIZE_F);
static inline D2D1_MATRIX_3X2_F D2DUtilMatrixMultiply(D2D1_MATRIX_3X2_F *, D2D_MATRIX_3X2_F *);
static inline D2D1_COLOR_F D2DColorFromRGBAndAlpha(COLORREF, float);
static inline D2D1_COLOR_F D2DColorFromRGB(COLORREF);

BOOL Settings_LoadFile(SETTINGS *pSettings, PWSTR pszPath);
BOOL Settings_SaveFile(SETTINGS *pSettings, PWSTR pszPath);
BOOL Settings_LoadDefault(SETTINGS *pSettings);

/* Application object methods forward declarations */
static inline LPPANIVIEWAPP GetApp();
BOOL PaniViewApp_Initialize(LPPANIVIEWAPP pApp);
PWSTR PaniViewApp_GetAppDataSitePath(LPPANIVIEWAPP pApp);
PWSTR PaniViewApp_GetSettingsFilePath(LPPANIVIEWAPP pApp);
BOOL PaniViewApp_CreateAppDataSite(LPPANIVIEWAPP pApp);
BOOL PaniViewApp_LoadSettings(LPPANIVIEWAPP pApp);
BOOL PaniViewApp_SaveSettings(LPPANIVIEWAPP pApp);
BOOL PaniViewApp_LoadDefaultSettings(LPPANIVIEWAPP pApp);

/* Application window forward-declared methods */
BOOL PaniView_RegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK PaniView_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL PaniView_OnCreate(HWND hWnd, LPCREATESTRUCT lpcs);
void PaniView_OnSize(HWND hWnd, UINT state, int cx, int cy);
void PaniView_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify);
void PaniView_OnPaint(HWND hWnd);
void PaniView_OnDestroy(HWND hWnd);

/* Renderer control forward-declared methods */
BOOL RenderCtl_RegisterClass(HINSTANCE);
LRESULT CALLBACK RenderCtl_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL RenderCtl_OnCreate(HWND hWnd, LPCREATESTRUCT);
void RenderCtl_OnSize(HWND hWnd, UINT state, int cx, int cy);
void RenderCtl_OnPaint(HWND hWnd);
void RenderCtl_OnDestroy(HWND hWnd);
void RenderCtl_OnSendNudes(HWND hWnd);
void RenderCtl_OnFilePrev(HWND hWnd);
void RenderCtl_OnFileNext(HWND hWnd);
void RenderCtl_OnFitCmd(HWND hWnd);
HRESULT RenderCtl_LoadFromFile(LPRENDERCTLDATA, LPWSTR);
HRESULT RenderCtl_LoadFromFilePGM(LPRENDERCTLDATA, PWSTR, FILE*);
HRESULT RenderCtl_LoadFromFileWIC(LPRENDERCTLDATA, LPWSTR);

/* Direct2D renderer context data structure */
typedef struct _tagD2DRENDERERCONTEXT {
  RENDERERCONTEXT base;

  ID2D1Factory* m_pD2DFactory;
  ID2D1HwndRenderTarget* m_pRenderTarget;
  ID2D1Bitmap* m_pD2DBitmap;

  ID2D1SolidColorBrush* m_pLightSlateGrayBrush;
  ID2D1SolidColorBrush* m_pCornflowerBlueBrush;
} D2DRENDERERCONTEXT, * LPD2DRENDERERCONTEXT;

/* Direct2D Renderer context forward declarations */
HRESULT D2DRendererContext_CreateDeviceResources(LPD2DRENDERERCONTEXT pD2DRendererContext, HWND hWnd);
void D2DRendererContext_DiscardDeviceResources(LPD2DRENDERERCONTEXT pD2DRendererContext);
void D2DRendererContext_Draw(LPD2DRENDERERCONTEXT pD2DRendererContext, LPRENDERCTLDATA pRenderCtl, HWND hWnd);
void D2DRendererContext_LoadWICBitmap(LPD2DRENDERERCONTEXT pD2DRendererContext, IWICBitmapSource* pIWICBitmapSource);
void D2DRendererContext_Release(LPD2DRENDERERCONTEXT pD2DRendererContext);
void D2DRendererContext_Resize(LPD2DRENDERERCONTEXT pD2DRendererContext, int cx, int cy);
void InitializeD2DRendererContextStruct(LPD2DRENDERERCONTEXT pD2DRendererContext);
LPD2DRENDERERCONTEXT CreateD2DRenderer();

/* OpenGL renderer context data structure */
typedef struct _tagOPENGLRENDERERCONTEXT {
  RENDERERCONTEXT base;

  HGLRC m_hGLContext;
  GLuint m_textureId;
  GLuint m_programId;

  GLuint m_vertexArray;
  GLuint m_vertexBuffer;
  GLuint m_uvBuffer;

  float m_viewportWidth;
  float m_viewportHeight;
  float m_imageWidth;
  float m_imageHeight;
} OPENGLRENDERERCONTEXT, * LPOPENGLRENDERERCONTEXT;

/* OpenGL Renderer context forward declarations */
void OpenGLRendererContext_CreateVBO(LPOPENGLRENDERERCONTEXT pGLRendererContext);
void OpenGLRendererContext_CreateDeviceResources(LPOPENGLRENDERERCONTEXT pGLRendererContext, LPRENDERCTLDATA pWndData);
void OpenGLRendererContext_CreateTexture(LPOPENGLRENDERERCONTEXT pGLRendererContext);
void OpenGLRendererContext_Draw(LPOPENGLRENDERERCONTEXT pGLRendererContext, LPRENDERCTLDATA pWndData, HWND hWnd);
void OpenGLRendererContext_DrawVBO(LPOPENGLRENDERERCONTEXT pGLRendererContext);
GLuint OpenGLRendererContext_LoadShader(LPOPENGLRENDERERCONTEXT lpGLRendererContext, PCWSTR shaderFilePath, GLuint shaderType);
GLuint OpenGLRendererContext_LoadShaders(LPOPENGLRENDERERCONTEXT pGLRendererContext, PCWSTR vertexFilePath, PCWSTR fragmentFilePath);
void OpenGLRendererContext_LoadWICBitmap(LPOPENGLRENDERERCONTEXT pGLRendererContext, IWICBitmapSource* pBitmapSource);
void OpenGLRendererContext_Release(LPOPENGLRENDERERCONTEXT pGLRendererContext);
void OpenGLRendererContext_Resize(LPOPENGLRENDERERCONTEXT pGLRendererContext, int cx, int cy);
void InitializeOpenGLRendererContextStruct(LPOPENGLRENDERERCONTEXT pGLRendererContext);
LPOPENGLRENDERERCONTEXT CreateOpenGLRenderer();

/* GDI renderer context data structure */
typedef struct _tagGDIRENDERERCONTEXT {
  RENDERERCONTEXT base;

  HBITMAP m_hBitmap;
  int m_width;
  int m_height;
} GDIRENDERERCONTEXT, * LPGDIRENDERERCONTEXT;

/* GDI Renderer context forward declarations */
void GDIRendererContext_Draw(LPGDIRENDERERCONTEXT pGDIRendererContext, LPRENDERCTLDATA pWndData, HWND hWnd);
void GDIRendererContext_LoadWICBitmap(LPGDIRENDERERCONTEXT pGDIRendererContext, IWICBitmapSource* pBitmapSource);
void GDIRendererContext_Release(LPGDIRENDERERCONTEXT pGDIRendererContext);
void GDIRendererContext_Resize(LPGDIRENDERERCONTEXT pGDIRendererContext, int cx, int cy);
void InitializeGDIRendererContextStruct(LPGDIRENDERERCONTEXT pGDIRendererContext);
LPGDIRENDERERCONTEXT CreateGDIRenderer();

HRESULT InvokeFileOpenDialog(LPWSTR*);

int GetFileMIMEType(PCWSTR pszPath);
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
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
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

  LPPANIVIEWAPP pApp = GetApp();
  if (!PaniViewApp_Initialize(pApp)) {
    return -1;
  }

  InitInstance(hInstance); /* Initialize the application's instance — register
                              classes, lock single instance mutexes, etc. */

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

  pApp->hWndMain = hWndMain;

  ShowWindow(hWndMain, nCmdShow);
  UpdateWindow(hWndMain); /* Force window contents paint inplace after
                             creating*/

  // Open file from shell command
  if (lpCmdLine[0] != L'\0') {
    int nArgs = 0;
    PWSTR* ppszArgv = CommandLineToArgvW(lpCmdLine, &nArgs);
    if (nArgs > 0) {
      PathUnquoteSpaces(ppszArgv[0]);

      LPMAINFRAMEDATA pMainWndData = NULL;
      pMainWndData = (LPMAINFRAMEDATA)GetWindowLongPtr(hWndMain, 0);

      if (pMainWndData) {
        HWND hRenderer;
        hRenderer = pMainWndData->hRenderer;

        if (hRenderer && IsWindow(hRenderer)) {
          LPRENDERCTLDATA pRenderCtlData = NULL;
          pRenderCtlData = (LPRENDERCTLDATA)GetWindowLongPtr(hRenderer, 0);

          RenderCtl_LoadFromFile(pRenderCtlData, ppszArgv[0]);
          InvalidateRect(hWndMain, NULL, FALSE);
        }
      }
    }
  }

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

  if (!PaniViewApp_SaveSettings(pApp)) {
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

static inline D2D1_COLOR_F D2DColorFromRGBAndAlpha(COLORREF color, float alpha)
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

BOOL Settings_LoadFile(SETTINGS *pSettings, PWSTR pszPath)
{
  if (!(pSettings && pszPath)) {
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
        memcpy(pSettings, tmpCfg, sizeof(SETTINGS));
        bStatus = TRUE;
      }
    }
  }

  free(tmpCfg);
  fclose(pfd);
  return bStatus;
}

BOOL Settings_SaveFile(SETTINGS *pSettings, PWSTR pszPath)
{
  if (!(pSettings && pszPath)) {
    return FALSE;
  }

  FILE *pfd = _wfopen(pszPath, L"wb");
  if (!pfd) {
    assert(FALSE);
    return FALSE;
  }

  memcpy(&pSettings->magic, g_cfgMagic, sizeof(g_cfgMagic));
  pSettings->checksum = 0xFFFFFFFF;
  pSettings->checksum = crc32((unsigned char *)pSettings, sizeof(SETTINGS));

  fwrite(pSettings, sizeof(SETTINGS), 1, pfd);
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

static inline LPPANIVIEWAPP GetApp()
{
  static LPPANIVIEWAPP s_gApp = NULL;

  if (!s_gApp) {
    s_gApp = calloc(1, sizeof(PANIVIEWAPP));
  }

  return s_gApp;
}

BOOL PaniViewApp_Initialize(LPPANIVIEWAPP pApp)
{
  if (!PaniViewApp_LoadSettings(pApp)) {
    if (PaniViewApp_LoadDefaultSettings(pApp))
    {
      if (!pApp->m_settings.bEulaAccepted)
      {
        BOOL bAccepted = PopupEULADialog();
        if (bAccepted) {
            pApp->m_settings.bEulaAccepted = TRUE;
        }
        else {
            return FALSE;
        }
      }

      PWSTR pszDataPath = PaniViewApp_GetAppDataSitePath(pApp);
      SHCreateDirectoryEx(pApp->hWndMain, pszDataPath, NULL);
      PaniViewApp_SaveSettings(pApp);
    }
    else {
      return FALSE;
    }
  }

  return TRUE;
}

PWSTR PaniViewApp_GetAppDataSitePath(LPPANIVIEWAPP pApp) {
  if (!pApp->m_appDataSite) {
    GetApplicationDataSitePath(&pApp->m_appDataSite);
  }

  return pApp->m_appDataSite;
}

PWSTR PaniViewApp_GetSettingsFilePath(LPPANIVIEWAPP pApp) {
  static const WCHAR szCfgFileName[] = L"settings.dat";

  PWSTR pszCfgPath = NULL;

  if (!pApp->m_appCfgPath) {
    PWSTR pszSite = PaniViewApp_GetAppDataSitePath(pApp);

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

BOOL PaniViewApp_CreateAppDataSite(LPPANIVIEWAPP pApp) {
  PWSTR pszSite;

  pszSite = PaniViewApp_GetAppDataSitePath(pApp);
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

BOOL PaniViewApp_LoadSettings(LPPANIVIEWAPP pApp)
{
  if (!pApp) {
    return FALSE;
  }

  PWSTR pszCfgPath = PaniViewApp_GetSettingsFilePath(pApp);
  if (!pszCfgPath) {
    return FALSE;
  }

  return Settings_LoadFile(&pApp->m_settings, pszCfgPath);
}

BOOL PaniViewApp_SaveSettings(LPPANIVIEWAPP pApp)
{
  if (!pApp) {
    return FALSE;
  }

  PWSTR pszCfgPath = PaniViewApp_GetSettingsFilePath(pApp);
  if (!pszCfgPath) {
    return FALSE;
  }

  return Settings_SaveFile(&pApp->m_settings, pszCfgPath);
}

BOOL PaniViewApp_LoadDefaultSettings(LPPANIVIEWAPP pApp)
{
  if (!pApp) {
    return FALSE;
  }

  return Settings_LoadDefault(&pApp->m_settings);
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

  HIMAGELIST hImageList = ImageList_LoadImage(
      g_hInst,
      MAKEINTRESOURCE(IDB_TOOLBARSTRIP24),
      24, /* Width */
      0,  /* Do not reserve space for growth */
      CLR_DEFAULT, /* No transparency mask */
      IMAGE_BITMAP, /* Type of image to load */
      LR_CREATEDIBSECTION | LR_DEFAULTCOLOR);
  assert(hImageList);

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
    { 6, 0, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR)L"Rotate" },
    { 7, 0, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0,
        (INT_PTR)L"Pan" },
  };

  /* Add buttons */
  SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)8, (LPARAM)&tbButtons);

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

size_t pwGetFileSize(FILE* fp)
{
  size_t size = 0;

  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  return size;
}

void OrthoMatrix(GLfloat mat[4][4], GLfloat width, GLfloat height)
{
  memset(&mat[0][0], 0, sizeof(mat));

  mat[0][0] = 2.0f / -width;
  mat[1][1] = 2.0f / -height;
  mat[3][3] = 1.0f;
}

void MatrixMultiply(GLfloat mat1[4][4], GLfloat mat2[4][4], GLfloat mat3[4][4])
{
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      mat3[i][j] = 0;
      for (int k = 0; k < 4; ++k) {
        mat3[i][k] += mat1[i][k] * mat2[k][j];
      }
    }
  }
}

int Rect_GetWidth(LPRECT prc)
{
  return prc->right - prc->left;
}

int Rect_GetHeight(LPRECT prc)
{
  return prc->bottom - prc->top;
}

BOOL RectBlt(HDC hdcDest, RECT rcDest, HDC hdcSrc, RECT rcSrc, DWORD rop)
{
  if (rcDest.left < 0)
  {
    float factor = (float)abs(rcDest.left) / (float)(abs(rcDest.left) + rcDest.right);

    rcSrc.left = rcSrc.right * factor;
    rcSrc.right -= rcSrc.left;
    rcDest.left = 0;

  }

  if (rcDest.top < 0)
  {
    float factor = (float)abs(rcDest.top) / (float)(abs(rcDest.top) + rcDest.bottom);

    rcSrc.top = rcSrc.bottom * factor;
    rcSrc.bottom -= rcSrc.top;
    rcDest.top = 0;
  }

  return StretchBlt(hdcDest,
    rcDest.left,
    rcDest.top,
    Rect_GetWidth(&rcDest),
    Rect_GetHeight(&rcDest),
    hdcSrc,
    rcSrc.left, rcSrc.top, rcSrc.right, rcSrc.bottom,
    rop);
}

void RectMatrixMultiply(LPRECT prc, float mat[4][4])
{
  prc->left *= mat[0][0];
  prc->right *= mat[0][0];
  prc->top *= mat[1][1];
  prc->bottom *= mat[1][1];
}

void RectTranslate(LPRECT prc, float x, float y)
{
  prc->left += (LONG)x;
  prc->right += (LONG)x;
  prc->top += (LONG)y;
  prc->bottom += (LONG)y;
}

/*********************************
 *  Direct2D Renderer functions  *
 *********************************/

HRESULT D2DRendererContext_CreateDeviceResources(LPD2DRENDERERCONTEXT pD2DRendererContext, HWND hWnd)
{
  ID2D1HwndRenderTarget** ppRenderTarget = &pD2DRendererContext->m_pRenderTarget;
  ID2D1Factory** ppD2DFactory = &pD2DRendererContext->m_pD2DFactory;

  HRESULT hr = S_OK;

  if (!*ppRenderTarget) {
    RECT rcClient;
    D2D1_SIZE_U size;

    GetClientRect(hWnd, &rcClient);
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
    rtHwndProp.hwnd = hWnd;
    rtHwndProp.pixelSize = size; /* CHECK */
    rtHwndProp.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

    /* Create a Direct2D render target */
    hr = dxID2D1Factory_CreateHwndRenderTarget(
      *ppD2DFactory,
      &rtProp,
      &rtHwndProp,
      ppRenderTarget
    );
  }

  return hr;
}

void D2DRendererContext_DiscardDeviceResources(LPD2DRENDERERCONTEXT pD2DRendererContext)
{
  SAFE_RELEASE(pD2DRendererContext->m_pRenderTarget);
}

void D2DRendererContext_Draw(LPD2DRENDERERCONTEXT pD2DRendererContext, LPRENDERCTLDATA pRenderCtl, HWND hWnd)
{
  ID2D1HwndRenderTarget** ppRenderTarget = &pD2DRendererContext->m_pRenderTarget;
  IWICFormatConverter** ppConvertedSourceBitmap = &pRenderCtl->m_pConvertedSourceBitmap;
  ID2D1Bitmap** ppD2DBitmap = &pD2DRendererContext->m_pD2DBitmap;

  HRESULT hr = S_OK;

  hr = D2DRendererContext_CreateDeviceResources(pD2DRendererContext, hWnd);
  if (SUCCEEDED(hr)) {

    dxID2D1RenderTarget_BeginDraw((ID2D1RenderTarget*)*ppRenderTarget);

    D2D1_MATRIX_3X2_F mat = D2DUtilMatrixIdentity();

    dxID2D1RenderTarget_SetTransform((ID2D1RenderTarget*)*ppRenderTarget, &mat);

    D2D1_COLOR_F clearColor = D2DColorFromRGB(RGB(0xFF, 0xFF, 0xFF));

    dxID2D1RenderTarget_Clear((ID2D1RenderTarget*)*ppRenderTarget, &clearColor);

    /* Retrieve the size of the drawing area. */
    D2D1_SIZE_F rtSize;
    rtSize = dxID2D1HwndRenderTarget_GetSize(*ppRenderTarget);

    /*  D2DBitmap may have been released due to device loss.
     *  If so, re-create it from the source bitmap */
    if (*ppConvertedSourceBitmap && !*ppD2DBitmap)
    {
      dxID2D1RenderTarget_CreateBitmapFromWicBitmap(
        (ID2D1RenderTarget*)*ppRenderTarget,
        (IWICBitmapSource*)*ppConvertedSourceBitmap,
        NULL,
        ppD2DBitmap
      );
    }

    /* Draw an image and scale it to the current window size */
    if (*ppD2DBitmap)
    {
      D2D1_MATRIX_3X2_F matAnchor;
      D2D1_MATRIX_3X2_F matPosition;
      D2D1_MATRIX_3X2_F mat;

      D2D1_SIZE_F bmpSize;
      bmpSize = dxID2D1Bitmap_GetSize(*ppD2DBitmap);

      LPPANIVIEWAPP pApp = GetApp();
      if (pApp->m_settings.bFit) {
        if (bmpSize.width > rtSize.width) {
          float ratio = bmpSize.height / bmpSize.width;
          bmpSize.width = rtSize.width;
          bmpSize.height = rtSize.width * ratio;
        }

        if (bmpSize.height > rtSize.height) {
          float ratio = bmpSize.width / bmpSize.height;
          bmpSize.width = rtSize.height * ratio;
          bmpSize.height = rtSize.height;
        }
      }

      matAnchor = D2DUtilMakeTranslationMatrix((D2D1_SIZE_F) {
        -(bmpSize.width / 2.0f),
          -(bmpSize.height / 2.0f)
      });

      matPosition = D2DUtilMakeTranslationMatrix((D2D1_SIZE_F) {
        roundf(rtSize.width / 2.0f),
          roundf(rtSize.height / 2.0f)
      });

      mat = D2DUtilMatrixMultiply(&matAnchor, &matPosition);

      dxID2D1RenderTarget_SetTransform((ID2D1RenderTarget*)*ppRenderTarget, &mat);

      D2D1_RECT_F imgRect = {
        0.0f, 0.0f,
        bmpSize.width, bmpSize.height
      };

      dxID2D1RenderTarget_DrawBitmap(
        (ID2D1RenderTarget*)*ppRenderTarget,
        *ppD2DBitmap,
        &imgRect, /* Destination rectangle */
        1.0f, /* Opacity */
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        NULL);  /* No source rectangle (full image) */
    }

    hr = dxID2D1RenderTarget_EndDraw((ID2D1RenderTarget*)*ppRenderTarget, NULL, NULL);
    if (hr == (HRESULT)D2DERR_RECREATE_TARGET) {
      hr = S_OK;
      D2DRendererContext_DiscardDeviceResources(pD2DRendererContext);
    }
  }
}

void D2DRendererContext_LoadWICBitmap(LPD2DRENDERERCONTEXT pD2DRendererContext, IWICBitmapSource* pIWICBitmapSource)
{
  HRESULT hr = S_OK;

  ID2D1HwndRenderTarget** ppRenderTarget = &pD2DRendererContext->m_pRenderTarget;
  ID2D1Bitmap** ppD2DBitmap = &pD2DRendererContext->m_pD2DBitmap;
  ID2D1Bitmap* pD2DDirtyBitmap = NULL;

  hr = dxID2D1RenderTarget_CreateBitmapFromWicBitmap(
    (ID2D1RenderTarget*)*ppRenderTarget,
    pIWICBitmapSource,
    NULL,
    &pD2DDirtyBitmap
  );

  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  /* Done successfully. Actually do the changes into structure. */
  SAFE_RELEASE(*ppD2DBitmap);
  *ppD2DBitmap = pD2DDirtyBitmap;

fail:
  // SAFE_RELEASE(pD2DDirtyBitmap);

  return hr;
}

void D2DRendererContext_Release(LPD2DRENDERERCONTEXT pD2DRendererContext)
{
  SAFE_RELEASE(pD2DRendererContext->m_pD2DFactory);
  SAFE_RELEASE(pD2DRendererContext->m_pRenderTarget);
  SAFE_RELEASE(pD2DRendererContext->m_pLightSlateGrayBrush);
  SAFE_RELEASE(pD2DRendererContext->m_pCornflowerBlueBrush);

  free(pD2DRendererContext);
}

void D2DRendererContext_Resize(LPD2DRENDERERCONTEXT pD2DRendererContext, int cx, int cy)
{
  ID2D1HwndRenderTarget** ppRenderTarget = &pD2DRendererContext->m_pRenderTarget;

  if (*ppRenderTarget)
  {
    dxID2D1HwndRenderTarget_Resize(*ppRenderTarget, &(D2D1_SIZE_U){ cx, cy });
  }
}

void InitializeD2DRendererContextStruct(LPD2DRENDERERCONTEXT pD2DRendererContext)
{
  pD2DRendererContext->base.Release = (void (*)(LPRENDERERCONTEXT))& D2DRendererContext_Release;
  pD2DRendererContext->base.Resize = (void (*)(LPRENDERERCONTEXT, int, int))& D2DRendererContext_Resize;
  pD2DRendererContext->base.Draw = (void (*)(LPRENDERERCONTEXT, LPRENDERCTLDATA, HWND))& D2DRendererContext_Draw;
  pD2DRendererContext->base.LoadWICBitmap = (void (*)(LPRENDERERCONTEXT, IWICBitmapSource*))& D2DRendererContext_LoadWICBitmap;
}

LPD2DRENDERERCONTEXT CreateD2DRenderer()
{
  LPD2DRENDERERCONTEXT pD2DRendererContext = NULL;
  ID2D1Factory** ppD2DFactory = NULL;
  HRESULT hr = S_OK;

  pD2DRendererContext = (LPD2DRENDERERCONTEXT) calloc(1, sizeof(D2DRENDERERCONTEXT));
  if (!pD2DRendererContext) {
    return NULL;
  }

  ppD2DFactory = &pD2DRendererContext->m_pD2DFactory;

  InitializeD2DRendererContextStruct(pD2DRendererContext);

  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (LPVOID) ppD2DFactory);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    /* will wndData be freed on WM_DESTROY after `return` returns FALSE? */
    assert(FALSE);
    return NULL;
  }

  return pD2DRendererContext;
}

/*******************************
 *  OpenGL Renderer functions  *
 *******************************/

const GLfloat g_vertexBufferData[] = {
   1.0f,  1.0f,  0.0f,
  -1.0f,  1.0f,  0.0f,
   1.0f, -1.0f,  0.0f,
  -1.0f, -1.0f,  0.0f,
};

const GLfloat g_uvBufferData[] = {
   1.0f,  0.0f,
   0.0f,  0.0f,
   1.0f,  1.0f,
   0.0f,  1.0f,
};

void OpenGLRendererContext_CreateVBO(LPOPENGLRENDERERCONTEXT pGLRendererContext)
{
  /* Create VBO and bind data to it */
  glGenVertexArrays(1, &pGLRendererContext->m_vertexArray);
  glBindVertexArray(pGLRendererContext->m_vertexArray);

  /* Initialize vertex buffer */
  glGenBuffers(1, &pGLRendererContext->m_vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, pGLRendererContext->m_vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertexBufferData), g_vertexBufferData, GL_STATIC_DRAW);

  /* Initialize uv buffer */
  glGenBuffers(1, &pGLRendererContext->m_uvBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, pGLRendererContext->m_uvBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_uvBufferData), g_uvBufferData, GL_STATIC_DRAW);
}

void OpenGLRendererContext_CreateDeviceResources(LPOPENGLRENDERERCONTEXT pGLRendererContext, LPRENDERCTLDATA pWndData)
{
  if (!pGLRendererContext->m_hGLContext)
  {
    /* Choose HDC pixel format */
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;

    HDC hDC = GetDC(pWndData->m_hWnd);

    int pixFmtId = ChoosePixelFormat(hDC, &pfd);
    if (!pixFmtId) {
      // Wrong pixel format
    }

    if (!SetPixelFormat(hDC, pixFmtId, &pfd)) {
      // Error while setting the pixel format
    }

    HGLRC hGLContext = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hGLContext);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
      free(pGLRendererContext);
      return;
    }

    GLint attribs[] = {
      WGL_CONTEXT_MAJOR_VERSION_ARB,
      3,
      WGL_CONTEXT_MINOR_VERSION_ARB,
      1,
      WGL_CONTEXT_FLAGS_ARB,
      0,
      0
    };

    if (wglewIsSupported("WGL_ARB_create_context") == 1)
    {
      HGLRC hGLEWContext = wglCreateContextAttribsARB(hDC, 0, attribs);

      wglMakeCurrent(NULL, NULL);
      wglDeleteContext(hGLContext);
      pGLRendererContext->m_hGLContext = hGLEWContext;

      wglMakeCurrent(hDC, pGLRendererContext->m_hGLContext);
    }

    if (!pGLRendererContext->m_hGLContext) {
      // Failed to initialize GL context
    }

    static const WCHAR szVertFileName[] = L"vert.glsl";
    static const WCHAR szFragFileName[] = L"frag.glsl";

    LPPANIVIEWAPP pApp = GetApp();
    PWSTR pszSite = PaniViewApp_GetAppDataSitePath(pApp);

    size_t lenVertPath = wcslen(pszSite) + ARRAYSIZE(szVertFileName) + 1;
    PWSTR pszVertPath = (PWSTR)calloc(1, lenVertPath * sizeof(WCHAR));
    if (pszVertPath) {
      StringCchCopy(pszVertPath, lenVertPath, pszSite);
      PathCchAppend(pszVertPath, lenVertPath, szVertFileName);
    }

    size_t lenFragPath = wcslen(pszSite) + ARRAYSIZE(szFragFileName) + 1;
    PWSTR pszFragPath = (PWSTR)calloc(1, lenFragPath * sizeof(WCHAR));
    if (pszFragPath) {
      StringCchCopy(pszFragPath, lenFragPath, pszSite);
      PathCchAppend(pszFragPath, lenFragPath, szFragFileName);
    }

    pGLRendererContext->m_programId = OpenGLRendererContext_LoadShaders(pGLRendererContext, pszVertPath, pszFragPath);

    free(pszVertPath);
    free(pszFragPath);

    OpenGLRendererContext_CreateTexture(pGLRendererContext);
    OpenGLRendererContext_CreateVBO(pGLRendererContext);
  }
}

void OpenGLRendererContext_CreateTexture(LPOPENGLRENDERERCONTEXT pGLRendererContext)
{
  GLuint* textureId = &pGLRendererContext->m_textureId;

  if (!*textureId)
  {
    glGenTextures(1, textureId);
    glBindTexture(GL_TEXTURE_2D, *textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void OpenGLRendererContext_Draw(LPOPENGLRENDERERCONTEXT pGLRendererContext, LPRENDERCTLDATA pWndData, HWND hWnd)
{
  OpenGLRendererContext_CreateDeviceResources(pGLRendererContext, pWndData);

  /* Clear */
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  /* Attach shader */
  glUseProgram(pGLRendererContext->m_programId);

  /* Attach texture */
  glBindTexture(GL_TEXTURE_2D, pGLRendererContext->m_textureId);

  /* Draw plane */
  OpenGLRendererContext_DrawVBO(pGLRendererContext);

  /* Swap buffers */
  HDC hdc = GetDC(hWnd);
  SwapBuffers(hdc);
  ReleaseDC(hWnd, hdc);
}

void OpenGLRendererContext_DrawVBO(LPOPENGLRENDERERCONTEXT pGLRendererContext)
{
  GLuint* vertexArray = &pGLRendererContext->m_vertexArray;
  GLuint* vertexBuffer = &pGLRendererContext->m_vertexBuffer;
  GLuint* uvBuffer = &pGLRendererContext->m_uvBuffer;

  /* Bind vertex array */
  glBindVertexArray(*vertexArray);

  /* Select vertex buffer */
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, *vertexBuffer);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  /* Select uv buffer */
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, *uvBuffer);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);

  float width = pGLRendererContext->m_imageWidth;
  float height = pGLRendererContext->m_imageHeight;

  if (width > pGLRendererContext->m_viewportWidth) {
    float ratio = height / width;
    width = pGLRendererContext->m_viewportWidth;
    height = pGLRendererContext->m_viewportWidth * ratio;
  }

  if (height > pGLRendererContext->m_viewportHeight)
  {
    float ratio = width / height;
    width = pGLRendererContext->m_viewportHeight * ratio;
    height = pGLRendererContext->m_viewportHeight;
  }

  GLfloat scale[4][4] = {
    {  -width / 2.0f,  0.0f,  0.0f,  0.0f },
    {  0.0f,  -height / 2.0f,  0.0f,  0.0f },
    {  0.0f,  0.0f,  0.0f,  0.0f },
    {  0.0f,  0.0f,  0.0f,  1.0f },
  };

  GLfloat ortho[4][4] = { 0 };
  GLfloat transform[4][4] = { 0 };

  OrthoMatrix(ortho, pGLRendererContext->m_viewportWidth, pGLRendererContext->m_viewportHeight);
  MatrixMultiply(scale, ortho, transform);

  GLuint transformUniform = glGetUniformLocation(pGLRendererContext->m_programId, "transform");
  glUniformMatrix4fv(transformUniform, 1, GL_FALSE, &transform[0][0]);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  glBindVertexArray(0);
}

GLuint OpenGLRendererContext_LoadShader(LPOPENGLRENDERERCONTEXT lpGLRendererContext, PCWSTR shaderFilePath, GLuint shaderType)
{
  GLuint shaderId = glCreateShader(shaderType);

  FILE* pShaderFile = NULL;
  pShaderFile = _wfopen(shaderFilePath, L"r");

  if (pShaderFile)
  {
    size_t fileSize = pwGetFileSize(pShaderFile);

    char* pShaderCode = (char*)calloc(1, fileSize);
    fread(pShaderCode, 1, fileSize, pShaderFile);
    pShaderCode[fileSize] = '\0';

    fclose(pShaderFile);

    /* Compile shader */
    glShaderSource(shaderId, 1, (const char**)&pShaderCode, NULL);
    glCompileShader(shaderId);

    GLint result = GL_FALSE;
    int infoLogLength;

    /* Check shader */
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
      char* pShaderErrorMessage = (char*)calloc(1, infoLogLength);
      glGetShaderInfoLog(shaderId, infoLogLength, NULL, pShaderErrorMessage);

      printf("%s\n", pShaderErrorMessage);

      free(pShaderErrorMessage);
      free(pShaderCode);

      exit(EXIT_FAILURE);
    }

    // free(pShaderCode);
  }
  else {
    fprintf(stderr, "Impossible to open %s. Are you in the right directory? Don't forget to read the FAQ!", shaderFilePath);
    exit(EXIT_FAILURE);
  }

  return shaderId;
}

GLuint OpenGLRendererContext_LoadShaders(LPOPENGLRENDERERCONTEXT pGLRendererContext, PCWSTR vertexFilePath, PCWSTR fragmentFilePath)
{
  /* Create shader objects */
  GLuint vertexShaderId;
  GLuint fragmentShaderId;

  vertexShaderId = OpenGLRendererContext_LoadShader(pGLRendererContext, vertexFilePath, GL_VERTEX_SHADER);
  fragmentShaderId = OpenGLRendererContext_LoadShader(pGLRendererContext, fragmentFilePath, GL_FRAGMENT_SHADER);

  /* Link the program */
  GLuint programId = glCreateProgram();
  glAttachShader(programId, vertexShaderId);
  glAttachShader(programId, fragmentShaderId);
  glLinkProgram(programId);

  /* Check the program */
  GLint result = GL_FALSE;
  int infoLogLength;

  glGetProgramiv(programId, GL_LINK_STATUS, &result);
  glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLogLength);

  if (infoLogLength)
  {
    char* programErrorMessage = (char*)calloc(1, infoLogLength + 1);
    glGetProgramInfoLog(programId, infoLogLength, NULL, programErrorMessage);

    printf("%s\n", programErrorMessage);

    free(programErrorMessage);

    exit(EXIT_FAILURE);
  }

  glDetachShader(programId, vertexShaderId);
  glDetachShader(programId, fragmentShaderId);

  glDeleteShader(vertexShaderId);
  glDeleteShader(fragmentShaderId);

  return programId;
}

void OpenGLRendererContext_LoadWICBitmap(LPOPENGLRENDERERCONTEXT pGLRendererContext, IWICBitmapSource* pBitmapSource)
{
  OpenGLRendererContext_CreateTexture(pGLRendererContext);

  UINT width;
  UINT height;
  pBitmapSource->lpVtbl->GetSize(pBitmapSource, &width, &height);

  unsigned char* data = calloc(1, width * height * 4);
  pBitmapSource->lpVtbl->CopyPixels(pBitmapSource, NULL, width * 4, width * height * 4, data);

  glBindTexture(GL_TEXTURE_2D, pGLRendererContext->m_textureId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, (const void*)data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);

  free(data);

  pGLRendererContext->m_imageWidth = (float)width;
  pGLRendererContext->m_imageHeight = (float)height;
}

void OpenGLRendererContext_Release(LPOPENGLRENDERERCONTEXT pGLRendererContext)
{

}

void OpenGLRendererContext_Resize(LPOPENGLRENDERERCONTEXT pGLRendererContext, int cx, int cy)
{
  pGLRendererContext->m_viewportWidth = (float)cx;
  pGLRendererContext->m_viewportHeight = (float)cy;
  glViewport(0, 0, cx, cy);
}

void InitializeOpenGLRendererContextStruct(LPOPENGLRENDERERCONTEXT pGLRendererContext)
{
  pGLRendererContext->base.Release = (void (*)(LPRENDERERCONTEXT)) & OpenGLRendererContext_Release;
  pGLRendererContext->base.Resize = (void (*)(LPRENDERERCONTEXT, int, int)) & OpenGLRendererContext_Resize;
  pGLRendererContext->base.Draw = (void (*)(LPRENDERERCONTEXT, LPRENDERCTLDATA, HWND)) & OpenGLRendererContext_Draw;
  pGLRendererContext->base.LoadWICBitmap = (void (*)(LPRENDERERCONTEXT, IWICBitmapSource*)) & OpenGLRendererContext_LoadWICBitmap;
}

LPOPENGLRENDERERCONTEXT CreateOpenGLRenderer()
{
  LPOPENGLRENDERERCONTEXT pGLRendererContext = NULL;

  pGLRendererContext = (LPOPENGLRENDERERCONTEXT) calloc(1, sizeof(OPENGLRENDERERCONTEXT));
  if (!pGLRendererContext)
  {
    return NULL;
  }
  InitializeOpenGLRendererContextStruct(pGLRendererContext);

  return pGLRendererContext;
}

/****************************
 *  GDI Renderer functions  *
 ****************************/

void GDIRendererContext_Draw(LPGDIRENDERERCONTEXT pGDIRendererContext, LPRENDERCTLDATA pWndData, HWND hWnd)
{
  HDC hdc;
  hdc = GetDC(hWnd);

  HDC hBitmapDC = CreateCompatibleDC(hdc);
  HBITMAP hOldBitmap = (HBITMAP) SelectObject(hBitmapDC, (HGDIOBJ) pGDIRendererContext->m_hBitmap);

  RECT rc = {0};
  GetClientRect(hWnd, &rc);

  FillRect(hdc, &rc, CreateSolidBrush(RGB(0, 0xFF, 0)));

  int imageWidth = pGDIRendererContext->m_width;
  int imageHeight = pGDIRendererContext->m_height;

  int viewX = 0;
  int viewY = 0;
  int viewWidth = Rect_GetWidth(&rc);
  int viewHeight = Rect_GetHeight(&rc);

  RECT rcDest = {0};
  rcDest.left = 0;
  rcDest.top = 0;
  rcDest.right = Rect_GetWidth(&rc);
  rcDest.bottom = Rect_GetHeight(&rc);

  RECT rcSrc = {0};
  rcSrc.left = 0;
  rcSrc.top = 0;
  rcSrc.right = imageWidth;
  rcSrc.bottom = imageHeight;

  RectTranslate(&rcDest, -((float)viewWidth / 2.0f), -((float)viewHeight / 2.0f));

  if (imageWidth > viewWidth) {
    float ratio = imageHeight / (float)imageWidth;
    imageWidth = viewWidth;
    imageHeight = viewWidth * ratio;
  }

  if (imageHeight > viewHeight)
  {
    float ratio = imageWidth / (float)imageHeight;
    imageWidth = viewHeight * ratio;
    imageHeight = viewHeight;
  }

  float mat[4][4] = {
    {  2.0f / (float)viewWidth,  0.0f,  0.0f,  0.0f},
    {  0.0f,  2.0f / (float)viewHeight,  0.0f,  0.0f},
    {  0.0f,  0.0f,  0.0f,  0.0f },
    {  0.0f,  0.0f,  0.0f,  1.0f },
  };
  float mat2[4][4] = {
    { (float)imageWidth / 2.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, (float)imageHeight / 2.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
  };

  float mat3[4][4] = {0};
  MatrixMultiply(mat, mat2, mat3);

  RectMatrixMultiply(&rcDest, mat3);
  RectTranslate(&rcDest, (float)viewWidth / 2.0f, (float)viewHeight / 2.0f);

  // RectTranslate(&rcDest, 40.f, 40.f);

  SetStretchBltMode(hdc, HALFTONE);
  RectBlt(hdc, rcDest, hBitmapDC, rcSrc, SRCCOPY);

  SelectObject(hBitmapDC, (HGDIOBJ) hOldBitmap);

  ReleaseDC(hWnd, hdc);
}

void GDIRendererContext_LoadWICBitmap(LPGDIRENDERERCONTEXT pGDIRendererContext, IWICBitmapSource* pBitmapSource)
{
  UINT width;
  UINT height;
  pBitmapSource->lpVtbl->GetSize(pBitmapSource, &width, &height);
  pGDIRendererContext->m_width = width;
  pGDIRendererContext->m_height = height;

  unsigned char* data = (unsigned char*)calloc(1, width * height * 4);

  pBitmapSource->lpVtbl->CopyPixels(pBitmapSource, NULL, width * 4, width * height * 4, data);

  pGDIRendererContext->m_hBitmap = CreateBitmap(width, height, 1, 32, data);

  free(data);
}

void GDIRendererContext_Release(LPGDIRENDERERCONTEXT pGDIRendererContext)
{
  DeleteObject(pGDIRendererContext->m_hBitmap);

  free(pGDIRendererContext);
}

void GDIRendererContext_Resize(LPGDIRENDERERCONTEXT pGDIRendererContext, int cx, int cy)
{

}

void InitializeGDIRendererContextStruct(LPGDIRENDERERCONTEXT pGDIRendererContext)
{
  pGDIRendererContext->base.Release = (void (*)(LPRENDERERCONTEXT)) & GDIRendererContext_Release;
  pGDIRendererContext->base.Resize = (void (*)(LPRENDERERCONTEXT, int, int)) & GDIRendererContext_Resize;
  pGDIRendererContext->base.Draw = (void (*)(LPRENDERERCONTEXT, LPRENDERCTLDATA, HWND)) & GDIRendererContext_Draw;
  pGDIRendererContext->base.LoadWICBitmap = (void (*)(LPRENDERERCONTEXT, IWICBitmapSource*)) &GDIRendererContext_LoadWICBitmap;
}

LPGDIRENDERERCONTEXT CreateGDIRenderer()
{
  LPGDIRENDERERCONTEXT pGDIRendererContext = NULL;

  pGDIRendererContext = (LPGDIRENDERERCONTEXT)calloc(1, sizeof(GDIRENDERERCONTEXT));
  if (!pGDIRendererContext)
  {
    return NULL;
  }
  InitializeGDIRendererContextStruct(pGDIRendererContext);

  return pGDIRendererContext;
}

/********************
 *  Render Control  *
 ********************/

LPRENDERERCONTEXT CreateRendererContext()
{
  LPPANIVIEWAPP pApp = GetApp();
  LPRENDERERCONTEXT pRendererContext = NULL;

  switch (pApp->m_settings.nRendererType)
  {
  case RENDERER_D2D:
    pRendererContext = (LPRENDERERCONTEXT) CreateD2DRenderer();
    break;

  case RENDERER_OPENGL:
    pRendererContext = (LPRENDERERCONTEXT) CreateOpenGLRenderer();
    break;

  case RENDERER_GDI:
    pRendererContext = (LPRENDERERCONTEXT) CreateGDIRenderer();
    break;
  }

  return pRendererContext;
}

LPRENDERERCONTEXT RenderCtl_GetRendererContext(LPRENDERCTLDATA pWndData)
{
  return pWndData->m_rendererContext;
}

HRESULT RenderCtl_InitializeWIC(LPRENDERCTLDATA wndData)
{
  HRESULT hr = S_OK;
  IWICImagingFactory** ppIWICFactory = &wndData->m_pIWICFactory;

  hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (LPVOID)ppIWICFactory);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    return NULL;
  }

  return hr;
}

BOOL RenderCtl_OnCreate(HWND hWnd, LPCREATESTRUCT lpcs)
{
  UNREFERENCED_PARAMETER(lpcs);

  LPRENDERCTLDATA pWndData = (LPRENDERCTLDATA) calloc(1, sizeof(RENDERCTLDATA));
  pWndData->m_hWnd = hWnd;

  RenderCtl_InitializeWIC(pWndData);
  pWndData->m_rendererContext = CreateRendererContext();

  SetWindowLongPtr(hWnd, 0, (LONG_PTR) pWndData);
  return TRUE;
}

void RenderCtl_OnSize(HWND hWnd, UINT state, int cx, int cy)
{
  UNREFERENCED_PARAMETER(hWnd);
  UNREFERENCED_PARAMETER(state);

  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  LPRENDERERCONTEXT pRendererContext = RenderCtl_GetRendererContext(wndData);
  if (pRendererContext)
  {
    pRendererContext->Resize(pRendererContext, cx, cy);
  }
}

void RenderCtl_OnPaint(HWND hWnd)
{
  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  HRESULT hr = S_OK;

  LPRENDERERCONTEXT pRendererContext = RenderCtl_GetRendererContext(wndData);
  if (pRendererContext)
  {
    pRendererContext->Draw(pRendererContext, wndData, hWnd);
  }

  ValidateRect(hWnd, NULL);
}

void RenderCtl_OnSendNudes(HWND hWnd)
{
  HRESULT hr = S_OK;

  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  LPWSTR szFilePath;
  hr = InvokeFileOpenDialog(&szFilePath);
  if (SUCCEEDED(hr))
  {
    hr = RenderCtl_LoadFromFile(wndData, szFilePath);

    if (FAILED(hr)) {
      PopupError(hr, NULL);
      assert(FALSE);
    }
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

  if (szNextFile[0] != '\0')
  {
    hr = RenderCtl_LoadFromFile(wndData, szNextFile);
    if (FAILED(hr)) {
      PopupError(hr, NULL);
      assert(FALSE);
    }
  }
}

void RenderCtl_OnFitCmd(HWND hWnd)
{
  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  LPPANIVIEWAPP pApp = GetApp();

  assert(wndData);

  pApp->m_settings.bFit = !pApp->m_settings.bFit;
  InvalidateRect(wndData->m_hWnd, NULL, FALSE);
}

void RenderCtl_OnDestroy(HWND hWnd)
{
  LPRENDERCTLDATA wndData = (LPRENDERCTLDATA) GetWindowLongPtr(hWnd, 0);
  assert(wndData);

  LPRENDERERCONTEXT pRendererContext = RenderCtl_GetRendererContext(wndData);
  if (pRendererContext) {
    pRendererContext->Release(pRendererContext);
  }

  free(wndData);
}

COMDLG_FILTERSPEC c_rgReadTypes[] = {
  {L"Common picture file formats", L"*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.pgm"},
  {L"Windows/OS2 Bitmap", L"*.bmp"},
  {L"Portable Network Graphics", L"*.png"},
  {L"Joint Picture Expert Group", L"*.jpg;*.jpeg"},
  {L"CompuServe Graphics Interchange Format", L"*.gif"},
  {L"Portable Graymap Format", L"*.pgm"},
  {L"All files", L"*.*"},
};

HRESULT InvokeFileOpenDialog(LPWSTR *szPath)
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
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
  {
      goto error;
  }
  else if (FAILED(hr))
  {
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

  return hr;
}

HRESULT RenderCtl_LoadFromFile(LPRENDERCTLDATA wndData, LPWSTR pszPath)
{
  FILE *pf = NULL;

  pf = _wfopen(pszPath, L"rb");
  if (!pf)
  {
    MessageBox(NULL, L"Cound not open file", NULL, MB_OK | MB_ICONERROR);
    return E_FAIL;
  }

  const char pgmMagic[] = { 'P', '5' };

  char magic[2];
  fread(magic, sizeof(magic), 1, pf);

  HRESULT hResult = E_FAIL;
  if (!memcmp(magic, pgmMagic, sizeof(magic)))
  {
    hResult = RenderCtl_LoadFromFilePGM(wndData, pszPath, pf);
  }
  else {
    hResult = RenderCtl_LoadFromFileWIC(wndData, pszPath);
  }

  fclose(pf);

  return hResult;
}

IWICBitmapSource* WICLoadFromMemory(LPRENDERCTLDATA pWndData, int width, int height, unsigned char* data, REFWICPixelFormatGUID pixelFormat)
{
  HRESULT hr = S_OK;

  IWICBitmap* pIWICBitmap;
  IWICFormatConverter* pConvertedSourceBitmap = NULL;

  hr = pWndData->m_pIWICFactory->lpVtbl->CreateBitmapFromMemory(
    pWndData->m_pIWICFactory,
    width, height,
    &GUID_WICPixelFormat8bppGray,
    width, width * height,
    (BYTE*) data,
    &pIWICBitmap
  );
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  /* Convert the image to 32bppPBGRA */
  hr = pWndData->m_pIWICFactory->lpVtbl->CreateFormatConverter(
    pWndData->m_pIWICFactory,
    &pConvertedSourceBitmap);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  hr = pConvertedSourceBitmap->lpVtbl->Initialize(
    pConvertedSourceBitmap,
    (IWICBitmapSource*) pIWICBitmap, /* Input bitmap to convert */
    &GUID_WICPixelFormat32bppPBGRA, /* Destination pixel format */
    WICBitmapDitherTypeNone,  /* No dither pattern */
    NULL, /* Do not specify particular color pallete */
    0.0f, /* Alpha threshold */
    WICBitmapPaletteTypeCustom);  /* Palette transform type */
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  SAFE_RELEASE(pWndData->m_pConvertedSourceBitmap);
  pWndData->m_pConvertedSourceBitmap = pConvertedSourceBitmap;

fail:
  if (FAILED(hr)) {
    SAFE_RELEASE(pConvertedSourceBitmap);
  }
  SAFE_RELEASE(pIWICBitmap);

  return pConvertedSourceBitmap;
}

HRESULT RenderCtl_LoadFromFilePGM(LPRENDERCTLDATA wndData, PWSTR pszPath, FILE *pf)
{
  HRESULT hr = E_FAIL;

  IWICBitmap *pIWICBitmap = NULL;
  ID2D1Bitmap *pD2DBitmap = NULL;
  IWICFormatConverter *pConvertedSourceBitmap = NULL;

  int width;
  int height;
  int depth;
  fscanf(pf, "%d %d\n%d\n", &width, &height, &depth);

  if (depth != 255) {
    return E_FAIL;
  }

  size_t dataLength = (size_t)width * height;
  unsigned char *data = (unsigned char *)calloc(dataLength, 1);
  fread(data, dataLength, 1, pf);

  pConvertedSourceBitmap = WICLoadFromMemory(wndData, width, height, data, &GUID_WICPixelFormat8bppGray);
  LPRENDERERCONTEXT pRendererContext = RenderCtl_GetRendererContext(wndData);
  if (pRendererContext) {
    pRendererContext->LoadWICBitmap(pRendererContext, pConvertedSourceBitmap);
  }

  /* Copy path to window data */
  StringCchCopy(wndData->szPath, MAX_PATH, pszPath);

fail:

  free(data);

  return hr;
}

IWICBitmapSource* WICDecodeFromFilename(LPRENDERCTLDATA pWndData, LPWSTR pszPath)
{
  HRESULT hr = S_OK;

  IWICBitmapDecoder* pDecoder = NULL;
  IWICBitmapFrameDecode* pFrame = NULL;
  IWICFormatConverter* pConvertedSourceBitmap = NULL;

  /* Request image decoder COM object */
  hr = pWndData->m_pIWICFactory->lpVtbl->CreateDecoderFromFilename(
    pWndData->m_pIWICFactory,
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
  hr = pWndData->m_pIWICFactory->lpVtbl->CreateFormatConverter(
    pWndData->m_pIWICFactory,
    &pConvertedSourceBitmap);
  if (FAILED(hr)) {
    PopupError(hr, NULL);
    assert(FALSE);
    goto fail;
  }

  hr = pConvertedSourceBitmap->lpVtbl->Initialize(
    pConvertedSourceBitmap,
    (IWICBitmapSource*)pFrame, /* Input bitmap to convert */
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

  SAFE_RELEASE(pWndData->m_pConvertedSourceBitmap);
  pWndData->m_pConvertedSourceBitmap = pConvertedSourceBitmap;

fail:

  SAFE_RELEASE(pDecoder);
  SAFE_RELEASE(pFrame);

  if (FAILED(hr)) {
    SAFE_RELEASE(pConvertedSourceBitmap);
  }

  return pConvertedSourceBitmap;
}

HRESULT RenderCtl_LoadFromFileWIC(LPRENDERCTLDATA wndData, LPWSTR pszPath)
{
  HRESULT hr = S_OK;

  IWICFormatConverter *pConvertedSourceBitmap = NULL;

  pConvertedSourceBitmap = WICDecodeFromFilename(wndData, pszPath);

  LPRENDERERCONTEXT pRendererContext = RenderCtl_GetRendererContext(wndData);
  if (pRendererContext) {
    pRendererContext->LoadWICBitmap(pRendererContext, (IWICBitmapSource *) pConvertedSourceBitmap);
  }

  StringCchCopy(wndData->szPath, MAX_PATH, pszPath);
  return hr;
}

BOOL WstringComparator(void *str1, void *str2)
{
  return wcscmp(str1, str2) > 0;
}

int GetFileMIMEType(PCWSTR pszPath)
{
  FILE* fp = NULL;
  unsigned char magicBuffer[80] = { 0 };
  size_t fileSize = 0;
  int mime = MIME_UNKNOWN;

  fp = _wfopen(pszPath, L"rb");
  if (fp)
  {
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize > 80)
    {
      fread(magicBuffer, 80, 1, fp);

      if (!memcmp(magicBuffer, g_pngMagic, sizeof(g_pngMagic)))
      {
        mime = MIME_IMAGE_PNG;
      }
      else if (!memcmp(magicBuffer, g_gifMagic, sizeof(g_gifMagic)))
      {
        mime = MIME_IMAGE_GIF;
      }
      else if (!memcmp(magicBuffer, g_jpgMagic, sizeof(g_jpgMagic)))
      {
        mime = MIME_IMAGE_JPG;
      }
      else if (!memcmp(magicBuffer, g_webpMagic, 4) && !memcmp(&magicBuffer[8], &g_webpMagic[4], 4))
      {
        mime = MIME_IMAGE_WEBP;
      }
      else if (!memcmp(magicBuffer, g_pgmMagic, sizeof(g_pgmMagic)))
      {
        mime = MIME_IMAGE_PGM;
      }
    }

    fclose(fp);
  }

  return mime;
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
    if (
      !wcscmp(ffd.cFileName, L".") ||
      !wcscmp(ffd.cFileName, L"..") ||
      ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
      )
    {
      continue;
    }

    /* Concatenate base path and filename */
    StringCchCopy(szPath, MAX_PATH, szDir);
    PathCchAppend(szPath, MAX_PATH, ffd.cFileName);

    int mime = GetFileMIMEType(szPath);
    if (!mime) {
      continue;
    }

    /* Make path string shared */
    LPWSTR lpszPath = calloc(1, sizeof(szPath));
    StringCchCopy(lpszPath, MAX_PATH, szPath);

    /* Append path to list */
    DoubleLinkList_AppendFront(&dirList, lpszPath, TRUE);

  } while (FindNextFile(hSearch, &ffd));

  FindClose(hSearch);

  /* Sort the filenames in alphabetical order */
  DoubleLinkList_Sort(&dirList);

  PCWSTR pNextPathData = NULL;
  for (DOUBLE_LINK_NODE *node = dirList.pBegin; node; node = node->pNext) {
    if (!(wcscmp(node->pData, pRenderCtl->szPath))) {
      if (fNext) {
        if (node->pNext && node->pNext->pData) {
          pNextPathData = (PCWSTR) node->pNext->pData;
        }
        else {
          pNextPathData = (PCWSTR) dirList.pBegin->pData;
        }
      }
      else {
        if (node->pPrev && node->pPrev->pData) {
          pNextPathData = (PCWSTR) node->pPrev->pData;
        }
        else {
          pNextPathData = (PCWSTR) dirList.pEnd->pData;
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
    case WM_INITDIALOG:
      {
        HINSTANCE hInstance = GetModuleHandle(NULL);

        HRSRC hResInfo = FindResource(hInstance, MAKEINTRESOURCE(ID_ABOUT_TEXT), RT_RCDATA);
        HGLOBAL hGlob = LoadResource(hInstance, hResInfo);
        DWORD dwSize = SizeofResource(hInstance, hResInfo);

        const char* pszText = LockResource(hGlob);

        int len = MultiByteToWideChar(CP_UTF8, 0, pszText, dwSize, NULL, 0);
        g_pszAboutString = calloc(len, len * sizeof(WCHAR));

        MultiByteToWideChar(CP_UTF8, 0, pszText, dwSize, g_pszAboutString, len);
        SetDlgItemText(hWnd, IDC_ABOUTTEXT, g_pszAboutString);

        FreeResource(hResInfo);
      }
      break;

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
          free(g_pszAboutString);
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
        LPPANIVIEWAPP pApp = GetApp();
        LPSETTINGS pSettings = &pApp->m_settings;

        // Load renderer combo box strings
        int nItem;
        HWND hBackendSel = GetDlgItem(hWnd, IDC_BACKENDSEL);

        nItem = ComboBox_AddString(hBackendSel, L"Direct2D (Default)");
        ComboBox_SetItemData(hBackendSel, nItem, (LPARAM) RENDERER_D2D);

        nItem = ComboBox_AddString(hBackendSel, L"OpenGL");
        ComboBox_SetItemData(hBackendSel, nItem, (LPARAM) RENDERER_OPENGL);

        nItem = ComboBox_AddString(hBackendSel, L"GDI");
        ComboBox_SetItemData(hBackendSel, nItem, (LPARAM) RENDERER_GDI);

        int nCurSel = 0;
        for (int i = 0; i < ComboBox_GetCount(hBackendSel); ++i)
        {
          int rendererType = (int) ComboBox_GetItemData(hBackendSel, i);
          if (rendererType == pSettings->nRendererType)
          {
            nCurSel = i;
            break;
          }
        }

        ComboBox_SetCurSel(hBackendSel, nCurSel);

        // Load toolbar theme combo box
        {
          HWND hToolbarIconSel = GetDlgItem(hWnd, IDC_TOOLBARICONSEL); 

          int nItem;
          nItem = ComboBox_AddString(hToolbarIconSel, L"Classic 16px");
          ComboBox_SetItemData(hToolbarIconSel, nItem,
              (LPARAM) TOOLBARTHEME_DEFAULT_16PX);

          nItem = ComboBox_AddString(hToolbarIconSel, L"Classic 24px (Default)");
          ComboBox_SetItemData(hToolbarIconSel, nItem,
              (LPARAM) TOOLBARTHEME_DEFAULT_24PX);

          nItem = ComboBox_AddString(hToolbarIconSel, L"Classic 32px");
          ComboBox_SetItemData(hToolbarIconSel, nItem,
              (LPARAM) TOOLBARTHEME_DEFAULT_32PX);

          nItem = ComboBox_AddString(hToolbarIconSel, L"Modern");
          ComboBox_SetItemData(hToolbarIconSel, nItem,
              (LPARAM) TOOLBARTHEME_MODERN_24PX);

          nItem = ComboBox_AddString(hToolbarIconSel, L"Fugue Icons");
          ComboBox_SetItemData(hToolbarIconSel, nItem,
              (LPARAM) TOOLBARTHEME_FUGUEICONS_24PX);
        }


        // Initialize file association listview
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
          LPPANIVIEWAPP pApp = GetApp();
          LPSETTINGS pSettings = &pApp->m_settings;

          HWND hRendererSel = GetDlgItem(hWnd, IDC_BACKENDSEL);
          int nCurSel = ComboBox_GetCurSel(hRendererSel);
          int rendererType = (int) ComboBox_GetItemData(hRendererSel, nCurSel);
          
          if (rendererType != pSettings->nRendererType)
          {
            MAINFRAMEDATA* pMainWndData = GetWindowLongPtr(pApp->hWndMain, 0);
            RENDERCTLDATA* pRenderCtlData = GetWindowLongPtr(pMainWndData->hRenderer, 0);
            
            pSettings->nRendererType = rendererType;

            pRenderCtlData->m_rendererContext = CreateRendererContext();
          }          

          EndDialog(hWnd, 0);
          return TRUE;
        }
      }
      break;
  }

  return FALSE;
}

PWSTR g_pszEULAText;

INT_PTR CALLBACK EULADlgProc(HWND hWnd, UINT message, WPARAM wParam,
    LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);

  switch (message)
  {
    case WM_INITDIALOG:
      {
        HINSTANCE hInstance = GetModuleHandle(NULL);

        HRSRC hResInfo = FindResource(hInstance, MAKEINTRESOURCE(ID_EULA_TEXT), RT_RCDATA);
        HGLOBAL hGlob = LoadResource(hInstance, hResInfo);
        DWORD dwSize = SizeofResource(hInstance, hResInfo);

        const char* pszText = LockResource(hGlob);

        int len = MultiByteToWideChar(CP_UTF8, 0, pszText, dwSize, NULL, 0);
        g_pszEULAText = calloc(len, len * sizeof(WCHAR));

        MultiByteToWideChar(CP_UTF8, 0, pszText, dwSize, g_pszEULAText, len);

        HWND hRichEdit = GetDlgItem(hWnd, IDC_EULATEXT);

        SETTEXTEX ste;
        ste.codepage = CP_UTF8;
        ste.flags = ST_DEFAULT;
        SendMessage(hRichEdit, EM_SETTEXTEX, (WPARAM) &ste, (LPARAM) g_pszEULAText);

        DWORD dwStyle = GetWindowStyle(hRichEdit);
        dwStyle &= ES_READONLY;
        // SetWindowLongPtr(hRichEdit, GWL_STYLE, dwStyle);

        FreeResource(hResInfo);
      }
    break;
    case WM_COMMAND:
      {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
          EndDialog(hWnd, LOWORD(wParam));
          free(g_pszEULAText);
          return TRUE;
        }
      }
      break;
  }

  return FALSE;
}
