#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

typedef int (CALLBACK *PFN_EnumFontFamiliesExA)(
    HDC hdc,
    LPLOGFONTA lpLogfont,
    FONTENUMPROCA lpProc,
    LPARAM lParam,
    DWORD dwFlags
);

static PFN_EnumFontFamiliesExA g_origEnumFontFamiliesExA = NULL;
static volatile LONG g_hookInstalled = 0;

typedef struct {
    FONTENUMPROCA origProc;
    LPARAM origParam;
} EnumCtx;

static int CALLBACK MyEnumFontProcA(
    const LOGFONTA *lpelfe,
    const TEXTMETRICA *lpntme,
    DWORD FontType,
    LPARAM lParam
) {
    EnumCtx *ctx = (EnumCtx *)lParam;
    if (!ctx || !ctx->origProc) return 0;

    if (lpelfe && lpelfe->lfCharSet == SHIFTJIS_CHARSET) {
        ENUMLOGFONTEXA copy;
        memcpy(&copy, lpelfe, sizeof(ENUMLOGFONTEXA));
        /* Shift-JIS bytes for "日本語\0" */
        static const unsigned char sjis_jp[7] = { 0x93, 0xFA, 0x96, 0x7B, 0x8C, 0xEA, 0x00 };
        memcpy(copy.elfScript, sjis_jp, sizeof(sjis_jp));
        return ctx->origProc((const LOGFONTA *)&copy, lpntme, FontType, ctx->origParam);
    }

    return ctx->origProc(lpelfe, lpntme, FontType, ctx->origParam);
}

static int WINAPI Hook_EnumFontFamiliesExA(
    HDC hdc,
    LPLOGFONTA lpLogfont,
    FONTENUMPROCA lpProc,
    LPARAM lParam,
    DWORD dwFlags
) {
    if (!g_origEnumFontFamiliesExA) return 0;
    if (!lpProc) {
        return g_origEnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }
    EnumCtx ctx;
    ctx.origProc = lpProc;
    ctx.origParam = lParam;
    return g_origEnumFontFamiliesExA(hdc, lpLogfont, MyEnumFontProcA, (LPARAM)&ctx, dwFlags);
}

static void InstallFontHook(void) {
    if (InterlockedCompareExchange(&g_hookInstalled, 1, 0) != 0) return;

    HMODULE hGdi = GetModuleHandleA("gdi32.dll");
    if (!hGdi) {
        g_hookInstalled = 0;
        return;
    }

    uint8_t *p = (uint8_t *)GetProcAddress(hGdi, "EnumFontFamiliesExA");
    if (!p) return;

    /* Follow FF 25 [mem32] jump stub if present on newer Windows builds */
    if (p[0] == 0xFF && p[1] == 0x25) {
        uint8_t **ppTarget = *(uint8_t ***)(p + 2);
        if (ppTarget && *ppTarget) {
            p = *ppTarget;
        }
    }

    /* Standard Microsoft 32-bit hotpatch prologue: 5 padding bytes (CC/90) + 8B FF */
    if (p[0] == 0x8B && p[1] == 0xFF) {
        int padOk = 1;
        for (int i = -5; i < 0; i++) {
            if (p[i] != 0xCC && p[i] != 0x90) {
                padOk = 0;
                break;
            }
        }
        if (padOk) {
            DWORD oldProt = 0;
            if (VirtualProtect(p - 5, 7, PAGE_EXECUTE_READWRITE, &oldProt)) {
                g_origEnumFontFamiliesExA = (PFN_EnumFontFamiliesExA)(p + 2);
                int32_t rel = (int32_t)((uintptr_t)Hook_EnumFontFamiliesExA - (uintptr_t)p);
                p[-5] = 0xE9;
                memcpy(p - 4, &rel, 4);
                /* Atomic 2-byte short jump back 7 bytes: EB F9 */
                *(volatile uint16_t *)p = 0xF9EB;
                FlushInstructionCache(GetCurrentProcess(), p - 5, 7);
                VirtualProtect(p - 5, 7, oldProt, &oldProt);
            }
        }
    }
}

/* Real dsound.dll lazy loader (never called inside DllMain) */
static HMODULE g_realDsound = NULL;
static INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK InitRealDsoundProc(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context) {
    (void)InitOnce; (void)Parameter; (void)Context;
    InstallFontHook();
    WCHAR sysDir[MAX_PATH];
    UINT len = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (len > 0 && len + 12 < MAX_PATH) {
        wcscat(sysDir, L"\\dsound.dll");
        g_realDsound = LoadLibraryW(sysDir);
    }
    return TRUE;
}

static FARPROC GetRealProc(const char *name) {
    InitOnceExecuteOnce(&g_initOnce, InitRealDsoundProc, NULL, NULL);
    if (!g_realDsound) return NULL;
    return GetProcAddress(g_realDsound, name);
}

/* Forwarded DirectSound exports (all 12 standard Win32 dsound.dll exports) */
HRESULT WINAPI Proxy_DirectSoundCreate(LPCVOID a, LPVOID *b, LPVOID c) {
    typedef HRESULT (WINAPI *Fn)(LPCVOID, LPVOID *, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundCreate");
    return fn ? fn(a, b, c) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundEnumerateA(LPVOID a, LPVOID b) {
    typedef HRESULT (WINAPI *Fn)(LPVOID, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundEnumerateA");
    return fn ? fn(a, b) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundEnumerateW(LPVOID a, LPVOID b) {
    typedef HRESULT (WINAPI *Fn)(LPVOID, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundEnumerateW");
    return fn ? fn(a, b) : E_FAIL;
}

HRESULT WINAPI Proxy_DllCanUnloadNow(void) {
    typedef HRESULT (WINAPI *Fn)(void);
    Fn fn = (Fn)GetRealProc("DllCanUnloadNow");
    return fn ? fn() : S_FALSE;
}

HRESULT WINAPI Proxy_DllGetClassObject(LPCVOID a, LPCVOID b, LPVOID *c) {
    typedef HRESULT (WINAPI *Fn)(LPCVOID, LPCVOID, LPVOID *);
    Fn fn = (Fn)GetRealProc("DllGetClassObject");
    return fn ? fn(a, b, c) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundCaptureCreate(LPCVOID a, LPVOID *b, LPVOID c) {
    typedef HRESULT (WINAPI *Fn)(LPCVOID, LPVOID *, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundCaptureCreate");
    return fn ? fn(a, b, c) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundCaptureEnumerateA(LPVOID a, LPVOID b) {
    typedef HRESULT (WINAPI *Fn)(LPVOID, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundCaptureEnumerateA");
    return fn ? fn(a, b) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundCaptureEnumerateW(LPVOID a, LPVOID b) {
    typedef HRESULT (WINAPI *Fn)(LPVOID, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundCaptureEnumerateW");
    return fn ? fn(a, b) : E_FAIL;
}

HRESULT WINAPI Proxy_GetDeviceID(LPCVOID a, LPVOID b) {
    typedef HRESULT (WINAPI *Fn)(LPCVOID, LPVOID);
    Fn fn = (Fn)GetRealProc("GetDeviceID");
    return fn ? fn(a, b) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundFullDuplexCreate(
    LPCVOID a, LPCVOID b, LPCVOID c, LPCVOID d, HWND e, DWORD f,
    LPVOID *g, LPVOID *h, LPVOID *i, LPVOID j
) {
    typedef HRESULT (WINAPI *Fn)(LPCVOID, LPCVOID, LPCVOID, LPCVOID, HWND, DWORD, LPVOID *, LPVOID *, LPVOID *, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundFullDuplexCreate");
    return fn ? fn(a, b, c, d, e, f, g, h, i, j) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundCreate8(LPCVOID a, LPVOID *b, LPVOID c) {
    typedef HRESULT (WINAPI *Fn)(LPCVOID, LPVOID *, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundCreate8");
    return fn ? fn(a, b, c) : E_FAIL;
}

HRESULT WINAPI Proxy_DirectSoundCaptureCreate8(LPCVOID a, LPVOID *b, LPVOID c) {
    typedef HRESULT (WINAPI *Fn)(LPCVOID, LPVOID *, LPVOID);
    Fn fn = (Fn)GetRealProc("DirectSoundCaptureCreate8");
    return fn ? fn(a, b, c) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        InstallFontHook();
    }
    return TRUE;
}
