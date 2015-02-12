/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <windows.h>  // NOLINT
#include <shellapi.h>
#include <commdlg.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <set>
#include <vector>

#include <assert.h>

#include <blpwtk2.h>
#include <v8.h>

HINSTANCE g_instance = 0;
WNDPROC g_defaultEditWndProc = 0;
blpwtk2::Toolkit* g_toolkit = 0;
blpwtk2::Profile* g_profile = 0;
bool g_spellCheckEnabled;
int g_autoCorrectBehavior;
std::set<std::string> g_languages;
std::vector<std::string> g_sideLoadedFonts;
std::string g_url;
std::string g_dataDir;
std::string g_dictDir;
bool g_no_disk_cache = false;
bool g_no_disk_cookies = false;
bool g_in_process_renderer = false;
bool g_custom_hit_test = false;
bool g_custom_tooltip = false;
bool g_no_plugin_discovery = false;
HANDLE g_hJob;

#define OVERRIDE override
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24
#define AUTO_PUMP 1

// button ids
enum {
    IDC_START_OF_BUTTONS = 1000,
    IDC_BACK,
    IDC_FORWARD,
    IDC_RELOAD,
    IDC_STOP,
    IDC_FIND,
    IDC_FIND_PREV,
    IDC_FIND_NEXT,
    IDC_END_OF_BUTTONS,
    NUM_BUTTONS = IDC_END_OF_BUTTONS - IDC_START_OF_BUTTONS - 1
};

// menu ids
enum {
    IDM_START_OF_MENU_ITEMS = 2000,
    IDM_FILE,
    IDM_NEW_WINDOW,
    IDM_CLOSE_WINDOW,
    IDM_CLEAR_WEB_CACHE,
    IDM_PRINT,
    IDM_EXIT,
    IDM_ZOOM,
    IDM_ZOOM_025,
    IDM_ZOOM_050,
    IDM_ZOOM_075,
    IDM_ZOOM_100,
    IDM_ZOOM_125,
    IDM_ZOOM_150,
    IDM_ZOOM_200,
    IDM_TEST,
    IDM_TEST_V8_APPEND_ELEMENT,
    IDM_TEST_KEYBOARD_FOCUS,
    IDM_TEST_LOGICAL_FOCUS,
    IDM_TEST_LOGICAL_BLUR,
    IDM_TEST_PLAY_KEYBOARD_EVENTS,
    IDM_SPELLCHECK,
    IDM_SPELLCHECK_ENABLED,
    IDM_AUTOCORRECT,
    IDM_AUTOCORRECT_WORDMAP,
    IDM_AUTOCORRECT_SWAP_ADJACENT_CHARS,
    IDM_LANGUAGES,
    IDM_LANGUAGE_DE,
    IDM_LANGUAGE_EN_GB,
    IDM_LANGUAGE_EN_US,
    IDM_LANGUAGE_ES,
    IDM_LANGUAGE_FR,
    IDM_LANGUAGE_IT,
    IDM_LANGUAGE_PT_BR,
    IDM_LANGUAGE_PT_PT,
    IDM_LANGUAGE_RU,
    IDM_CUT,
    IDM_COPY,
    IDM_PASTE,
    IDM_DELETE,
    IDM_INSPECT,
    IDM_ADD_TO_DICTIONARY,
    IDM_CONTEXT_MENU_BASE_CUSTOM_TAG = 5000,
    IDM_CONTEXT_MENU_END_CUSTOM_TAG = 5999,
    IDM_CONTEXT_MENU_BASE_SPELL_TAG = 6000,
    IDM_CONTEXT_MENU_END_SPELL_TAG = 6999
};

static const char LANGUAGE_DE[] = "de-DE";
static const char LANGUAGE_EN_GB[] = "en-GB";
static const char LANGUAGE_EN_US[] = "en-US";
static const char LANGUAGE_ES[] = "es-ES";
static const char LANGUAGE_FR[] = "fr-FR";
static const char LANGUAGE_IT[] = "it-IT";
static const char LANGUAGE_PT_BR[] = "pt-BR";
static const char LANGUAGE_PT_PT[] = "pt-PT";
static const char LANGUAGE_RU[] = "ru-RU";

class Shell;
int registerShellWindowClass();
Shell* createShell(blpwtk2::Profile* profile, blpwtk2::WebView* webView = 0);
blpwtk2::ResourceLoader* createInProcessResourceLoader();
void populateSubmenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuItem& item);
void populateContextMenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuParams& params);
void updateSpellCheckConfig(blpwtk2::Profile* profile);
void toggleLanguage(blpwtk2::Profile* profile, const std::string& language);
const char* getHeaderFooterHTMLContent();

void testV8AppendElement(blpwtk2::WebView* webView)
{
    blpwtk2::WebFrame* mainFrame = webView->mainFrame();
    v8::Isolate* isolate = mainFrame->scriptIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = mainFrame->mainWorldScriptContext();
    static const char SCRIPT[] =
        "var div = document.createElement('div');\n"
        "div.textContent = 'Hello From Shell Using V8!!!';\n"
        "document.body.appendChild(div);\n";

    v8::Context::Scope contextScope(context);
    v8::ScriptCompiler::Source compilerSource(v8::String::NewFromUtf8(isolate, SCRIPT));
    v8::Local<v8::Script> script = v8::ScriptCompiler::Compile(isolate, &compilerSource);
    assert(!script.IsEmpty());  // this should never fail to compile

    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
        v8::String::Utf8Value msg(tryCatch.Exception());
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "EXCEPTION: %s\n", *msg);
        OutputDebugStringA(buf);
    }
}

void testPlayKeyboardEvents(HWND hwnd, blpwtk2::WebView* webView)
{
    blpwtk2::WebView::InputEvent ev = { 0 };
    ev.hwnd = hwnd;
    ev.message = WM_CHAR;
    ev.lparam = 0;
    ev.wparam = 'A';
    webView->handleInputEvents(&ev, 1);
    ev.wparam = 'B';
    webView->handleInputEvents(&ev, 1);
    ev.wparam = 'C';
    webView->handleInputEvents(&ev, 1);
}

class Shell : public blpwtk2::WebViewDelegate {
public:
    static std::set<Shell*> s_shells;

    HWND d_mainWnd;
    HWND d_urlEntryWnd;
    HWND d_findEntryHwnd;
    HMENU d_spellCheckMenu;
    blpwtk2::WebView* d_webView;
    blpwtk2::Profile* d_profile;
    Shell* d_inspectorShell;
    Shell* d_inspectorFor;
    POINT d_contextMenuPoint;
    std::string d_findText;
    std::vector<std::string> d_contextMenuSpellReplacements;
    std::string d_misspelledWord;

    Shell(HWND mainWnd,
          HWND urlEntryWnd,
          HWND findEntryHwnd,
          HMENU spellCheckMenu,
          blpwtk2::Profile* profile,
          blpwtk2::WebView* webView = 0)
    : d_mainWnd(mainWnd)
    , d_urlEntryWnd(urlEntryWnd)
    , d_findEntryHwnd(findEntryHwnd)
    , d_spellCheckMenu(spellCheckMenu)
    , d_webView(webView)
    , d_profile(profile)
    , d_inspectorShell(0)
    , d_inspectorFor(0)
    {
        s_shells.insert(this);

        if (!d_webView) {
            blpwtk2::WebViewCreateParams params;
            params.setProfile(d_profile);
            if (g_in_process_renderer && d_profile == g_profile) {
                params.setRendererAffinity(blpwtk2::Constants::IN_PROCESS_RENDERER);
            }
            d_webView = g_toolkit->createWebView(d_mainWnd, this, params);
        }
        else
            d_webView->setParent(d_mainWnd);

        d_webView->enableAltDragRubberbanding(true);
        d_webView->enableFocusBefore(true);
        d_webView->enableFocusAfter(true);
        d_webView->enableNCHitTest(g_custom_hit_test);
        d_webView->enableCustomTooltip(g_custom_tooltip);

        SetWindowLongPtr(d_mainWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        SetWindowLongPtr(d_urlEntryWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    ~Shell()
    {
        SetWindowLongPtr(d_mainWnd, GWLP_USERDATA, NULL);
        SetWindowLongPtr(d_urlEntryWnd, GWLP_USERDATA, NULL);

        if (d_inspectorFor) {
            d_inspectorFor->d_inspectorShell = 0;
            d_inspectorFor = 0;
        }

        if (d_inspectorShell) {
            DestroyWindow(d_inspectorShell->d_mainWnd);
            d_inspectorShell = 0;
        }

        d_webView->destroy();

        if (d_profile != g_profile) {
            // If the shell has its own profile, then the profile needs to be
            // destroyed.  g_profile gets destroyed before main() exits.
            d_profile->destroy();
        }

        s_shells.erase(this);
        if (0 == s_shells.size()) {
            PostQuitMessage(0);
        }
    }

    void resizeSubViews()
    {
        if (!d_webView) return;

        RECT rect;
        GetClientRect(d_mainWnd, &rect);
        assert(0 == rect.left);
        assert(0 == rect.top);
        d_webView->move(0, URLBAR_HEIGHT, rect.right, rect.bottom-URLBAR_HEIGHT);

        int x = NUM_BUTTONS * BUTTON_WIDTH;
        MoveWindow(d_urlEntryWnd, x, 0, rect.right-x, URLBAR_HEIGHT, TRUE);
    }


    ///////// WebViewDelegate overrides

    virtual void updateTargetURL(blpwtk2::WebView* source, const blpwtk2::StringRef& url) OVERRIDE
    {
        assert(source == d_webView);
        std::string str(url.data(), url.length());

        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: updateTargetUrl('%s')\n", str.c_str());
        OutputDebugStringA(buf);
    }

    // Notification that the navigation state of the specified 'source' has
    // been updated.  This notification can be used to update the state of UI
    // controls for back/forward actions.
    virtual void updateNavigationState(blpwtk2::WebView* source,
                                       const NavigationState& state) OVERRIDE
    {
        assert(source == d_webView);

        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: updateNavigationState(canGoBack(%d), canGoForward(%d), isLoading(%d))\n",
                  (int)state.canGoBack, (int)state.canGoForward, (int)state.isLoading);
        OutputDebugStringA(buf);

        EnableWindow(GetDlgItem(d_mainWnd, IDC_BACK), state.canGoBack);
        EnableWindow(GetDlgItem(d_mainWnd, IDC_FORWARD), state.canGoForward);
        EnableWindow(GetDlgItem(d_mainWnd, IDC_STOP), state.isLoading);
    }

    // Invoked when a main frame navigation occurs.
    virtual void didNavigateMainFramePostCommit(blpwtk2::WebView* source, const blpwtk2::StringRef& url) OVERRIDE
    {
        assert(source == d_webView);
        blpwtk2::String surl(url);

        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: didNavigateMainFramePostCommit('%s')\n", surl.c_str());
        OutputDebugStringA(buf);

        SendMessageA(d_urlEntryWnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(surl.c_str()));
        EnableWindow(GetDlgItem(d_mainWnd, IDC_RELOAD), TRUE);
    }

    // Invoked when the main frame finished loading the specified 'url'.  This
    // is the notification that guarantees that the 'mainFrame()' method on the
    // WebView can be used (for in-process WebViews, and in the renderer
    // thread).
    virtual void didFinishLoad(blpwtk2::WebView* source, const blpwtk2::StringRef& url) OVERRIDE
    {
        assert(source == d_webView);
        std::string str(url.data(), url.length());

        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: didFinishLoad('%s')\n", str.c_str());
        OutputDebugStringA(buf);
    }

    // Invoked when the main frame failed loading the specified 'url', or was
    // cancelled (e.g. window.stop() was called).
    virtual void didFailLoad(blpwtk2::WebView* source,
                             const blpwtk2::StringRef& url) OVERRIDE
    {
        assert(source == d_webView);
        std::string str(url.data(), url.length());

        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: didFailLoad('%s')\n", str.c_str());
        OutputDebugStringA(buf);
    }

    // Invoked when the WebView creates a new WebView, for example by using
    // 'window.open'.  The default implementation of this method is to simply
    // destroy the specified 'newView'.  The delegate for the 'newView' will
    // initially be null, it can be set by modifying the specified
    // 'newViewDelegate'.
    virtual void didCreateNewView(blpwtk2::WebView* source,
                                  blpwtk2::WebView* newView,
                                  const blpwtk2::NewViewParams& params,
                                  blpwtk2::WebViewDelegate** newViewDelegate) OVERRIDE
    {
        assert(source == d_webView);

        char buf[1024];
        blpwtk2::StringRef targetUrlRef = params.targetUrl();
        std::string targetUrl(targetUrlRef.data(), targetUrlRef.length());

        sprintf_s(buf, sizeof(buf), "DELEGATE: didCreateNewView('%s', numFeatures=%d)\n",
                  targetUrl.c_str(), params.additionalFeatureCount());
        OutputDebugStringA(buf);

        for (size_t i = 0; i < params.additionalFeatureCount(); ++i) {
            blpwtk2::StringRef featureStringRef = params.additionalFeatureAt(i);
            std::string featureString(featureStringRef.data(), featureStringRef.length());
            sprintf_s(buf, sizeof(buf), "DELEGATE:    additionalFeature(%s)\n",
                      featureString.c_str());
            OutputDebugStringA(buf);
        }

        Shell* newShell = createShell(d_profile, newView);
        *newViewDelegate = newShell;
        ShowWindow(newShell->d_mainWnd, SW_SHOW);
        UpdateWindow(newShell->d_mainWnd);
        SetFocus(newShell->d_urlEntryWnd);
    }

    // Invoked when the WebView needs to be destroyed, for example by using
    // 'window.close'.  The default implementation of this method is to simply
    // destroy the specified 'source'.
    virtual void destroyView(blpwtk2::WebView* source) OVERRIDE
    {
        assert(source == d_webView);
        OutputDebugStringA("DELEGATE: destroyView()\n");
        DestroyWindow(d_mainWnd);
    }

    // Invoked when WebKit is done tabbing backwards through controls in the page.  This
    // is only invoked if 'enableFocusBefore(true)' was called on the WebView.
    virtual void focusBefore(blpwtk2::WebView* source) OVERRIDE
    {
        assert(source == d_webView);
        OutputDebugStringA("DELEGATE: focusBefore()\n");
    }

    // Invoked when WebKit is done tabbing forwards through controls in the page.  This
    // is only invoked if 'enableFocusAfter(true)' was called on the WebView.
    virtual void focusAfter(blpwtk2::WebView* source) OVERRIDE
    {
        assert(source == d_webView);
        OutputDebugStringA("DELEGATE: focusAfter()\n");
    }

    // Notification that |source| has gained focus.
    virtual void focused(blpwtk2::WebView* source) OVERRIDE
    {
        assert(source == d_webView);
        OutputDebugStringA("DELEGATE: focused\n");
    }

    // Notification that |source| has lost focus.
    virtual void blurred(blpwtk2::WebView* source) OVERRIDE
    {
        assert(source == d_webView);
        OutputDebugStringA("DELEGATE: blurred\n");
    }

    std::string extentionForMimeType(const blpwtk2::StringRef& mimeType)
    {
        const char* end = mimeType.data() + mimeType.length();
        const char* p = end - 1;
        while (p > mimeType.data()) {
            if ('/' == *p) {
                return std::string(p + 1, end);
            }
            --p;
        }
        return "*";
    }

    void appendStringToVector(std::vector<char> *result, const blpwtk2::StringRef& str)
    {
        result->reserve(result->size() + str.length());
        const char* p = str.data();
        const char* end = p + str.length();
        while (p < end) {
            result->push_back(*p);
            ++p;
        }
    }

    virtual void runFileChooser(blpwtk2::WebView* source, const blpwtk2::FileChooserParams& params) OVERRIDE
    {
        assert(source == d_webView);

        std::vector<char> filter;
        if (0 != params.numAcceptTypes()) {
            if (1 == params.numAcceptTypes()) {
                blpwtk2::StringRef mimeType = params.acceptTypeAt(0);
                appendStringToVector(&filter, mimeType);
                filter.push_back('\0');
                appendStringToVector(&filter, "*.");
                appendStringToVector(&filter, extentionForMimeType(mimeType));
            }
            else {
                appendStringToVector(&filter, "Custom Types");
                filter.push_back('\0');
                for (size_t i = 0; i < params.numAcceptTypes(); ++i) {
                    if (0 != i)
                        filter.push_back(';');
                    appendStringToVector(&filter, "*.");
                    blpwtk2::StringRef mimeType = params.acceptTypeAt(i);
                    appendStringToVector(&filter, extentionForMimeType(mimeType));
                }
                for (size_t i = 0; i < params.numAcceptTypes(); ++i) {
                    filter.push_back('\0');
                    blpwtk2::StringRef mimeType = params.acceptTypeAt(i);
                    appendStringToVector(&filter, mimeType);
                    filter.push_back('\0');
                    appendStringToVector(&filter, "*.");
                    appendStringToVector(&filter, extentionForMimeType(mimeType));
                }
            }
            filter.push_back('\0');
        }

        appendStringToVector(&filter, "All Files");
        filter.push_back('\0');
        appendStringToVector(&filter, "*.*");
        filter.push_back('\0');
        filter.push_back('\0');

        std::string title = params.title().toStdString();
        if (title.empty())
            title = "Select File";
        char fileNameBuf[8192] = { 0 };
        strcpy_s(fileNameBuf, sizeof(fileNameBuf) - 1, params.defaultFileName().toStdString().c_str());

        OPENFILENAMEA ofn = { 0 };
        ofn.lStructSize = sizeof(OPENFILENAMEA);
        ofn.hwndOwner = d_mainWnd;
        ofn.hInstance = g_instance;
        ofn.lpstrFilter = filter.data();
        ofn.lpstrFile = fileNameBuf;
        ofn.nMaxFile = sizeof(fileNameBuf) - 2;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NOCHANGEDIR;

        BOOL retVal;
        switch (params.mode()) {
        case blpwtk2::FileChooserParams::OPEN:
            ofn.Flags |= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            retVal = ::GetOpenFileNameA(&ofn);
            break;
        case blpwtk2::FileChooserParams::OPEN_MULTIPLE:
            ofn.Flags |= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT;
            retVal = ::GetOpenFileNameA(&ofn);
            break;
        case blpwtk2::FileChooserParams::SAVE:
            ofn.Flags = OFN_OVERWRITEPROMPT;
            retVal = ::GetSaveFileNameA(&ofn);
            break;
        case blpwtk2::FileChooserParams::UPLOAD_FOLDER:
            // TODO: what should we do here?
            retVal = FALSE;
            break;
        }

        std::vector<std::string> selectedFiles;
        std::vector<blpwtk2::StringRef> selectedFilesRef;

        if (retVal) {
            // Figure out if the user selected multiple files.  If fileNameBuf is
            // a directory, then multiple files were selected!
            if ((ofn.Flags & OFN_ALLOWMULTISELECT) && (::GetFileAttributesA(fileNameBuf) & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string dirName = fileNameBuf;
                const char* p = fileNameBuf + strlen(fileNameBuf) + 1;
                while (*p) {
                    selectedFiles.push_back(dirName);
                    selectedFiles.back().append("\\");
                    selectedFiles.back().append(p);
                    p += strlen(p) + 1;
                }
                selectedFilesRef.resize(selectedFiles.size());
                for (size_t i = 0; i < selectedFiles.size(); ++i) {
                    selectedFilesRef[i] = selectedFiles[i];
                }
            }
            else {
                selectedFilesRef.push_back(fileNameBuf);
            }
        }

        d_webView->fileChooserCompleted(selectedFilesRef.data(), selectedFilesRef.size());
    }

    virtual void showContextMenu(blpwtk2::WebView* source, const blpwtk2::ContextMenuParams& params) OVERRIDE
    {
        assert(source == d_webView);
        OutputDebugStringA("DELEGATE: showContextMenu\n");

        d_contextMenuPoint = params.pointOnScreen();
        ::ScreenToClient(d_mainWnd, &d_contextMenuPoint);
        d_contextMenuPoint.y -= URLBAR_HEIGHT;

        HMENU menu = createContextMenu(params);
        TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                       params.pointOnScreen().x, params.pointOnScreen().y,
                       0, d_mainWnd, NULL);
        DestroyMenu(menu);
    }

    virtual void handleExternalProtocol(blpwtk2::WebView* source, const blpwtk2::StringRef& url) OVERRIDE
    {
        assert(source == d_webView);
        std::string target(url.data(), url.length());

        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: handleExternalProtocol('%s')\n", target.c_str());
        OutputDebugStringA(buf);

        ShellExecuteA(NULL, NULL, target.c_str(), NULL, NULL, SW_SHOWNORMAL);        
    }

    virtual void requestNCHitTest(blpwtk2::WebView* source)
    {
        assert(source == d_webView);
        POINT pt;
        ::GetCursorPos(&pt);
        POINT ptClient = pt;
        ::ScreenToClient(d_mainWnd, &ptClient);
        RECT rcClient;
        ::GetClientRect(d_mainWnd, &rcClient);

        bool nearLeftBorder = false, nearTopBorder = false, nearRightBorder = false, nearBottomBorder = false;
        if (ptClient.x >= 0 && ptClient.x <= 50)
            nearLeftBorder = true;
        else if (ptClient.x >= rcClient.right - 50 && ptClient.x <= rcClient.right)
            nearRightBorder = true;
        if (ptClient.y >= URLBAR_HEIGHT && ptClient.y <= URLBAR_HEIGHT + 50)
            nearTopBorder = true;
        else if (ptClient.y >= rcClient.bottom - 50 && ptClient.y <= rcClient.bottom)
            nearBottomBorder = true;

        int result = HTCLIENT;
        if (nearLeftBorder) {
            if (nearTopBorder)
                result = HTTOPLEFT;
            else if (nearBottomBorder)
                result = HTBOTTOMLEFT;
            else
                result = HTLEFT;
        }
        else if (nearRightBorder) {
            if (nearTopBorder)
                result = HTTOPRIGHT;
            else if (nearBottomBorder)
                result = HTBOTTOMRIGHT;
            else
                result = HTRIGHT;
        }
        else if (nearTopBorder)
            result = HTTOP;
        else if (nearBottomBorder)
            result = HTBOTTOM;

        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: requestNCHitTest(x=%d, y=%d, result=%d)\n",
                  ptClient.x, ptClient.y, result);
        OutputDebugStringA(buf);
        d_webView->onNCHitTestResult(pt.x, pt.y, result);
    }

    virtual void ncDragBegin(blpwtk2::WebView* source,
                             int hitTestCode,
                             const POINT& startPoint)
    {
        assert(source == d_webView);
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: ncDragBegin(%d, x=%d, y=%d)\n",
                  hitTestCode, startPoint.x, startPoint.y);
        OutputDebugStringA(buf);
    }

    virtual void ncDragMove(blpwtk2::WebView* source, const POINT& movePoint)
    {
        assert(source == d_webView);
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: ncDragMove(x=%d, y=%d)\n",
                  movePoint.x, movePoint.y);
        OutputDebugStringA(buf);
    }

    virtual void ncDragEnd(blpwtk2::WebView* source, const POINT& endPoint)
    {
        assert(source == d_webView);
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "DELEGATE: ncDragEnd(x=%d, y=%d)\n",
                  endPoint.x, endPoint.y);
        OutputDebugStringA(buf);
    }

    virtual void showTooltip(blpwtk2::WebView* source,
                             const blpwtk2::StringRef& tooltipText,
                             blpwtk2::TextDirection::Value direction)
    {
        assert(source == d_webView);
        if (!tooltipText.isEmpty()) {
            std::string stext(tooltipText.data(), tooltipText.length());
            char buf[1024];
            sprintf_s(buf, sizeof(buf), "DELEGATE: showTooltip '%s'\n", stext.c_str());
            OutputDebugStringA(buf);
        }
    }

    void find()
    {
        char buf[200];
        int len = ::GetWindowTextA(d_findEntryHwnd, buf, sizeof buf);

        d_findText.assign(buf, len);
        d_webView->find(d_findText, false, true);
    }

    void findNext(bool forward)
    {
        d_webView->find(d_findText, false, forward);
    }

    virtual void findState(blpwtk2::WebView* source, int numberOfMatches, int activeMatchOrdinal, bool finalUpdate) OVERRIDE
    {
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "FIND: count:%d, current:%d, final:%s\n",
                  numberOfMatches, activeMatchOrdinal, finalUpdate ? "yes" : "no");
        OutputDebugStringA(buf);
    }

    HMENU createContextMenu(const blpwtk2::ContextMenuParams& params)
    {
        bool addSeparator = false;
        if (params.canCut() || params.canCopy() || params.canPaste() || params.canDelete())
            addSeparator = true;

        HMENU menu = CreatePopupMenu();

        if (params.numCustomItems() > 0) {
            populateContextMenu(menu, IDM_CONTEXT_MENU_BASE_CUSTOM_TAG, params);
            AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
        }

        if (params.canCut())
            AppendMenu(menu, MF_STRING, IDM_CUT, L"C&ut");
        if (params.canCopy())
            AppendMenu(menu, MF_STRING, IDM_COPY, L"&Copy");
        if (params.canPaste())
            AppendMenu(menu, MF_STRING, IDM_PASTE, L"&Paste");
        if (params.canDelete())
            AppendMenu(menu, MF_STRING, IDM_DELETE, L"&Delete");

        if (addSeparator)
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);

        AppendMenu(menu, MF_STRING, IDM_INSPECT, L"I&nspect Element");

        if (!params.misspelledWord().isEmpty()) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);

            d_misspelledWord.assign(params.misspelledWord().data(),
                                    params.misspelledWord().length());

            std::string menuText = "Add to dictionary: ";
            menuText.append(d_misspelledWord.c_str());
            AppendMenuA(menu, MF_STRING, IDM_ADD_TO_DICTIONARY, menuText.c_str());
        }

        d_contextMenuSpellReplacements.clear();

        if (params.numSpellSuggestions() > 0) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);

            for (int i = 0; i < params.numSpellSuggestions(); ++i) {
                int index = d_contextMenuSpellReplacements.size();
                std::string text(params.spellSuggestion(i).data(),
                                 params.spellSuggestion(i).length());
                d_contextMenuSpellReplacements.push_back(text);
                AppendMenuA(menu, MF_STRING, IDM_CONTEXT_MENU_BASE_SPELL_TAG + index, text.c_str());
            }
        }

        return menu;
    }
};
std::set<Shell*> Shell::s_shells;

void runMessageLoop()
{
    MSG msg;
#if AUTO_PUMP
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#else
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!g_toolkit->preHandleMessage(&msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        g_toolkit->postHandleMessage(&msg);
    }
#endif
}

struct HostWatcherThreadData {
    std::vector<HANDLE> d_processHandles;
    int d_mainThreadId;
};

DWORD hostWatcherThreadFunc(LPVOID lParam)
{
    HostWatcherThreadData* data = (HostWatcherThreadData*)lParam;
    ::WaitForMultipleObjects(data->d_processHandles.size(),
                             data->d_processHandles.data(),
                             TRUE,
                             INFINITE);
    ::PostThreadMessage(data->d_mainThreadId, WM_QUIT, 0, 0);
    return 0;
}

HANDLE spawnProcess()
{
    blpwtk2::String channelId = g_toolkit->createHostChannel(10000);

    char fileName[MAX_PATH + 1];
    ::GetModuleFileNameA(NULL, fileName, MAX_PATH);

    std::string cmdline;
    cmdline.append("\"");
    cmdline.append(fileName);
    cmdline.append("\" " + g_url);
    cmdline.append(" --host-channel=");
    cmdline.append(channelId.c_str());
    if (g_in_process_renderer) {
        cmdline.append(" --in-process-renderer");
    }
    if (!g_dataDir.empty()) {
        cmdline.append(" --data-dir=");
        cmdline.append(g_dataDir);
    }
    if (!g_dictDir.empty()) {
        cmdline.append(" --dict-dir=");
        cmdline.append(g_dictDir);
    }
    if (g_no_disk_cache) {
        cmdline.append(" --no-disk-cache");
    }
    if (g_no_disk_cookies) {
        cmdline.append(" --no-disk-cookies");
    }
    if (g_custom_hit_test) {
        cmdline.append(" --custom-hit-test");
    }
    if (g_custom_tooltip) {
        cmdline.append(" --custom-tooltip");
    }
    if (g_no_plugin_discovery) {
        cmdline.append(" --no-plugin-discovery");
    }
    for (size_t i = 0; i < g_sideLoadedFonts.size(); ++i) {
        cmdline.append(" --sideload-font=");
        cmdline.append(g_sideLoadedFonts[i]);
    }

    // It seems like CreateProcess wants a char* instead of
    // a const char*.  So we need to make a copy to a modifiable
    // buffer.
    char cmdlineCopy[1024];
    strcpy_s(cmdlineCopy, sizeof(cmdlineCopy), cmdline.c_str());

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);

    PROCESS_INFORMATION procInfo;
    BOOL success = ::CreateProcessA(
        NULL,
        cmdlineCopy,
        NULL,
        NULL,
        FALSE,
        CREATE_BREAKAWAY_FROM_JOB,
        NULL,
        NULL,
        &si,
        &procInfo);

    if (!success) {
        DWORD lastError = ::GetLastError();
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "CreateProcess failed: %d\n", lastError);
        OutputDebugStringA(buf);
        return NULL;
    }

    if (!::AssignProcessToJobObject(g_hJob, procInfo.hProcess)) {
        DWORD lastError = ::GetLastError();
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "AssignProcessToJobObject failed: %d\n", lastError);
        OutputDebugStringA(buf);
        ::TerminateProcess(procInfo.hProcess, -1);
        return NULL;
    }

    ::CloseHandle(procInfo.hThread);
    return procInfo.hProcess;
}

void runHost()
{
    g_hJob = ::CreateJobObject(NULL, NULL);
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = { 0 };
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!::SetInformationJobObject(g_hJob,
                                       JobObjectExtendedLimitInformation,
                                       &info,
                                       sizeof(info))) {
            DWORD lastError = ::GetLastError();
            char buf[1024];
            sprintf_s(buf, sizeof(buf), "SetInformationJobObject failed: %d\n", lastError);
            OutputDebugStringA(buf);
            return;
        }
    }

    HostWatcherThreadData threadData;
    threadData.d_mainThreadId = ::GetCurrentThreadId();
    for (size_t i = 0; i < 3; ++i) {
        HANDLE processHandle = spawnProcess();
        if (!processHandle) {
            return;
        }
        threadData.d_processHandles.push_back(processHandle);
    }

    HANDLE watcherThread = ::CreateThread(
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)hostWatcherThreadFunc,
        &threadData,
        0,
        NULL);

    runMessageLoop();
    ::WaitForSingleObject(watcherThread, INFINITE);
    ::CloseHandle(watcherThread);
    for (size_t i = 0; i < threadData.d_processHandles.size(); ++i) {
        ::CloseHandle(threadData.d_processHandles[i]);
    }
    ::CloseHandle(g_hJob);
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int)
{
    g_instance = instance;

    g_url = "http://www.google.com";
    std::string hostChannel = "";
    bool isHost = false;

    {
        int argc;
        LPWSTR *argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        if (!argv) {
            return -1;
        }

        for (int i = 1; i < argc; ++i) {
            if (0 == wcscmp(L"--host", argv[i])) {
                isHost = true;
            }
            else if (0 == wcsncmp(L"--host-channel=", argv[i], 15)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i]+15);
                hostChannel = buf;
            }
            else if (0 == wcscmp(L"--in-process-renderer", argv[i])) {
                g_in_process_renderer = true;
            }
            else if (0 == wcsncmp(L"--data-dir=", argv[i], 11)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i]+11);
                g_dataDir = buf;
            }
            else if (0 == wcsncmp(L"--sideload-font=", argv[i], 16)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i] + 16);
                g_sideLoadedFonts.push_back(buf);
            }
            else if (0 == wcsncmp(L"--dict-dir=", argv[i], 11)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i] + 11);
                g_dictDir = buf;
            }
            else if (0 == wcscmp(L"--no-disk-cache", argv[i])) {
                g_no_disk_cache = true;
            }
            else if (0 == wcscmp(L"--no-disk-cookies", argv[i])) {
                g_no_disk_cookies = true;
            }
            else if (0 == wcscmp(L"--custom-hit-test", argv[i])) {
                g_custom_hit_test = true;
            }
            else if (0 == wcscmp(L"--custom-tooltip", argv[i])) {
                g_custom_tooltip = true;
            }
            else if (0 == wcscmp(L"--no-plugin-discovery", argv[i])) {
                g_no_plugin_discovery = true;
            }
            else if (argv[i][0] != '-') {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i]);
                g_url = buf;
            }
        }

        ::LocalFree(argv);
    }

    {
        char buf[1024];
        sprintf_s(buf, sizeof(buf), "URL(%s) isHost(%d) hostChannel(%s)\n",
                  g_url.c_str(),
                  isHost ? 1 : 0,
                  hostChannel.c_str());
        OutputDebugStringA(buf);
    }

    blpwtk2::ToolkitCreateParams toolkitParams;
    if (isHost || (!g_in_process_renderer && hostChannel.empty())) {
        toolkitParams.setThreadMode(blpwtk2::ThreadMode::ORIGINAL);
        toolkitParams.disableInProcessRenderer();
    }
    else {
        toolkitParams.setThreadMode(blpwtk2::ThreadMode::RENDERER_MAIN);
        toolkitParams.setInProcessResourceLoader(createInProcessResourceLoader());
        toolkitParams.setHostChannel(hostChannel);
    }
#if AUTO_PUMP
    toolkitParams.setPumpMode(blpwtk2::PumpMode::AUTOMATIC);
#endif

    if (g_no_plugin_discovery) {
        toolkitParams.disablePluginDiscovery();
    }

    for (size_t i = 0; i < g_sideLoadedFonts.size(); ++i) {
        toolkitParams.appendSideLoadedFontInProcess(g_sideLoadedFonts[i]);
    }

    toolkitParams.setHeaderFooterHTML(getHeaderFooterHTMLContent());
    toolkitParams.enablePrintBackgroundGraphics();
    toolkitParams.setDictionaryPath(g_dictDir);
    g_toolkit = blpwtk2::ToolkitFactory::create(toolkitParams);

    if (isHost) {
        runHost();
        g_toolkit->destroy();
        g_toolkit = 0;
        return 0;
    }

    int rc = registerShellWindowClass();
    if (rc) return rc;

    blpwtk2::ProfileCreateParams profileParams(g_dataDir);
    if (g_no_disk_cache)
        profileParams.setDiskCacheEnabled(false);
    if (g_no_disk_cookies)
        profileParams.setCookiePersistenceEnabled(false);
    g_profile = g_toolkit->createProfile(profileParams);

    g_spellCheckEnabled = true;
    g_autoCorrectBehavior =
        blpwtk2::SpellCheckConfig::AUTOCORRECT_WORD_MAP |
        blpwtk2::SpellCheckConfig::AUTOCORRECT_SWAP_ADJACENT_CHARS;
    g_languages.insert(LANGUAGE_EN_US);
    updateSpellCheckConfig(g_profile);

    // Configure custom words.
    std::vector<blpwtk2::StringRef> customWords;
    customWords.push_back("foo");
    customWords.push_back("zzzx");
    customWords.push_back("Bloomberg");
    g_profile->addCustomWords(customWords.data(), customWords.size());

    // Configure autocorrect words.
    std::vector<blpwtk2::StringRef> badWords, goodWords;
    badWords.push_back("speling");           goodWords.push_back("spelling");
    badWords.push_back("ambigous");          goodWords.push_back("ambiguous");
    badWords.push_back("restraunt");         goodWords.push_back("restaurant");
    badWords.push_back("comitee");           goodWords.push_back("committee");
    badWords.push_back("misunderestimate");  goodWords.push_back("underestimate");
    badWords.push_back("speeking");          goodWords.push_back("speaking");
    assert(badWords.size() == goodWords.size());
    g_profile->addAutocorrectWords(badWords.data(), goodWords.data(), badWords.size());

    Shell* firstShell = createShell(g_profile);
    firstShell->d_webView->loadUrl(g_url);
    ShowWindow(firstShell->d_mainWnd, SW_SHOW);
    UpdateWindow(firstShell->d_mainWnd);
    firstShell->d_webView->takeKeyboardFocus();
    firstShell->d_webView->setLogicalFocus(true);

    runMessageLoop();

    g_profile->destroy();
    g_toolkit->destroy();
    g_toolkit = 0;
    return 0;
}

void adjustMenuItemStateFlag(HMENU menu, int menuItem, int state, bool on)
{
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;
    GetMenuItemInfo(menu, menuItem, TRUE, &mii);
    if (on) {
        mii.fState |= state;
    }
    else {
        mii.fState &= ~state;
    }
    SetMenuItemInfo(menu, menuItem, TRUE, &mii);
}

LRESULT CALLBACK shellWndProc(HWND hwnd,        // handle to window
                              UINT uMsg,        // message identifier
                              WPARAM wParam,    // first message parameter
                              LPARAM lParam)    // second message parameter
{
    Shell* shell = reinterpret_cast<Shell*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!shell) return DefWindowProc(hwnd, uMsg, wParam, lParam);

    int wmId;
    Shell* newShell;
    switch(uMsg) {
    case WM_COMMAND:
        wmId = LOWORD(wParam);
        if (wmId >= IDM_CONTEXT_MENU_BASE_CUSTOM_TAG && wmId < IDM_CONTEXT_MENU_END_CUSTOM_TAG) {
            shell->d_webView->performCustomContextMenuAction(wmId - IDM_CONTEXT_MENU_BASE_CUSTOM_TAG);
            return 0;
        }
        if (wmId >= IDM_CONTEXT_MENU_BASE_SPELL_TAG && wmId < IDM_CONTEXT_MENU_END_SPELL_TAG) {
            int idx = (wmId - IDM_CONTEXT_MENU_BASE_SPELL_TAG);
            shell->d_webView->replaceMisspelledRange(shell->d_contextMenuSpellReplacements[idx]);
            return 0;
        }
        switch (wmId) {
        case IDC_BACK:
            shell->d_webView->goBack();
            shell->d_webView->takeKeyboardFocus();
            shell->d_webView->setLogicalFocus(true);
            return 0;
        case IDC_FORWARD:
            shell->d_webView->goForward();
            shell->d_webView->takeKeyboardFocus();
            shell->d_webView->setLogicalFocus(true);
            return 0;
        case IDC_RELOAD:
            shell->d_webView->reload();
            shell->d_webView->takeKeyboardFocus();
            shell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_ZOOM_025:
        case IDM_ZOOM_050:
        case IDM_ZOOM_075:
        case IDM_ZOOM_100:
        case IDM_ZOOM_125:
        case IDM_ZOOM_150:
        case IDM_ZOOM_200: {
                static const int zoom_values[] = {25, 50, 75, 100, 125, 150, 200};
                shell->d_webView->setZoomPercent(zoom_values[wmId - IDM_ZOOM_025]);
            }
            return 0;
        case IDC_FIND:
            if (HIWORD(wParam) == EN_CHANGE) {
                shell->find();
            }
            return 0;
        case IDC_FIND_PREV:
        case IDC_FIND_NEXT:
            shell->findNext(wmId == IDC_FIND_NEXT);
            return 0;
        case IDC_STOP:
            shell->d_webView->stop();
            shell->d_webView->takeKeyboardFocus();
            shell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_NEW_WINDOW:
            newShell = createShell(shell->d_profile);
            newShell->d_webView->loadUrl(g_url);
            ShowWindow(newShell->d_mainWnd, SW_SHOW);
            UpdateWindow(newShell->d_mainWnd);
            newShell->d_webView->takeKeyboardFocus();
            newShell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_CLOSE_WINDOW:
            DestroyWindow(shell->d_mainWnd);
            return 0;
        case IDM_CLEAR_WEB_CACHE:
            g_toolkit->clearWebCache();
            return 0;
        case IDM_PRINT:
            shell->d_webView->print();
            return 0;
        case IDM_TEST_V8_APPEND_ELEMENT:
            testV8AppendElement(shell->d_webView);
            return 0;
        case IDM_TEST_KEYBOARD_FOCUS:
            shell->d_webView->takeKeyboardFocus();
            return 0;
        case IDM_TEST_LOGICAL_FOCUS:
            shell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_TEST_LOGICAL_BLUR:
            shell->d_webView->setLogicalFocus(false);
            return 0;
        case IDM_TEST_PLAY_KEYBOARD_EVENTS:
            testPlayKeyboardEvents(shell->d_mainWnd, shell->d_webView);
            return 0;
        case IDM_SPELLCHECK_ENABLED:
            g_spellCheckEnabled = !g_spellCheckEnabled;
            updateSpellCheckConfig(shell->d_profile);
            return 0;
        case IDM_AUTOCORRECT_WORDMAP:
            g_autoCorrectBehavior ^= blpwtk2::SpellCheckConfig::AUTOCORRECT_WORD_MAP;
            updateSpellCheckConfig(shell->d_profile);
            return 0;
        case IDM_AUTOCORRECT_SWAP_ADJACENT_CHARS:
            g_autoCorrectBehavior ^= blpwtk2::SpellCheckConfig::AUTOCORRECT_SWAP_ADJACENT_CHARS;
            updateSpellCheckConfig(shell->d_profile);
            return 0;
        case IDM_LANGUAGE_DE:
            toggleLanguage(shell->d_profile, LANGUAGE_DE);
            return 0;
        case IDM_LANGUAGE_EN_GB:
            toggleLanguage(shell->d_profile, LANGUAGE_EN_GB);
            return 0;
        case IDM_LANGUAGE_EN_US:
            toggleLanguage(shell->d_profile, LANGUAGE_EN_US);
            return 0;
        case IDM_LANGUAGE_ES:
            toggleLanguage(shell->d_profile, LANGUAGE_ES);
            return 0;
        case IDM_LANGUAGE_FR:
            toggleLanguage(shell->d_profile, LANGUAGE_FR);
            return 0;
        case IDM_LANGUAGE_IT:
            toggleLanguage(shell->d_profile, LANGUAGE_IT);
            return 0;
        case IDM_LANGUAGE_PT_BR:
            toggleLanguage(shell->d_profile, LANGUAGE_PT_BR);
            return 0;
        case IDM_LANGUAGE_PT_PT:
            toggleLanguage(shell->d_profile, LANGUAGE_PT_PT);
            return 0;
        case IDM_LANGUAGE_RU:
            toggleLanguage(shell->d_profile, LANGUAGE_RU);
            return 0;
        case IDM_CUT:
            shell->d_webView->cutSelection();
            return 0;
        case IDM_COPY:
            shell->d_webView->copySelection();
            return 0;
        case IDM_PASTE:
            shell->d_webView->paste();
            return 0;
        case IDM_DELETE:
            shell->d_webView->deleteSelection();
            return 0;
        case IDM_INSPECT:
            if (shell->d_inspectorShell) {
                BringWindowToTop(shell->d_inspectorShell->d_mainWnd);
                shell->d_inspectorShell->d_webView->inspectElementAt(shell->d_contextMenuPoint);
                shell->d_inspectorShell->d_webView->takeKeyboardFocus();
                shell->d_inspectorShell->d_webView->setLogicalFocus(true);
                return 0;
            }
            {
                blpwtk2::Profile* profile = g_profile;
                if (!g_dataDir.empty()) {
                    blpwtk2::ProfileCreateParams profileParams("");
                    profile = g_toolkit->createProfile(profileParams);
                }
                shell->d_inspectorShell = createShell(profile);
            }
            shell->d_inspectorShell->d_inspectorFor = shell;
            ShowWindow(shell->d_inspectorShell->d_mainWnd, SW_SHOW);
            UpdateWindow(shell->d_inspectorShell->d_mainWnd);
            shell->d_inspectorShell->d_webView->loadInspector(shell->d_webView);
            shell->d_inspectorShell->d_webView->inspectElementAt(shell->d_contextMenuPoint);
            shell->d_inspectorShell->d_webView->takeKeyboardFocus();
            shell->d_inspectorShell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_ADD_TO_DICTIONARY:
            {
                blpwtk2::StringRef word = shell->d_misspelledWord;
                shell->d_profile->addCustomWords(&word, 1);
            }
            return 0;
        case IDM_EXIT:
            std::vector<Shell*> shells(Shell::s_shells.begin(), Shell::s_shells.end());
            for (int i = 0, size = shells.size(); i < size; ++i)
                DestroyWindow(shells[i]->d_mainWnd);
            return 0;
        }
        break;
    case WM_INITMENUPOPUP: {
            HMENU menu = (HMENU)wParam;

            bool autocorrectWordMap
                = g_autoCorrectBehavior & blpwtk2::SpellCheckConfig::AUTOCORRECT_WORD_MAP;
            bool autocorrectSwapChars
                = g_autoCorrectBehavior & blpwtk2::SpellCheckConfig::AUTOCORRECT_SWAP_ADJACENT_CHARS;

            adjustMenuItemStateFlag(shell->d_spellCheckMenu, 1, MFS_DISABLED, !g_spellCheckEnabled);
            adjustMenuItemStateFlag(shell->d_spellCheckMenu, 2, MFS_DISABLED, !g_spellCheckEnabled);
            CheckMenuItem(menu, IDM_SPELLCHECK_ENABLED, g_spellCheckEnabled ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_AUTOCORRECT_WORDMAP, autocorrectWordMap ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_AUTOCORRECT_SWAP_ADJACENT_CHARS, autocorrectSwapChars ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_DE, g_languages.find(LANGUAGE_DE) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_EN_GB, g_languages.find(LANGUAGE_EN_GB) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_EN_US, g_languages.find(LANGUAGE_EN_US) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_ES, g_languages.find(LANGUAGE_ES) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_FR, g_languages.find(LANGUAGE_FR) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_IT, g_languages.find(LANGUAGE_IT) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_PT_BR, g_languages.find(LANGUAGE_PT_BR) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_PT_PT, g_languages.find(LANGUAGE_PT_PT) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_RU, g_languages.find(LANGUAGE_RU) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
        }
        break;
    case WM_WINDOWPOSCHANGED:
        shell->d_webView->rootWindowPositionChanged();
        break;
    case WM_SETTINGCHANGE:
        shell->d_webView->rootWindowSettingsChanged();
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        delete shell;
        return 0;
    case WM_SIZE:
        shell->resizeSubViews();
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK urlEntryWndProc(HWND hwnd,        // handle to window
                                 UINT uMsg,        // message identifier
                                 WPARAM wParam,    // first message parameter
                                 LPARAM lParam)    // second message parameter
{
    Shell* shell = reinterpret_cast<Shell*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!shell) return CallWindowProc(g_defaultEditWndProc, hwnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_CHAR:
        if (wParam == VK_RETURN) {
            const int MAX_URL_LENGTH = 255;
            char str[MAX_URL_LENGTH + 1];  // Leave room for adding a NULL;
            *((WORD*)str) = MAX_URL_LENGTH;
            LRESULT str_len = SendMessageA(hwnd, EM_GETLINE, 0, (LPARAM)str);
            if (str_len > 0) {
                str[str_len] = 0;  // EM_GETLINE doesn't NULL terminate.
                shell->d_webView->loadUrl(str);
                shell->d_webView->takeKeyboardFocus();
                shell->d_webView->setLogicalFocus(true);
            }
            return 0;
        }
    }

    return CallWindowProc(g_defaultEditWndProc, hwnd, uMsg, wParam, lParam);
}



int registerShellWindowClass()
{
    WNDCLASSEX wcx;

    // Fill in the window class structure with parameters
    // that describe the main window.

    wcx.cbSize = sizeof(wcx);               // size of structure
    wcx.style = CS_HREDRAW | CS_VREDRAW;    // redraw if size changes
    wcx.lpfnWndProc = shellWndProc;         // points to window procedure
    wcx.cbClsExtra = 0;                     // no extra class memory
    wcx.cbWndExtra = 0;                     // no extra window memory
    wcx.hInstance = g_instance;             // handle to instance
    wcx.hIcon = LoadIcon(NULL, IDI_APPLICATION);    // predefined app. icon
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);      // predefined arrow
    wcx.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);    // white background brush
    wcx.lpszMenuName =  NULL;               // name of menu resource
    wcx.lpszClassName = L"ShellWClass";     // name of window class
    wcx.hIconSm = (HICON)LoadImage(g_instance,  // small class icon
                                   MAKEINTRESOURCE(5),
                                   IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON),
                                   LR_DEFAULTCOLOR);

    // Register the window class.  RegisterClassEx returns 0 for failure!!
    return RegisterClassEx(&wcx) == 0 ? -1 : 0;
}

Shell* createShell(blpwtk2::Profile* profile, blpwtk2::WebView* webView)
{
    // Create the main window.
    HWND mainWnd = CreateWindow(L"ShellWClass",      // name of window class
                                L"Sample",           // title-bar string
                                WS_OVERLAPPEDWINDOW, // top-level window
                                CW_USEDEFAULT,       // default horizontal position
                                CW_USEDEFAULT,       // default vertical position
                                CW_USEDEFAULT,       // default width
                                CW_USEDEFAULT,       // default height
                                (HWND) NULL,         // no owner window
                                (HMENU) NULL,        // use class menu
                                g_instance,          // handle to application instance
                                (LPVOID) NULL);      // no window-creation data
    assert(mainWnd);

    HMENU menu = CreateMenu();
    HMENU fileMenu = CreateMenu();
    AppendMenu(fileMenu, MF_STRING, IDM_NEW_WINDOW, L"&New Window");
    AppendMenu(fileMenu, MF_STRING, IDM_CLOSE_WINDOW, L"&Close Window");
    AppendMenu(fileMenu, MF_STRING, IDM_CLEAR_WEB_CACHE, L"Clear Web Cache");
    AppendMenu(fileMenu, MF_STRING, IDM_PRINT, L"P&rint");
    AppendMenu(fileMenu, MF_SEPARATOR, 0, 0);
    AppendMenu(fileMenu, MF_STRING, IDM_EXIT, L"E&xit");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)fileMenu, L"&File");
    HMENU zoomMenu = CreateMenu();
    AppendMenu(zoomMenu, MF_STRING, IDM_ZOOM_025, L"25%");
    AppendMenu(zoomMenu, MF_STRING, IDM_ZOOM_050, L"50%");
    AppendMenu(zoomMenu, MF_STRING, IDM_ZOOM_075, L"75%");
    AppendMenu(zoomMenu, MF_STRING, IDM_ZOOM_100, L"100%");
    AppendMenu(zoomMenu, MF_STRING, IDM_ZOOM_125, L"125%");
    AppendMenu(zoomMenu, MF_STRING, IDM_ZOOM_150, L"150%");
    AppendMenu(zoomMenu, MF_STRING, IDM_ZOOM_200, L"200%");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)zoomMenu, L"&Zoom");
    HMENU testMenu = CreateMenu();
    AppendMenu(testMenu, MF_STRING, IDM_TEST_V8_APPEND_ELEMENT, L"Append Element Using &V8");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_KEYBOARD_FOCUS, L"Test Keyboard Focus");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_LOGICAL_FOCUS, L"Test Logical Focus");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_LOGICAL_BLUR, L"Test Logical Blur");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_PLAY_KEYBOARD_EVENTS, L"Test Play Keyboard Events");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)testMenu, L"&Test");
    HMENU spellCheckMenu = CreateMenu();
    AppendMenu(spellCheckMenu, MF_STRING, IDM_SPELLCHECK_ENABLED, L"Enable &Spellcheck");
    HMENU autocorrectMenu = CreateMenu();
    AppendMenu(autocorrectMenu, MF_STRING, IDM_AUTOCORRECT_WORDMAP, L"Word &Map");
    AppendMenu(autocorrectMenu, MF_STRING, IDM_AUTOCORRECT_SWAP_ADJACENT_CHARS, L"&Swap Adjacent Chars");
    AppendMenu(spellCheckMenu, MF_POPUP, (UINT_PTR)autocorrectMenu, L"&Autocorrect");
    HMENU languagesMenu = CreateMenu();
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_DE, L"&German");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_EN_GB, L"&English (Great Britain)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_EN_US, L"English (&United States)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_ES, L"&Spanish");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_FR, L"&French");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_IT, L"I&talian");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_PT_BR, L"Portuguese (&Brazil)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_PT_PT, L"Portuguese (&Portugal)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_RU, L"&Russian");
    AppendMenu(spellCheckMenu, MF_POPUP, (UINT_PTR)languagesMenu, L"&Languages");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)spellCheckMenu, L"&Spelling");
    SetMenu(mainWnd, menu);

    HWND hwnd;
    int x = 0;

    hwnd = CreateWindow(L"BUTTON", L"Back",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_BACK, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"Forward",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_FORWARD, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"Reload",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_RELOAD, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"Stop",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_STOP, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"STATIC", L"Find: ",
                        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
                        x, 0, BUTTON_WIDTH*3/4, URLBAR_HEIGHT,
                        mainWnd, 0, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH*3/4;

    HWND findEntryHwnd = CreateWindow(L"EDIT", 0,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                        ES_AUTOVSCROLL | ES_AUTOHSCROLL,  x, 0, BUTTON_WIDTH*6/4,
                        URLBAR_HEIGHT, mainWnd, (HMENU)IDC_FIND, g_instance, 0);

    assert(findEntryHwnd);
    x += BUTTON_WIDTH*6/4;

    hwnd = CreateWindow(L"BUTTON", L"\u2191",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        x, 0, BUTTON_WIDTH/4, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_FIND_PREV, g_instance, 0);

    assert(hwnd);
    x += BUTTON_WIDTH/4;

    hwnd = CreateWindow(L"BUTTON", L"\u2193",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        x, 0, BUTTON_WIDTH/4, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_FIND_NEXT, g_instance, 0);

    assert(hwnd);
    x += BUTTON_WIDTH/4;

    // This control is positioned by resizeSubViews.
    HWND urlEntryWnd = CreateWindow(L"EDIT", 0,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                                    ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                    x, 0, 0, 0, mainWnd, 0, g_instance, 0);
    assert(urlEntryWnd);

    if (!g_defaultEditWndProc)
        g_defaultEditWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(urlEntryWnd, GWLP_WNDPROC));
    SetWindowLongPtr(urlEntryWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(urlEntryWndProc));

    return new Shell(mainWnd, urlEntryWnd, findEntryHwnd, spellCheckMenu, profile, webView);
}

void populateMenuItem(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuItem& item)
{
    UINT flags =  MF_STRING | (item.enabled() ? MF_ENABLED : MF_GRAYED);
    std::string label(item.label().data(),
                      item.label().length());
    if (item.type() == blpwtk2::ContextMenuItem::OPTION) {
        AppendMenuA(menu, flags, menuIdStart + item.action(), label.c_str());
    }
    else if (item.type() == blpwtk2::ContextMenuItem::CHECKABLE_OPTION) {
        flags = flags | (item.checked() ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuA(menu, flags, menuIdStart + item.action(), label.c_str());
    } else if (item.type() ==  blpwtk2::ContextMenuItem::SEPARATOR) {
        AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    } else if (item.type() == blpwtk2::ContextMenuItem::SUBMENU) {
        HMENU popupMenu = CreatePopupMenu();
        flags = flags | MF_POPUP;
        AppendMenuA(menu, flags, (UINT_PTR)popupMenu, label.c_str());
        populateSubmenu(popupMenu, menuIdStart, item);
    }
}

void populateContextMenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuParams& params)
{
    for (int i = 0; i < params.numCustomItems(); ++i) {
        populateMenuItem(menu, menuIdStart, params.customItem(i));
    }
}

void populateSubmenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuItem& item)
{
    for (int i = 0; i < item.numSubMenuItems(); ++i) {
        populateMenuItem(menu, menuIdStart,item.subMenuItem(i));
    }
}

void updateSpellCheckConfig(blpwtk2::Profile* profile)
{
    blpwtk2::SpellCheckConfig config;

    config.enableSpellCheck(g_spellCheckEnabled);
    config.setAutocorrectBehavior(
        g_spellCheckEnabled ? g_autoCorrectBehavior
                            : blpwtk2::SpellCheckConfig::AUTOCORRECT_NONE);

    std::vector<blpwtk2::StringRef> languages;
    for (std::set<std::string>::const_iterator it = g_languages.begin();
                                               it != g_languages.end();
                                               ++it) {
        languages.push_back(it->c_str());
    }
    config.setLanguages(languages.data(), languages.size());

    profile->setSpellCheckConfig(config);
}

void toggleLanguage(blpwtk2::Profile* profile, const std::string& language)
{
    if (g_languages.find(language) == g_languages.end()) {
        g_languages.insert(language);
    }
    else {
        g_languages.erase(language);
    }
    updateSpellCheckConfig(profile);
}

class DummyResourceLoader : public blpwtk2::ResourceLoader {
    // This dummy ResourceLoader handles all "http://cdrive/" requests
    // and responds with the file at the specified path in the C drive.  For
    // example:
    //
    //     http://cdrive/stuff/test.html
    //
    // will return the contents of:
    //
    //     C:\stuff\test.html

public:
    static const char PREFIX[];

    virtual bool canHandleURL(const blpwtk2::StringRef& url) OVERRIDE
    {
        if (url.length() <= (int)strlen(PREFIX))
            return false;
        blpwtk2::StringRef prefix(url.data(), strlen(PREFIX));
        if (!prefix.equals(PREFIX))
            return false;
        return true;
    }

    virtual void start(const blpwtk2::StringRef& url,
                       blpwtk2::ResourceContext* context,
                       void** userData) OVERRIDE
    {
        assert(canHandleURL(url));

        std::string filePath = "C:\\";
        filePath.append(url.data() + strlen(PREFIX),
                        url.length() - strlen(PREFIX));
        std::replace(filePath.begin(), filePath.end(), '/', '\\');

        std::ifstream fstream(filePath.c_str());
        char buffer[1024];
        if (!fstream.is_open()) {
            context->replaceStatusLine("HTTP/1.1 404 Not Found");
            strcpy(buffer, "The specified file was not found.");
            context->addResponseData(buffer, strlen(buffer));
        }
        else {
            while (!fstream.eof()) {
                fstream.read(buffer, sizeof(buffer));
                if (fstream.bad()) {
                    // some other failure
                    context->failed();
                    break;
                }

                assert(fstream.gcount() <= sizeof(buffer));
                context->addResponseData(buffer, fstream.gcount());
            }
        }
        context->finish();
    }

    virtual void cancel(blpwtk2::ResourceContext* context,
                        void* userData)
    {
        assert(false);  // everything is loaded in start(), so we should never
                        // get canceled
    }
};
const char DummyResourceLoader::PREFIX[] = "http://cdrive/";

blpwtk2::ResourceLoader* createInProcessResourceLoader()
{
    return new DummyResourceLoader();
}

const char* getHeaderFooterHTMLContent() {
  return
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<style>\n"
      "  body {\n"
      "    margin: 0px;\n"
      "    width: 0px;\n"
      "  }\n"
      "  .row {\n"
      "    display: table-row;\n"
      "    vertical-align: inherit;\n"
      "  }\n"
      "  #header, #footer {\n"
      "    display: table;\n"
      "    table-layout:fixed;\n"
      "    width: inherit;\n"
      "  }\n"
      "  #header {\n"
      "    vertical-align: top;\n"
      "  }\n"
      "  #footer {\n"
      "    vertical-align: bottom;\n"
      "  }\n"
      "  .text {\n"
      "    display: table-cell;\n"
      "    font-family: sans-serif;\n"
      "    font-size: 8px;\n"
      "    vertical-align: inherit;\n"
      "    white-space: nowrap;\n"
      "  }\n"
      "  #page_number {\n"
      "    text-align: right;\n"
      "  }\n"
      "  #title {\n"
      "    text-align: center;\n"
      "  }\n"
      "  #date, #url {\n"
      "    padding-left: 0.7cm;\n"
      "    padding-right: 0.1cm;\n"
      "  }\n"
      "  #title, #page_number {\n"
      "    padding-left: 0.1cm;\n"
      "    padding-right: 0.7cm;\n"
      "  }\n"
      "  #title, #url {\n"
      "    overflow: hidden;\n"
      "    text-overflow: ellipsis;\n"
      "  }\n"
      "  #title, #date {\n"
      "    padding-bottom: 0cm;\n"
      "    padding-top: 0.4cm;\n"
      "  }\n"
      "  #page_number, #url {\n"
      "    padding-bottom: 0.4cm;\n"
      "    padding-top: 0cm;\n"
      "  }\n"
      "</style>\n"
      "<script>\n"
      "function pixels(value) {\n"
      "  return value + 'px';\n"
      "}\n"
      "function setup(options) {\n"
      "  var body = document.querySelector('body');\n"
      "  var header = document.querySelector('#header');\n"
      "  var content = document.querySelector('#content');\n"
      "  var footer = document.querySelector('#footer');\n"
      "  body.style.width = pixels(options['width']);\n"
      "  body.style.height = pixels(options['height']);\n"
      "  header.style.height = pixels(options['topMargin']);\n"
      "  content.style.height = pixels(options['height'] - options['topMargin'] - \n"
      "    options['bottomMargin']);\n"
      "  footer.style.height = pixels(options['bottomMargin']);\n"
      "  document.querySelector('#date span').innerText =\n"
      "    new Date(options['date']).toLocaleDateString();\n"
      "  document.querySelector('#title span').innerText = options['title'];\n"
      "  document.querySelector('#url span').innerText = options['url'];\n"
      "  document.querySelector('#page_number span').innerText = options['pageNumber'];\n"
      "  document.querySelector('#date').style.width =\n"
      "    pixels(document.querySelector('#date span').offsetWidth);\n"
      "  document.querySelector('#page_number').style.width = \n"
      "    pixels(document.querySelector('#page_number span').offsetWidth);\n"
      "  if (header.offsetHeight > options['topMargin'] + 1) {\n"
      "    header.style.display = 'none';\n"
      "    content.style.height = pixels(options['height'] - options['bottomMargin']);\n"
      "  }\n"
      "  if (footer.offsetHeight > options['bottomMargin'] + 1) {\n"
      "     footer.style.display = 'none';\n"
      "  }\n"
      "}\n"
      "</script>\n"
      "</head>\n"
      "<body>\n"
      "<div id=\"header\">\n"
      "  <div class=\"row\">\n"
      "    <div id=\"date\" class = \"text\"><span/></div>\n"
      "    <div id=\"title\" class = \"text\"><span/></div>\n"
      "  </div>\n"
      "</div>\n"
      "<div id=\"content\">\n"
      "</div>\n"
      "<div id=\"footer\">\n"
      "  <div class=\"row\">\n"
      "    <div id=\"url\" class = \"text\"><span/></div>\n"
      "    <div id=\"page_number\" class = \"text\"><span/></div>\n"
      "  </div>\n"
      "</div>\n"
      "</body>\n"
      "</html>\n";
}
