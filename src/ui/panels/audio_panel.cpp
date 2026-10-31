#include "audio_panel.hpp"
#include "../../core/utils.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <iomanip>
#include <sstream>

namespace rx::ui {

static const wchar_t* kAudioPanelClass = L"RXAudioPanelClass";
static bool s_audioPanelClassRegistered = false;

constexpr UINT IDC_AP_COMBO_DEVICE    = 6001;
constexpr UINT IDC_AP_CHK_EXCLUSIVE   = 6002;
constexpr UINT IDC_AP_CHK_NORMALIZE   = 6003;
constexpr UINT IDC_AP_CHK_DOWNMIX     = 6004;

constexpr UINT IDC_AP_TB_DELAY        = 6010;
constexpr UINT IDC_AP_CHK_EQ_ENABLE   = 6020;
constexpr UINT IDC_AP_COMBO_PRESET    = 6021;
constexpr UINT IDC_AP_BTN_RESET_EQ    = 6022;

constexpr UINT IDC_AP_TB_BASS         = 6030;
constexpr UINT IDC_AP_TB_TREBLE       = 6031;
constexpr UINT IDC_AP_TB_BALANCE      = 6032;

constexpr UINT IDC_AP_COMBO_TRACKS    = 6040;
constexpr UINT IDC_AP_TB_BAND_BASE    = 6100; // 6100 to 6109

RXAudioPanel::RXAudioPanel(MpvBackend& engine)
    : m_engine(engine) {}

RXAudioPanel::~RXAudioPanel() {
    if (m_font) DeleteObject(m_font);
    if (m_titleFont) DeleteObject(m_titleFont);
    if (m_smallFont) DeleteObject(m_smallFont);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool RXAudioPanel::create(HWND parentHwnd, int x, int y, int width, int height) {
    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_audioPanelClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXAudioPanel::wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kAudioPanelClass;
        RegisterClassExW(&wc);
        s_audioPanelClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, kAudioPanelClass, L"Audio & Equalizer",
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    m_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_titleFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_smallFont = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int curY = 10;
    int padX = 12;
    int fullW = width - padX * 2;

    // 1. Audio Device Section
    HWND lblDev = CreateWindowExW(0, L"STATIC", L"Output Device (WASAPI):", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblDev, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 20;

    m_comboDevice = CreateWindowExW(0, L"COMBOBOX", L"",
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   padX, curY, fullW, 140, m_hwnd, (HMENU)(INT_PTR)IDC_AP_COMBO_DEVICE, hInst, nullptr);
    SendMessageW(m_comboDevice, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 30;

    // Checkboxes: Exclusive mode, Normalize, Downmix
    m_chkExclusive = CreateWindowExW(0, L"BUTTON", L"Exclusive Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                     padX, curY, 110, 20, m_hwnd, (HMENU)(INT_PTR)IDC_AP_CHK_EXCLUSIVE, hInst, nullptr);
    m_chkNormalize = CreateWindowExW(0, L"BUTTON", L"EBU R128 Normalization", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                     padX + 115, curY, 160, 20, m_hwnd, (HMENU)(INT_PTR)IDC_AP_CHK_NORMALIZE, hInst, nullptr);
    curY += 22;

    m_chkDownmix = CreateWindowExW(0, L"BUTTON", L"Stereo Downmix", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   padX, curY, 120, 20, m_hwnd, (HMENU)(INT_PTR)IDC_AP_CHK_DOWNMIX, hInst, nullptr);

    SendMessageW(m_chkExclusive, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkNormalize, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkDownmix,   WM_SETFONT, (WPARAM)m_font, TRUE);

    curY += 26;

    // 2. Audio Delay Section
    HWND lblDel = CreateWindowExW(0, L"STATIC", L"A/V Sync Delay:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  padX, curY + 2, 95, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblDel, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_tbDelay = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                                padX + 100, curY, fullW - 165, 24, m_hwnd, (HMENU)(INT_PTR)IDC_AP_TB_DELAY, hInst, nullptr);
    SendMessageW(m_tbDelay, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000)); // offset by 500: 500 = 0ms, step = 10ms
    SendMessageW(m_tbDelay, TBM_SETPOS, TRUE, 500);

    m_lblValDelay = CreateWindowExW(0, L"STATIC", L"0 ms", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                    padX + fullW - 60, curY + 2, 60, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValDelay, WM_SETFONT, (WPARAM)m_font, TRUE);

    curY += 32;

    // 3. 10-Band Graphic Equalizer Section
    m_chkEqEnable = CreateWindowExW(0, L"BUTTON", L"10-Band Graphic Equalizer", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                    padX, curY, 180, 20, m_hwnd, (HMENU)(INT_PTR)IDC_AP_CHK_EQ_ENABLE, hInst, nullptr);
    SendMessageW(m_chkEqEnable, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_chkEqEnable, WM_SETFONT, (WPARAM)m_titleFont, TRUE);

    m_comboPreset = CreateWindowExW(0, L"COMBOBOX", L"",
                                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                    padX + 185, curY - 2, fullW - 185, 140, m_hwnd, (HMENU)(INT_PTR)IDC_AP_COMBO_PRESET, hInst, nullptr);
    SendMessageW(m_comboPreset, WM_SETFONT, (WPARAM)m_font, TRUE);

    // Populate Presets
    const wchar_t* presets[] = { L"Flat", L"Rock", L"Pop", L"Jazz", L"Classical", L"Speech", L"Bass Boost", L"Treble Boost", L"Movie", L"Custom" };
    for (const auto* p : presets) {
        SendMessageW(m_comboPreset, CB_ADDSTRING, 0, (LPARAM)p);
    }
    SendMessageW(m_comboPreset, CB_SETCURSEL, 0, 0);

    curY += 28;

    // 10 Vertical Sliders
    int bandW = (fullW - 5) / audio::kEqualizerBandCount;
    int sliderHeight = 85;

    const wchar_t* bandNames[] = { L"31", L"62", L"125", L"250", L"500", L"1k", L"2k", L"4k", L"8k", L"16k" };

    for (size_t i = 0; i < audio::kEqualizerBandCount; ++i) {
        int bx = padX + static_cast<int>(i) * bandW;

        m_tbBands[i] = CreateWindowExW(
            0, TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_AUTOTICKS | TBS_NOTICKS,
            bx, curY, bandW, sliderHeight,
            m_hwnd, (HMENU)(INT_PTR)(IDC_AP_TB_BAND_BASE + i), hInst, nullptr
        );
        // Range: -12dB to +12dB -> 0 to 24 (12 is 0dB, inverted for vertical trackbar: top is max)
        SendMessageW(m_tbBands[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 24));
        SendMessageW(m_tbBands[i], TBM_SETPOS, TRUE, 12);

        m_lblBands[i] = CreateWindowExW(
            0, L"STATIC", bandNames[i],
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            bx, curY + sliderHeight + 2, bandW, 16,
            m_hwnd, nullptr, hInst, nullptr
        );
        SendMessageW(m_lblBands[i], WM_SETFONT, (WPARAM)m_smallFont, TRUE);
    }

    curY += sliderHeight + 22;

    // Reset EQ Button
    m_btnResetEq = CreateWindowExW(0, L"BUTTON", L"Reset Equalizer", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  padX, curY, fullW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_AP_BTN_RESET_EQ, hInst, nullptr);
    SendMessageW(m_btnResetEq, WM_SETFONT, (WPARAM)m_font, TRUE);

    curY += 32;

    // 4. Tone Controls: Bass, Treble, Balance
    auto createToneSlider = [&](const wchar_t* label, UINT id, HWND& tb, HWND& lbl) {
        HWND l = CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                padX, curY + 2, 60, 20, m_hwnd, nullptr, hInst, nullptr);
        SendMessageW(l, WM_SETFONT, (WPARAM)m_font, TRUE);

        tb = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                             padX + 65, curY, fullW - 120, 24, m_hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(0, 20)); // 0 to 20 -> -10dB to +10dB
        SendMessageW(tb, TBM_SETPOS, TRUE, 10);

        lbl = CreateWindowExW(0, L"STATIC", L"0 dB", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                              padX + fullW - 50, curY + 2, 50, 20, m_hwnd, nullptr, hInst, nullptr);
        SendMessageW(lbl, WM_SETFONT, (WPARAM)m_font, TRUE);

        curY += 26;
    };

    createToneSlider(L"Bass", IDC_AP_TB_BASS, m_tbBass, m_lblValBass);
    createToneSlider(L"Treble", IDC_AP_TB_TREBLE, m_tbTreble, m_lblValTreble);

    // Balance
    HWND lblBal = CreateWindowExW(0, L"STATIC", L"Balance", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 padX, curY + 2, 60, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblBal, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_tbBalance = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                                  padX + 65, curY, fullW - 120, 24, m_hwnd, (HMENU)(INT_PTR)IDC_AP_TB_BALANCE, hInst, nullptr);
    SendMessageW(m_tbBalance, TBM_SETRANGE, TRUE, MAKELPARAM(0, 20)); // 10 = center
    SendMessageW(m_tbBalance, TBM_SETPOS, TRUE, 10);

    m_lblValBalance = CreateWindowExW(0, L"STATIC", L"Center", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                      padX + fullW - 50, curY + 2, 50, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValBalance, WM_SETFONT, (WPARAM)m_font, TRUE);

    curY += 30;

    // 5. Audio Tracks
    HWND lblTrk = CreateWindowExW(0, L"STATIC", L"Audio Track:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  padX, curY, 80, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblTrk, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_comboTracks = CreateWindowExW(0, L"COMBOBOX", L"",
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   padX + 85, curY - 2, fullW - 85, 120, m_hwnd, (HMENU)(INT_PTR)IDC_AP_COMBO_TRACKS, hInst, nullptr);
    SendMessageW(m_comboTracks, WM_SETFONT, (WPARAM)m_font, TRUE);

    refreshDevices();
    refreshTracks();
    updateLabels();
    return true;
}

void RXAudioPanel::move(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

void RXAudioPanel::setVisible(bool visible) {
    m_visible = visible;
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
        if (visible) {
            refreshDevices();
            refreshTracks();
        }
    }
}

void RXAudioPanel::refreshDevices() {
    if (!m_comboDevice) return;
    SendMessageW(m_comboDevice, CB_RESETCONTENT, 0, 0);

    m_endpoints = audio::WasapiEnumerator::enumerateEndpoints();
    int selIdx = 0;
    for (size_t i = 0; i < m_endpoints.size(); ++i) {
        std::wstring nameW = rx::utils::utf8ToWide(m_endpoints[i].name);
        SendMessageW(m_comboDevice, CB_ADDSTRING, 0, (LPARAM)nameW.c_str());
        if (m_endpoints[i].id == m_selectedDeviceId) {
            selIdx = static_cast<int>(i);
        }
    }
    SendMessageW(m_comboDevice, CB_SETCURSEL, selIdx, 0);
}

void RXAudioPanel::refreshTracks() {
    if (!m_comboTracks) return;
    SendMessageW(m_comboTracks, CB_RESETCONTENT, 0, 0);

    const auto& media = m_engine.getMediaInfo();
    int selIdx = 0;
    int curIndex = 0;

    SendMessageW(m_comboTracks, CB_ADDSTRING, 0, (LPARAM)L"None (Disabled)");

    for (const auto& track : media.tracks) {
        if (track.type == "audio") {
            curIndex++;
            std::wstring desc = std::to_wstring(track.id) + L": ";
            if (!track.title.empty()) desc += rx::utils::utf8ToWide(track.title);
            else if (!track.lang.empty()) desc += rx::utils::utf8ToWide(track.lang);
            else desc += L"Audio Track";

            if (!track.codec.empty()) desc += L" [" + rx::utils::utf8ToWide(track.codec) + L"]";

            SendMessageW(m_comboTracks, CB_ADDSTRING, 0, (LPARAM)desc.c_str());
            if (track.selected) {
                selIdx = curIndex;
            }
        }
    }
    SendMessageW(m_comboTracks, CB_SETCURSEL, selIdx, 0);
}

void RXAudioPanel::setEqualizerState(const audio::EqualizerState& state) {
    m_eqState = state;
    if (!m_hwnd) return;

    SendMessageW(m_chkEqEnable, BM_SETCHECK, state.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkNormalize, BM_SETCHECK, state.normalize ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkDownmix, BM_SETCHECK, state.stereoDownmix ? BST_CHECKED : BST_UNCHECKED, 0);

    SendMessageW(m_comboPreset, CB_SETCURSEL, static_cast<int>(state.preset), 0);

    for (size_t i = 0; i < audio::kEqualizerBandCount; ++i) {
        // Vertical trackbar: 0 is top (+12dB), 12 is 0dB, 24 is -12dB
        int sliderVal = 12 - static_cast<int>(state.gains[i]);
        SendMessageW(m_tbBands[i], TBM_SETPOS, TRUE, sliderVal);
    }

    SendMessageW(m_tbBass, TBM_SETPOS, TRUE, static_cast<int>(state.bass) + 10);
    SendMessageW(m_tbTreble, TBM_SETPOS, TRUE, static_cast<int>(state.treble) + 10);
    SendMessageW(m_tbBalance, TBM_SETPOS, TRUE, static_cast<int>(state.leftRightBalance * 10.0) + 10);

    updateLabels();
}

void RXAudioPanel::updateLabels() {
    // Delay
    std::wstring delStr;
    if (m_audioDelayMs > 0) delStr = L"+" + std::to_wstring(static_cast<int>(m_audioDelayMs)) + L" ms";
    else delStr = std::to_wstring(static_cast<int>(m_audioDelayMs)) + L" ms";
    SetWindowTextW(m_lblValDelay, delStr.c_str());

    // Bass & Treble
    auto fmtDb = [](double val) {
        std::wostringstream ss;
        if (val > 0) ss << L"+";
        ss << std::fixed << std::setprecision(1) << val << L" dB";
        return ss.str();
    };
    SetWindowTextW(m_lblValBass, fmtDb(m_eqState.bass).c_str());
    SetWindowTextW(m_lblValTreble, fmtDb(m_eqState.treble).c_str());

    // Balance
    if (std::abs(m_eqState.leftRightBalance) < 0.05) {
        SetWindowTextW(m_lblValBalance, L"Center");
    } else if (m_eqState.leftRightBalance < 0.0) {
        int pct = static_cast<int>(std::abs(m_eqState.leftRightBalance) * 100.0);
        SetWindowTextW(m_lblValBalance, (L"L " + std::to_wstring(pct) + L"%").c_str());
    } else {
        int pct = static_cast<int>(m_eqState.leftRightBalance * 100.0);
        SetWindowTextW(m_lblValBalance, (L"R " + std::to_wstring(pct) + L"%").c_str());
    }
}

void RXAudioPanel::notifyChange() {
    updateLabels();
    if (m_callback) {
        m_callback(m_eqState, m_selectedDeviceId, m_audioDelayMs, m_exclusiveMode);
    }
}

LRESULT CALLBACK RXAudioPanel::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXAudioPanel* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXAudioPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXAudioPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                pThis->onPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_VSCROLL: {
                HWND tb = reinterpret_cast<HWND>(lParam);
                int pos = static_cast<int>(SendMessageW(tb, TBM_GETPOS, 0, 0));
                pThis->onVScroll(tb, pos);
                return 0;
            }
            case WM_HSCROLL: {
                HWND tb = reinterpret_cast<HWND>(lParam);
                int pos = static_cast<int>(SendMessageW(tb, TBM_GETPOS, 0, 0));
                pThis->onHScroll(tb, pos);
                return 0;
            }
            case WM_COMMAND:
                pThis->onCommand(LOWORD(wParam), HIWORD(wParam));
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXAudioPanel::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Left border separator line
    HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    MoveToEx(hdc, 0, 0, nullptr);
    LineTo(hdc, 0, rc.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

void RXAudioPanel::onVScroll(HWND tb, int pos) {
    for (size_t i = 0; i < audio::kEqualizerBandCount; ++i) {
        if (tb == m_tbBands[i]) {
            // Invert pos: 0 is +12dB, 24 is -12dB
            double gain = static_cast<double>(12 - pos);
            m_eqState.gains[i] = gain;
            m_eqState.preset = audio::EqualizerPreset::Custom;
            SendMessageW(m_comboPreset, CB_SETCURSEL, static_cast<int>(audio::EqualizerPreset::Custom), 0);
            notifyChange();
            return;
        }
    }
}

void RXAudioPanel::onHScroll(HWND tb, int pos) {
    if (tb == m_tbDelay) {
        // pos: 0 to 1000, 500 = 0ms, step = 10ms
        m_audioDelayMs = static_cast<double>((pos - 500) * 10);
        notifyChange();
    } else if (tb == m_tbBass) {
        m_eqState.bass = static_cast<double>(pos - 10);
        notifyChange();
    } else if (tb == m_tbTreble) {
        m_eqState.treble = static_cast<double>(pos - 10);
        notifyChange();
    } else if (tb == m_tbBalance) {
        m_eqState.leftRightBalance = static_cast<double>(pos - 10) / 10.0;
        notifyChange();
    }
}

void RXAudioPanel::onCommand(WORD id, WORD code) {
    if (id == IDC_AP_COMBO_DEVICE && code == CBN_SELCHANGE) {
        int sel = static_cast<int>(SendMessageW(m_comboDevice, CB_GETCURSEL, 0, 0));
        if (sel >= 0 && sel < static_cast<int>(m_endpoints.size())) {
            m_selectedDeviceId = m_endpoints[sel].id;
            notifyChange();
        }
    } else if (id == IDC_AP_COMBO_PRESET && code == CBN_SELCHANGE) {
        int sel = static_cast<int>(SendMessageW(m_comboPreset, CB_GETCURSEL, 0, 0));
        if (sel >= 0 && sel <= static_cast<int>(audio::EqualizerPreset::Custom)) {
            auto preset = static_cast<audio::EqualizerPreset>(sel);
            if (preset != audio::EqualizerPreset::Custom) {
                audio::Equalizer::applyPreset(m_eqState, preset);
                setEqualizerState(m_eqState);
                notifyChange();
            }
        }
    } else if (id == IDC_AP_COMBO_TRACKS && code == CBN_SELCHANGE) {
        int sel = static_cast<int>(SendMessageW(m_comboTracks, CB_GETCURSEL, 0, 0));
        if (sel == 0) {
            m_engine.selectAudioTrack(0); // none
        } else {
            // Find audio track id corresponding to this selection
            int cur = 0;
            for (const auto& track : m_engine.getMediaInfo().tracks) {
                if (track.type == "audio") {
                    cur++;
                    if (cur == sel) {
                        m_engine.selectAudioTrack(track.id);
                        break;
                    }
                }
            }
        }
    } else if (id == IDC_AP_CHK_EXCLUSIVE) {
        m_exclusiveMode = (SendMessageW(m_chkExclusive, BM_GETCHECK, 0, 0) == BST_CHECKED);
        notifyChange();
    } else if (id == IDC_AP_CHK_NORMALIZE) {
        m_eqState.normalize = (SendMessageW(m_chkNormalize, BM_GETCHECK, 0, 0) == BST_CHECKED);
        notifyChange();
    } else if (id == IDC_AP_CHK_DOWNMIX) {
        m_eqState.stereoDownmix = (SendMessageW(m_chkDownmix, BM_GETCHECK, 0, 0) == BST_CHECKED);
        notifyChange();
    } else if (id == IDC_AP_CHK_EQ_ENABLE) {
        m_eqState.enabled = (SendMessageW(m_chkEqEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
        notifyChange();
    } else if (id == IDC_AP_BTN_RESET_EQ) {
        m_eqState.reset();
        setEqualizerState(m_eqState);
        notifyChange();
    }
}

} // namespace rx::ui
