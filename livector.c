#ifndef UNICODE
#define UNICODE
#endif 

#include <stdio.h>
#include <windows.h>
#include <math.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
    if (FAILED(hres)) { goto Exit; }

void SafeRelease(void **ppObj)
{
    if (*ppObj)
    {
        IUnknown *pUnk = *ppObj;
        pUnk->lpVtbl->Release(pUnk);
        *ppObj = NULL;
    }
}

#include <initguid.h>

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4c32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioCaptureClient, 0xC8ADBD64, 0xE71E, 0x48a0, 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17);

#define TIMER_ID 1
#define MAX_POINTS 4800
#define COLOR_GRADATIONS 256
#define UPDATE_FREQUENCY 120

#define AUDIO_BITDEPTH 32

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void DrawContent(HDC hdc, RECT* pRect);
void AddPoint();

CRITICAL_SECTION pointsLock;
BOOL bContinueCapture = TRUE;
BOOL bHasSignaledExit = FALSE;

POINT points[MAX_POINTS];
unsigned int pointCount = 0;

int windowWidth = 400;
int windowHeight = 400;
float scaling_factor;

void calculate_scaling() {
    scaling_factor = powf(2, AUDIO_BITDEPTH) / min(windowHeight, windowWidth) / 2.0f;
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

    hr = CoInitialize(NULL);
    EXIT_ON_ERROR(hr)

    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, &IID_IMMDeviceEnumerator,
        (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(
        pEnumerator, eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

    hr = pDevice->lpVtbl->Activate(
        pDevice, &IID_IAudioClient, CLSCTX_ALL,
        NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->GetMixFormat(pAudioClient, &pwfx);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->Initialize(
        pAudioClient,
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000,
        0,
        pwfx,
        NULL);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->GetService(
        pAudioClient,
        &IID_IAudioCaptureClient,
        (void**)&pCaptureClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->Start(pAudioClient);
    EXIT_ON_ERROR(hr)

    while (TRUE) {
        EnterCriticalSection(&pointsLock);
        if (!bContinueCapture) {
            LeaveCriticalSection(&pointsLock);
            DeleteCriticalSection(&pointsLock);
            break;
        }
        LeaveCriticalSection(&pointsLock);
        Sleep(10);

        hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
        EXIT_ON_ERROR(hr)

        while (packetLength != 0) {
            hr = pCaptureClient->lpVtbl->GetBuffer(
                pCaptureClient,
                &pData,
                &numFramesAvailable,
                &flags, NULL, NULL);
            EXIT_ON_ERROR(hr)
            
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                if (pwfx->nChannels >= 2 && pwfx->wBitsPerSample == AUDIO_BITDEPTH) {   // Assuming 16-bit audio
                    FLOAT* samples = (FLOAT*)pData;
                    for (UINT32 i = 0; i < numFramesAvailable; i += 1) {
                        FLOAT sampleLeft = samples[i*2];
                        FLOAT sampleRight = samples[i*2+1];
                        int x = windowWidth  / 2 + (int)((sampleLeft)  / scaling_factor * pow(2, 32));  // Left channel for X
                        int y = windowHeight / 2 + (int)((-sampleRight) / scaling_factor * pow(2, 32));  // Right channel for Y
                        AddPoint(x, y);
                    }
                }
            }

            hr = pCaptureClient->lpVtbl->ReleaseBuffer(pCaptureClient, numFramesAvailable);
            EXIT_ON_ERROR(hr)

            hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
            EXIT_ON_ERROR(hr)
        }
    }

    hr = pAudioClient->lpVtbl->Stop(pAudioClient);
    EXIT_ON_ERROR(hr)

Exit:
    CoTaskMemFree(pwfx);
    SafeRelease((void**)&pEnumerator);
    SafeRelease((void**)&pDevice);
    SafeRelease((void**)&pAudioClient);
    SafeRelease((void**)&pCaptureClient);
    CoUninitialize();

    return 0;
}

HANDLE StartAudioCapture() {
    InitializeCriticalSection(&pointsLock);
    return CreateThread(NULL, 0, CaptureAudioThread, NULL, 0, NULL);
}

void StopAudioCapture() {
    if (!bHasSignaledExit) {
        EnterCriticalSection(&pointsLock);
        bContinueCapture = FALSE;
        LeaveCriticalSection(&pointsLock);
        bHasSignaledExit = TRUE;
    }
}

void AddPoint(int x, int y) {
    EnterCriticalSection(&pointsLock);
    if (pointCount < MAX_POINTS) {
        points[pointCount].x = x;
        points[pointCount].y = y;
        pointCount++;
    } else {
        memmove(points, points + 1, (MAX_POINTS - 1) * sizeof(POINT));
        points[MAX_POINTS - 1].x = x;
        points[MAX_POINTS - 1].y = y;
    }
    LeaveCriticalSection(&pointsLock);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

    calculate_scaling();
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    const wchar_t CLASS_NAME[]  = L"MainClass";
    WNDCLASS wc;
    MSG msg;
    HWND hwndMain;

    wc.style = CS_HREDRAW | CS_VREDRAW;

    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = 0;

    wc.hInstance      = hInstance;
    wc.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);

    wc.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);

    wc.lpszMenuName   = 0;
    wc.lpszClassName  = CLASS_NAME;
    wc.lpfnWndProc    = MainWindowProc;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Window class is not registered", L"Error", MB_ICONERROR);
        return 0;
    }

    hwndMain = CreateWindowEx(
        0,                     // Optional window styles.
        CLASS_NAME,            // Window class
        L"Livector Window",    // Window text
        WS_OVERLAPPEDWINDOW,   // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight, 


        (HWND) NULL,   // Parent window    
        (HMENU) NULL,  // Menu
        hInstance,     // Instance handle
        NULL           // Additional application data
        );
    
    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();

    return 0;
}

void DrawContent(HDC hdc, RECT* pRect) {
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));

    int lines = 0;
    unsigned int start = 0;
    EnterCriticalSection(&pointsLock);
    if (pointCount > 1) {
        for (unsigned int i = pointCount; i > 1; --i) {
            float brightness = powf((float)i / (float)pointCount, 2.f);
            if (brightness < 1.f / 255.f) break;

            float dx = (float)(points[i].x) - (float)(points[i-1].x);
            float dy = (float)(points[i].y) - (float)(points[i-1].y);
            float e_factor = powf(2.71828f, sqrtf((dx * dx) + (dy * dy)) / 25.f);
            brightness *= 1.f - e_factor * (1.f / (100.f + e_factor));

            SetDCPenColor(hdc, RGB(255*brightness, 20*brightness, 20*brightness));
            Polyline(hdc, points + i - 2, 2);
            lines++;
        }
    }
    LeaveCriticalSection(&pointsLock);

    SelectObject(hdc, oldPen);

    SetTextColor(hdc, RGB(200, 200, 200));
    SetBkMode(hdc, TRANSPARENT);

    wchar_t infoText[100];
    if (pointCount > 0) {
        swprintf(infoText, 100, L"Points: %d, Last: (%d, %d), Lines: %d", 
                 pointCount, points[pointCount-1].x, points[pointCount-1].y, lines);
    } else {
        wcscpy_s(infoText, 100, L"Points: 0");
    }

    TextOutW(hdc, 10, 10, infoText, (int)wcslen(infoText));
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            InitializeCriticalSection(&pointsLock);
            CreateThread(NULL, 0, CaptureAudioThread, NULL, 0, NULL);
            SetTimer(hwnd, TIMER_ID, (UINT)(1000 / UPDATE_FREQUENCY), NULL);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            FillRect(memDC, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));

            DrawContent(memDC, &ps.rcPaint);

            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                   ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
                   memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            windowWidth = LOWORD(lParam);
            windowHeight = HIWORD(lParam);
            calculate_scaling();
            return 0;
        
        case WM_TIMER:
            if (wParam == TIMER_ID) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            StopAudioCapture();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
