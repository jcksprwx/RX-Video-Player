#include "online_subtitles_dialog.hpp"
#include "../theme.hpp"
#include "../../core/settings.hpp"
#include "../../core/utils.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <filesystem>

namespace rx::ui {

static const wchar_t* kOnlineSubClass = L"RXOnlineSubtitlesDlgClass";

constexpr UINT IDC_OS_TITLE    = 6001;
constexpr UINT IDC_OS_YEAR     = 6002;
constexpr UINT IDC_OS_SEASON   = 6003;
constexpr UINT IDC_OS_EPISODE  = 6004;
constexpr UINT IDC_OS_LANG     = 6005;
constexpr UINT IDC_OS_SEARCH   = 6006;
constexpr UINT IDC_OS_APIKEY   = 6007;
constexpr UINT IDC_OS_SAVEKEY  = 6008;
constexpr UINT IDC_OS_LIST     = 6009;
constexpr UINT IDC_OS_DL_APPLY = 6010;
constexpr UINT IDC_OS_DL_ONLY  = 6011;
constexpr UINT IDC_OS_CLOSE    = 6012;

constexpr UINT WM_APP_SEARCH_COMPLETE   = WM_APP + 201;
constexpr UINT WM_APP_DOWNLOAD_COMPLETE = WM_APP + 202;

struct LangOption {
    const wchar_t* name;
    const char* code;
};

static const LangOption s_languages[] = {
    { L"English (en)", "en" },
    { L"Hindi (hi)", "hi" },
    { L"Telugu (te)", "te" },
    { L"Tamil (ta)", "ta" },
    { L"Malayalam (ml)", "ml" },
    { L"Bengali (bn)", "bn" },
    { L"Kannada (kn)", "kn" },
    { L"Spanish (es)", "es" },
    { L"French (fr)", "fr" },
    { L"German (de)", "de" },
    { L"Italian (it)", "it" },
    { L"Japanese (ja)", "ja" },
    { L"Korean (ko)", "ko" },
    { L"Chinese (zh)", "zh" },
    { L"Portuguese (pt)", "pt" },
    { L"Russian (ru)", "ru" },
    { L"Arabic (ar)", "ar" },
    { L"All Languages (all)", "all" }
};

RXOnlineSubtitlesDialog::RXOnlineSubtitlesDialog() {
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    m_bgBrush = CreateSolidBrush(theme::kPanelBg);
    m_editBrush = CreateSolidBrush(RGB(24, 24, 24));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = dlgProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = m_bgBrush;
    wc.lpszClassName = kOnlineSubClass;
    RegisterClassExW(&wc);

    m_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    m_fontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

RXOnlineSubtitlesDialog::~RXOnlineSubtitlesDialog() {
    close();
    if (m_workerThread.joinable()) m_workerThread.join();
    if (m_font) DeleteObject(m_font);
    if (m_fontBold) DeleteObject(m_fontBold);
    if (m_bgBrush) DeleteObject(m_bgBrush);
    if (m_editBrush) DeleteObject(m_editBrush);
}

void RXOnlineSubtitlesDialog::close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool RXOnlineSubtitlesDialog::show(HWND parentHwnd, const std::string& currentMediaPath) {
    m_parent = parentHwnd;
    m_currentMediaPath = currentMediaPath;

    if (!m_currentMediaPath.empty()) {
        m_mediaInfo = subtitles::OpenSubtitlesClient::parseFilename(m_currentMediaPath);
        uint64_t fSize = 0;
        uint64_t hash = subtitles::OpenSubtitlesClient::computeMovieHash(m_currentMediaPath, fSize);
        m_movieHash = subtitles::OpenSubtitlesClient::formatMovieHash(hash);
    } else {
        m_mediaInfo = subtitles::ParsedMediaInfo();
        m_movieHash.clear();
    }

    if (m_hwnd && IsWindow(m_hwnd)) {
        // Dialog is already created; update fields
        std::wstring fileText = L"File: " + rx::utils::utf8ToWide(m_mediaInfo.rawFilename.empty() ? "(No media loaded)" : m_mediaInfo.rawFilename);
        if (!m_movieHash.empty()) {
            fileText += L"  [MovieHash: " + rx::utils::utf8ToWide(m_movieHash) + L"]";
        }
        if (m_lblFile) SetWindowTextW(m_lblFile, fileText.c_str());
        if (m_editTitle) SetWindowTextW(m_editTitle, rx::utils::utf8ToWide(m_mediaInfo.cleanTitle).c_str());
        if (m_editYear) SetWindowTextW(m_editYear, (m_mediaInfo.year > 0) ? std::to_wstring(m_mediaInfo.year).c_str() : L"");
        if (m_editSeason) SetWindowTextW(m_editSeason, (m_mediaInfo.season > 0) ? std::to_wstring(m_mediaInfo.season).c_str() : L"");
        if (m_editEpisode) SetWindowTextW(m_editEpisode, (m_mediaInfo.episode > 0) ? std::to_wstring(m_mediaInfo.episode).c_str() : L"");

        SetForegroundWindow(m_hwnd);
        return true;
    }

    int dlgW = 750;
    int dlgH = 550;
    RECT pr;
    GetWindowRect(parentHwnd, &pr);
    int posX = pr.left + (pr.right - pr.left - dlgW) / 2;
    int posY = pr.top + (pr.bottom - pr.top - dlgH) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kOnlineSubClass,
        L"Download Subtitles Online \x2014 OpenSubtitles.com",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
        posX, posY, dlgW, dlgH,
        parentHwnd, nullptr, GetModuleHandleW(nullptr), this
    );

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    std::string key = Settings::instance().opensubtitles.apiKey;
    if (key.empty()) {
        char* envKey = getenv("OPENSUBTITLES_API_KEY");
        if (envKey) key = envKey;
    }

    if (key.empty()) {
        updateStatus(L"Notice: OpenSubtitles API key is required. Enter your free API key above to search.");
    } else if (!m_mediaInfo.cleanTitle.empty()) {
        onSearchClicked();
    }

    return true;
}

void RXOnlineSubtitlesDialog::onInitDialog() {
    // 1. Current file label
    std::wstring fileText = L"File: " + rx::utils::utf8ToWide(m_mediaInfo.rawFilename.empty() ? "(No media loaded)" : m_mediaInfo.rawFilename);
    if (!m_movieHash.empty()) {
        fileText += L"  [MovieHash: " + rx::utils::utf8ToWide(m_movieHash) + L"]";
    }

    m_lblFile = CreateWindowExW(0, L"STATIC", fileText.c_str(),
                                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                16, 12, 700, 20, m_hwnd, nullptr, nullptr, nullptr);
    SendMessageW(m_lblFile, WM_SETFONT, (WPARAM)m_fontBold, TRUE);

    // 2. Search controls row
    CreateWindowExW(0, L"STATIC", L"Title:", WS_CHILD | WS_VISIBLE,
                    16, 40, 40, 20, m_hwnd, nullptr, nullptr, nullptr);
    m_editTitle = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", rx::utils::utf8ToWide(m_mediaInfo.cleanTitle).c_str(),
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 60, 38, 220, 24, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_TITLE, nullptr, nullptr);
    SendMessageW(m_editTitle, WM_SETFONT, (WPARAM)m_font, TRUE);

    CreateWindowExW(0, L"STATIC", L"Year:", WS_CHILD | WS_VISIBLE,
                    290, 40, 35, 20, m_hwnd, nullptr, nullptr, nullptr);
    std::wstring yearStr = (m_mediaInfo.year > 0) ? std::to_wstring(m_mediaInfo.year) : L"";
    m_editYear = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", yearStr.c_str(),
                                WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                330, 38, 55, 24, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_YEAR, nullptr, nullptr);
    SendMessageW(m_editYear, WM_SETFONT, (WPARAM)m_font, TRUE);

    CreateWindowExW(0, L"STATIC", L"Season:", WS_CHILD | WS_VISIBLE,
                    395, 40, 50, 20, m_hwnd, nullptr, nullptr, nullptr);
    std::wstring sStr = (m_mediaInfo.season > 0) ? std::to_wstring(m_mediaInfo.season) : L"";
    m_editSeason = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", sStr.c_str(),
                                  WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                  450, 38, 40, 24, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_SEASON, nullptr, nullptr);
    SendMessageW(m_editSeason, WM_SETFONT, (WPARAM)m_font, TRUE);

    CreateWindowExW(0, L"STATIC", L"Ep:", WS_CHILD | WS_VISIBLE,
                    500, 40, 25, 20, m_hwnd, nullptr, nullptr, nullptr);
    std::wstring epStr = (m_mediaInfo.episode > 0) ? std::to_wstring(m_mediaInfo.episode) : L"";
    m_editEpisode = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", epStr.c_str(),
                                   WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                   525, 38, 40, 24, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_EPISODE, nullptr, nullptr);
    SendMessageW(m_editEpisode, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_comboLang = CreateWindowExW(0, L"COMBOBOX", L"",
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  575, 38, 140, 200, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_LANG, nullptr, nullptr);
    SendMessageW(m_comboLang, WM_SETFONT, (WPARAM)m_font, TRUE);
    for (const auto& l : s_languages) {
        SendMessageW(m_comboLang, CB_ADDSTRING, 0, (LPARAM)l.name);
    }
    SendMessageW(m_comboLang, CB_SETCURSEL, 0, 0); // Default to English

    // 3. API Key row
    CreateWindowExW(0, L"STATIC", L"API Key:", WS_CHILD | WS_VISIBLE,
                    16, 72, 55, 20, m_hwnd, nullptr, nullptr, nullptr);
    std::string currentKey = Settings::instance().opensubtitles.apiKey;
    if (currentKey.empty()) {
        char* envKey = getenv("OPENSUBTITLES_API_KEY");
        if (envKey) currentKey = envKey;
    }
    m_editApiKey = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", rx::utils::utf8ToWide(currentKey).c_str(),
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                                  75, 70, 360, 24, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_APIKEY, nullptr, nullptr);
    SendMessageW(m_editApiKey, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnSaveKey = CreateWindowExW(0, L"BUTTON", L"Save Key",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  445, 70, 75, 24, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_SAVEKEY, nullptr, nullptr);
    SendMessageW(m_btnSaveKey, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnSearch = CreateWindowExW(0, L"BUTTON", L"Search Subtitles",
                                 WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                 530, 70, 185, 24, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_SEARCH, nullptr, nullptr);
    SendMessageW(m_btnSearch, WM_SETFONT, (WPARAM)m_fontBold, TRUE);

    // 4. Results List View
    m_listResults = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_VSCROLL,
        16, 104, 700, 340, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_LIST, nullptr, nullptr
    );
    SendMessageW(m_listResults, WM_SETFONT, (WPARAM)m_font, TRUE);
    ListView_SetExtendedListViewStyle(m_listResults, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(m_listResults, RGB(24, 24, 24));
    ListView_SetTextBkColor(m_listResults, RGB(24, 24, 24));
    ListView_SetTextColor(m_listResults, RGB(230, 230, 230));

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0; lvc.cx = 100; lvc.pszText = (LPWSTR)L"Language";
    ListView_InsertColumn(m_listResults, 0, &lvc);

    lvc.iSubItem = 1; lvc.cx = 290; lvc.pszText = (LPWSTR)L"Subtitle / Release Name";
    ListView_InsertColumn(m_listResults, 1, &lvc);

    lvc.iSubItem = 2; lvc.cx = 65;  lvc.pszText = (LPWSTR)L"Format";
    ListView_InsertColumn(m_listResults, 2, &lvc);

    lvc.iSubItem = 3; lvc.cx = 85;  lvc.pszText = (LPWSTR)L"Downloads";
    ListView_InsertColumn(m_listResults, 3, &lvc);

    lvc.iSubItem = 4; lvc.cx = 75;  lvc.pszText = (LPWSTR)L"Rating";
    ListView_InsertColumn(m_listResults, 4, &lvc);

    lvc.iSubItem = 5; lvc.cx = 85;  lvc.pszText = (LPWSTR)L"Hearing Imp.";
    ListView_InsertColumn(m_listResults, 5, &lvc);

    // 5. Bottom action bar
    m_lblStatus = CreateWindowExW(0, L"STATIC", L"Ready. Enter search criteria or click Search Subtitles.",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                 16, 460, 360, 22, m_hwnd, nullptr, nullptr, nullptr);
    SendMessageW(m_lblStatus, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnDownloadApply = CreateWindowExW(0, L"BUTTON", L"\x2B07 Download & Apply",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        390, 456, 140, 28, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_DL_APPLY, nullptr, nullptr);
    SendMessageW(m_btnDownloadApply, WM_SETFONT, (WPARAM)m_fontBold, TRUE);

    m_btnDownloadOnly = CreateWindowExW(0, L"BUTTON", L"Download Only",
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       540, 456, 100, 28, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_DL_ONLY, nullptr, nullptr);
    SendMessageW(m_btnDownloadOnly, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnClose = CreateWindowExW(0, L"BUTTON", L"Close",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                650, 456, 65, 28, m_hwnd, (HMENU)(UINT_PTR)IDC_OS_CLOSE, nullptr, nullptr);
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)m_font, TRUE);

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXOnlineSubtitlesDialog::onResize(int width, int height) {
    if (!m_hwnd || width < 400 || height < 300) return;

    SetWindowPos(m_lblFile, nullptr, 16, 12, width - 32, 20, SWP_NOZORDER);

    // Position API Key row responsively
    int searchBtnW = 160;
    int searchBtnX = width - 16 - searchBtnW;
    SetWindowPos(m_btnSearch, nullptr, searchBtnX, 70, searchBtnW, 24, SWP_NOZORDER);
    int saveKeyW = 75;
    int saveKeyX = searchBtnX - 8 - saveKeyW;
    SetWindowPos(m_btnSaveKey, nullptr, saveKeyX, 70, saveKeyW, 24, SWP_NOZORDER);
    int keyEditW = saveKeyX - 8 - 75;
    if (keyEditW > 100) {
        SetWindowPos(m_editApiKey, nullptr, 75, 70, keyEditW, 24, SWP_NOZORDER);
    }

    int btnTop = height - 44;
    SetWindowPos(m_listResults, nullptr, 16, 104, width - 32, btnTop - 114, SWP_NOZORDER);

    SetWindowPos(m_lblStatus, nullptr, 16, btnTop + 4, width - 350, 22, SWP_NOZORDER);
    SetWindowPos(m_btnDownloadApply, nullptr, width - 335, btnTop, 140, 28, SWP_NOZORDER);
    SetWindowPos(m_btnDownloadOnly, nullptr, width - 185, btnTop, 105, 28, SWP_NOZORDER);
    SetWindowPos(m_btnClose, nullptr, width - 72, btnTop, 56, 28, SWP_NOZORDER);
}

void RXOnlineSubtitlesDialog::updateStatus(const std::wstring& status) {
    if (m_lblStatus && IsWindow(m_lblStatus)) {
        SetWindowTextW(m_lblStatus, status.c_str());
    }
}

void RXOnlineSubtitlesDialog::onSearchClicked() {
    if (m_isBusy) return;

    wchar_t bufTitle[256] = {0};
    GetWindowTextW(m_editTitle, bufTitle, 256);
    std::string query = rx::utils::wideToUtf8(bufTitle);

    wchar_t bufYear[16] = {0};
    GetWindowTextW(m_editYear, bufYear, 16);
    int year = (wcslen(bufYear) > 0) ? _wtoi(bufYear) : 0;

    wchar_t bufS[16] = {0};
    GetWindowTextW(m_editSeason, bufS, 16);
    int season = (wcslen(bufS) > 0) ? _wtoi(bufS) : 0;

    wchar_t bufEp[16] = {0};
    GetWindowTextW(m_editEpisode, bufEp, 16);
    int episode = (wcslen(bufEp) > 0) ? _wtoi(bufEp) : 0;

    int selLang = (int)SendMessageW(m_comboLang, CB_GETCURSEL, 0, 0);
    std::string langCode = "en";
    if (selLang >= 0 && selLang < static_cast<int>(sizeof(s_languages) / sizeof(s_languages[0]))) {
        langCode = s_languages[selLang].code;
    }

    wchar_t bufKey[256] = {0};
    GetWindowTextW(m_editApiKey, bufKey, 256);
    std::string apiKey = rx::utils::wideToUtf8(bufKey);

    if (apiKey.empty()) {
        updateStatus(L"Please enter an OpenSubtitles API key (get one free at opensubtitles.com).");
        return;
    }

    m_isBusy = true;
    updateStatus(L"Searching OpenSubtitles.com...");
    EnableWindow(m_btnSearch, FALSE);
    ListView_DeleteAllItems(m_listResults);

    if (m_workerThread.joinable()) m_workerThread.join();

    m_workerThread = std::thread([this, apiKey, query, langCode, season, episode, year]() {
        std::string err;
        // Search by MovieHash first, if available
        std::vector<subtitles::SubtitleSearchResult> res =
            subtitles::OpenSubtitlesClient::searchSubtitles(apiKey, query, langCode, m_movieHash, season, episode, year, &err);

        // If hash returned empty and query is provided, fallback to title search
        if (res.empty() && !m_movieHash.empty() && !query.empty()) {
            res = subtitles::OpenSubtitlesClient::searchSubtitles(apiKey, query, langCode, "", season, episode, year, &err);
        }

        m_results = std::move(res);
        m_lastSearchError = err;
        m_isBusy = false;
        PostMessageW(m_hwnd, WM_APP_SEARCH_COMPLETE, (WPARAM)m_results.size(), (LPARAM)(err.empty() ? 0 : 1));
    });
}

void RXOnlineSubtitlesDialog::populateResults() {
    ListView_DeleteAllItems(m_listResults);

    for (size_t i = 0; i < m_results.size(); ++i) {
        const auto& r = m_results[i];

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = static_cast<int>(i);

        // 0: Language
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)r.languageName.c_str();
        ListView_InsertItem(m_listResults, &lvi);

        // 1: Release / Filename
        std::wstring name = rx::utils::utf8ToWide(r.fileName.empty() ? r.release : r.fileName);
        ListView_SetItemText(m_listResults, static_cast<int>(i), 1, (LPWSTR)name.c_str());

        // 2: Format
        std::wstring fmt = rx::utils::utf8ToWide(r.format);
        ListView_SetItemText(m_listResults, static_cast<int>(i), 2, (LPWSTR)fmt.c_str());

        // 3: Downloads
        std::wstring dlStr = std::to_wstring(r.downloadCount);
        ListView_SetItemText(m_listResults, static_cast<int>(i), 3, (LPWSTR)dlStr.c_str());

        // 4: Rating
        wchar_t rBuf[32];
        if (r.rating > 0.0) {
            swprintf_s(rBuf, L"\x2605 %.1f", r.rating);
        } else {
            wcscpy_s(rBuf, L"-");
        }
        ListView_SetItemText(m_listResults, static_cast<int>(i), 4, rBuf);

        // 5: Hearing Impaired
        ListView_SetItemText(m_listResults, static_cast<int>(i), 5, (LPWSTR)(r.hearingImpaired ? L"Yes" : L"No"));
    }

    if (!m_results.empty()) {
        ListView_SetItemState(m_listResults, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void RXOnlineSubtitlesDialog::onDownloadClicked(bool applyNow) {
    if (m_isBusy) return;

    int sel = ListView_GetNextItem(m_listResults, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_results.size())) {
        MessageBoxW(m_hwnd, L"Please select a subtitle from the list to download.", L"Download Subtitles", MB_ICONINFORMATION | MB_OK);
        return;
    }

    const auto& item = m_results[sel];
    if (item.fileId <= 0) {
        MessageBoxW(m_hwnd, L"Invalid subtitle file ID.", L"Download Error", MB_ICONERROR | MB_OK);
        return;
    }

    wchar_t bufKey[256] = {0};
    GetWindowTextW(m_editApiKey, bufKey, 256);
    std::string apiKey = rx::utils::wideToUtf8(bufKey);

    // Determine save path: adjacent to media file or under AppData/Subtitles
    std::filesystem::path destPath;
    if (!m_currentMediaPath.empty()) {
        std::filesystem::path mediaP(m_currentMediaPath);
        destPath = mediaP.parent_path() / (mediaP.stem().string() + "." + item.language + ".srt");
    } else {
        wchar_t appData[MAX_PATH];
        GetEnvironmentVariableW(L"LOCALAPPDATA", appData, MAX_PATH);
        destPath = std::filesystem::path(appData) / "RX Media Player" / "Subtitles" / (item.fileName.empty() ? "downloaded_subtitle.srt" : item.fileName);
    }

    m_isBusy = true;
    updateStatus(L"Downloading subtitle from OpenSubtitles CDN...");
    EnableWindow(m_btnDownloadApply, FALSE);
    EnableWindow(m_btnDownloadOnly, FALSE);

    if (m_workerThread.joinable()) m_workerThread.join();

    m_workerThread = std::thread([this, apiKey, item, destPath, applyNow]() {
        std::string err;
        bool ok = subtitles::OpenSubtitlesClient::downloadSubtitle(apiKey, item.fileId, destPath.string(), &err);

        m_isBusy = false;
        PostMessageW(m_hwnd, WM_APP_DOWNLOAD_COMPLETE, (WPARAM)(ok ? 1 : 0), (LPARAM)(applyNow ? 1 : 0));

        if (ok && m_downloadCb) {
            m_downloadCb(destPath.string(), applyNow);
        }
    });
}

LRESULT CALLBACK RXOnlineSubtitlesDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXOnlineSubtitlesDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXOnlineSubtitlesDialog*>(cs->lpCreateParams);
        pThis->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<RXOnlineSubtitlesDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE: {
            pThis->onInitDialog();
            return 0;
        }
        case WM_SIZE: {
            pThis->onResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(220, 220, 220));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)pThis->m_bgBrush;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(245, 245, 245));
            SetBkColor(hdcEdit, RGB(24, 24, 24));
            return (LRESULT)pThis->m_editBrush;
        }
        case WM_CTLCOLORBTN: {
            return (LRESULT)pThis->m_bgBrush;
        }
        case WM_COMMAND: {
            UINT id = LOWORD(wParam);
            if (id == IDC_OS_SEARCH) {
                pThis->onSearchClicked();
            } else if (id == IDC_OS_SAVEKEY) {
                wchar_t bufKey[256] = {0};
                GetWindowTextW(pThis->m_editApiKey, bufKey, 256);
                Settings::instance().opensubtitles.apiKey = rx::utils::wideToUtf8(bufKey);
                Settings::instance().save();
                MessageBoxW(hwnd, L"OpenSubtitles API Key saved successfully.", L"API Key Saved", MB_ICONINFORMATION | MB_OK);
            } else if (id == IDC_OS_DL_APPLY) {
                pThis->onDownloadClicked(true);
            } else if (id == IDC_OS_DL_ONLY) {
                pThis->onDownloadClicked(false);
            } else if (id == IDC_OS_CLOSE || id == IDCANCEL) {
                pThis->close();
            }
            return 0;
        }
        case WM_NOTIFY: {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr && hdr->idFrom == IDC_OS_LIST && hdr->code == NM_DBLCLK) {
                pThis->onDownloadClicked(true); // Double click downloads and applies
            }
            return 0;
        }
        case WM_APP_SEARCH_COMPLETE: {
            EnableWindow(pThis->m_btnSearch, TRUE);
            pThis->populateResults();
            size_t count = pThis->m_results.size();
            if (count > 0) {
                pThis->updateStatus(L"Found " + std::to_wstring(count) + L" matching subtitles. Select one and click Download.");
            } else if (!pThis->m_lastSearchError.empty()) {
                pThis->updateStatus(L"Search error: " + rx::utils::utf8ToWide(pThis->m_lastSearchError));
            } else {
                pThis->updateStatus(L"No subtitles found for this title. Try a different title or select 'All Languages'.");
            }
            return 0;
        }
        case WM_APP_DOWNLOAD_COMPLETE: {
            EnableWindow(pThis->m_btnDownloadApply, TRUE);
            EnableWindow(pThis->m_btnDownloadOnly, TRUE);
            bool ok = (wParam != 0);
            bool applied = (lParam != 0);
            if (ok) {
                pThis->updateStatus(applied ? L"Subtitle downloaded and applied to video successfully!" : L"Subtitle downloaded successfully!");
            } else {
                pThis->updateStatus(L"Subtitle download failed. Please check network and API quota.");
            }
            return 0;
        }
        case WM_CLOSE: {
            pThis->close();
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace rx::ui
