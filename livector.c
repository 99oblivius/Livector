#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <wchar.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <math.h>

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
    if (FAILED(hres)) { goto Exit; }

#define SAFE_RELEASE(punk)  \
    if ((punk) != NULL)  \
    { (punk)->lpVtbl->Release(punk); (punk) = NULL; }

// Audio settings
#define SAMPLE_RATE 48000
#define AUDIO_BITDEPTH 32
#define BUFFER_DURATION_MS 100  // 100ms buffer

// Display settings
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define UPDATE_FREQUENCY 60  // 60 Hz refresh rate
#define COLOR_GRADATIONS 256

// Circular buffer
#define BUFFER_SIZE (SAMPLE_RATE * BUFFER_DURATION_MS / 1000)

typedef struct {
    POINT buffer[BUFFER_SIZE];
    int head;
    int count;
} CircularBuffer;

// Global variables
HWND g_hwnd;
CircularBuffer g_audioBuffer = {0};
CRITICAL_SECTION g_bufferLock;
BOOL g_continueCapture = TRUE;
int g_windowWidth = WINDOW_WIDTH;
int g_windowHeight = WINDOW_HEIGHT;
double g_scalingFactor;
int g_xOrigin, g_yOrigin;
double g_xCursor, g_yCursor;
BOOL g_leftButtonDown = FALSE;
BOOL g_alignVertical = FALSE;
double g_brightnessExponent = 6.0;

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI CaptureAudioThread(LPVOID lpParam);
void DrawContent(HDC hdc, RECT* pRect);
void AddPointToBuffer(int x, int y);
void CalculateScaling();

// GUID definitions
#include <initguid.h>
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4c32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioCaptureClient, 0xC8ADBD64, 0xE71E, 0x48a0, 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AudioVisualizerClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(
        0, L"AudioVisualizerClass", L"Audio Visualizer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, g_windowWidth, g_windowHeight,
        NULL, NULL, hInstance, NULL
    );

    if (g_hwnd == NULL) {
        return 0;
    }

    ShowWindow(g_hwnd, nCmdShow);

    CalculateScaling();
    g_xOrigin = g_windowWidth / 2;
    g_yOrigin = g_windowHeight / 2;
    g_xCursor = g_windowWidth / 2;
    g_yCursor = g_windowHeight / 2;

    InitializeCriticalSection(&g_bufferLock);
    CreateThread(NULL, 0, CaptureAudioThread, NULL, 0, NULL);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteCriticalSection(&g_bufferLock);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 1000 / UPDATE_FREQUENCY, NULL);
            return 0;

        case WM_DESTROY:
            g_continueCapture = FALSE;
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawContent(hdc, &ps.rcPaint);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            g_windowWidth = LOWORD(lParam);
            g_windowHeight = HIWORD(lParam);
            CalculateScaling();
            return 0;

        case WM_TIMER:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            g_leftButtonDown = TRUE;
            g_xCursor = LOWORD(lParam);
            g_yCursor = HIWORD(lParam);
            return 0;

        case WM_LBUTTONUP:
            g_leftButtonDown = FALSE;
            return 0;

        case WM_MOUSEMOVE:
            if (g_leftButtonDown) {
                g_xCursor = LOWORD(lParam);
                g_yCursor = HIWORD(lParam);
            }
            return 0;

        case WM_RBUTTONDOWN:
            g_xCursor = g_windowWidth / 2;
            g_yCursor = g_windowHeight / 2;
            return 0;

        case WM_KEYDOWN:
            switch (wParam) {
                case 'R':
                    g_alignVertical = !g_alignVertical;
                    break;
                case VK_UP:
                    g_brightnessExponent = min(50.0, g_brightnessExponent + 0.125);
                    break;
                case VK_DOWN:
                    g_brightnessExponent = max(0.0, g_brightnessExponent - 0.125);
                    break;
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CalculateScaling() {
    g_scalingFactor = 0.2f / min(g_windowHeight, g_windowWidth);
}

void AddPointToBuffer(int x, int y) {
    EnterCriticalSection(&g_bufferLock);
    g_audioBuffer.buffer[g_audioBuffer.head].x = x;
    g_audioBuffer.buffer[g_audioBuffer.head].y = y;
    g_audioBuffer.head = (g_audioBuffer.head + 1) % BUFFER_SIZE;
    if (g_audioBuffer.count < BUFFER_SIZE) {
        g_audioBuffer.count++;
    }
    LeaveCriticalSection(&g_bufferLock);
}

void DrawContent(HDC hdc, RECT* pRect) {
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, pRect->right, pRect->bottom);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hOldBitmap = SelectObject(hdcMem, hBitmap);

    // Clear background
    HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdcMem, pRect, hBlackBrush);
    DeleteObject(hBlackBrush);

    // Update origin with smooth motion
    g_xOrigin = (int)((double)g_xOrigin * 0.95 + g_xCursor * 0.05);
    g_yOrigin = (int)((double)g_yOrigin * 0.95 + g_yCursor * 0.05);

    // Draw points
    EnterCriticalSection(&g_bufferLock);
    for (int i = 0; i < g_audioBuffer.count; i++) {
        int index = (g_audioBuffer.head - 1 - i + BUFFER_SIZE) % BUFFER_SIZE;
        POINT point = g_audioBuffer.buffer[index];
        double age = (double)i / g_audioBuffer.count;
        double brightness = pow(1 - age, g_brightnessExponent);
        COLORREF color = RGB(255 * brightness, 35 * brightness, 200 * brightness);
        SetPixel(hdcMem, point.x, point.y, color);
    }
    LeaveCriticalSection(&g_bufferLock);

    // Draw info text
    SetTextColor(hdcMem, RGB(200, 200, 200));
    SetBkMode(hdcMem, TRANSPARENT);
    wchar_t infoText[256];
    int charsWritten = swprintf_s(infoText, 256,
        L"Origin x:%d y:%d\n"
        L"Points: %d\n"
        L"Falloff: %.3lf\n"
        L"Aligned: %d",
        g_xOrigin, g_yOrigin, g_audioBuffer.count, g_brightnessExponent, g_alignVertical);
    RECT textRect = {10, 10, pRect->right - 10, pRect->bottom - 10};
    DrawText(hdcMem, infoText, charsWritten, &textRect, DT_NOCLIP);

    // Copy to screen
    BitBlt(hdc, 0, 0, pRect->right, pRect->bottom, hdcMem, 0, 0, SRCCOPY);

    // Clean up
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
}

DWORD WINAPI CaptureAudioThread(LPVOID lpParam) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    UINT32 packetLength = 0;
    BYTE *pData;
    UINT32 numFramesAvailable;
    DWORD flags;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    EXIT_ON_ERROR(hr)

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, 
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->GetMixFormat(pAudioClient, &pwfx);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->Initialize(pAudioClient, AUDCLNT_SHAREMODE_SHARED,
                                          AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, NULL);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->GetService(pAudioClient, &IID_IAudioCaptureClient, (void**)&pCaptureClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->Start(pAudioClient);
    EXIT_ON_ERROR(hr)

    while (g_continueCapture) {
        Sleep(10);  // Wait for 10ms

        hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
        EXIT_ON_ERROR(hr)

        while (packetLength != 0) {
            hr = pCaptureClient->lpVtbl->GetBuffer(pCaptureClient, &pData, &numFramesAvailable, &flags, NULL, NULL);
            EXIT_ON_ERROR(hr)

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                FLOAT *pFloat = (FLOAT*)pData;
                for (UINT32 i = 0; i < numFramesAvailable; i++) {
                    FLOAT left = pFloat[2*i];
                    FLOAT right = pFloat[2*i + 1];
                    int x = (int)(g_xOrigin + (left - (g_alignVertical ? right : 0)) / g_scalingFactor);
                    int y = (int)(g_yOrigin + (-right - (g_alignVertical ? left : 0)) / g_scalingFactor);
                    AddPointToBuffer(x, y);
                }
            }

            hr = pCaptureClient->lpVtbl->ReleaseBuffer(pCaptureClient, numFramesAvailable);
            EXIT_ON_ERROR(hr)

            hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
            EXIT_ON_ERROR(hr)
        }
    }

Exit:
    if (pAudioClient) {
        pAudioClient->lpVtbl->Stop(pAudioClient);
    }
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(pAudioClient)
    SAFE_RELEASE(pCaptureClient)
    CoUninitialize();

    return 0;
}

