#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cmath>
#include <cstdint>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AML12", __VA_ARGS__)

// Posisi tombol melayang "AML" & status menu
static float g_BtnX = 120.0f;
static float g_BtnY = 120.0f;
static bool g_MenuOpen = false;
static bool g_Dragging = false;
static bool g_HasMoved = false;
static float g_StartX = 0.0f, g_StartY = 0.0f;
static float g_OffsetX = 0.0f, g_OffsetY = 0.0f;

// Tipe fungsi internal GTA SA (ARMv7)
typedef void (*DrawRect_t)(float* rect, uint32_t* color);
typedef void (*SetScale_t)(float w, float h);
typedef void (*SetColor_t)(uint32_t* color);
typedef void (*PrintString_t)(float x, float y, uint16_t* text);
typedef void (*RequestModel_t)(int id, int flags);
typedef void (*LoadAllModels_t)(bool onlyPriority);
typedef void* (*FindPlayerPed_t)(int playerId);
typedef void* (*CreateCar_t)(int modelId, float x, float y, float z, bool createdBy);

static DrawRect_t CSprite2d_DrawRect = nullptr;
static SetScale_t CFont_SetScale = nullptr;
static SetColor_t CFont_SetColor = nullptr;
static PrintString_t CFont_PrintString = nullptr;
static RequestModel_t CStreaming_RequestModel = nullptr;
static LoadAllModels_t CStreaming_LoadAllRequestedModels = nullptr;
static FindPlayerPed_t FindPlayerPed = nullptr;

// Konversi teks ASCII ke UTF-16 untuk CFont GTA SA
void AsciiToGxt(const char* src, uint16_t* dst, int maxLen) {
    int i = 0;
    while (src[i] && i < maxLen - 1) {
        dst[i] = (uint16_t)src[i];
        i++;
    }
    dst[i] = 0;
}

// Fungsi memanggil mobil Turismo (ID 451)
void SpawnTurismo() {
    if (!CStreaming_RequestModel || !CStreaming_LoadAllRequestedModels || !FindPlayerPed) return;
    
    const int TURISMO_ID = 451;
    CStreaming_RequestModel(TURISMO_ID, 0x16);
    CStreaming_LoadAllRequestedModels(false);
    LOGI("Model Turismo (451) dimuat ke memori.");
}

// Menggambar UI tombol "AML" dan menu pop-up
void RenderMenu() {
    if (!CSprite2d_DrawRect || !CFont_PrintString) return;

    uint16_t textBuf[64];
    uint32_t bgDark = 0xCC111111;
    uint32_t btnAccent = 0xEE0077FF;
    uint32_t textWhite = 0xFFFFFFFF;

    // 1. Kotak & tulisan melayang "AML"
    float btnRect[4] = { g_BtnX, g_BtnY + 45.0f, g_BtnX + 110.0f, g_BtnY };
    CSprite2d_DrawRect(btnRect, &bgDark);

    if (CFont_SetScale) CFont_SetScale(0.7f, 1.4f);
    if (CFont_SetColor) CFont_SetColor(&textWhite);
    AsciiToGxt("AML", textBuf, 64);
    CFont_PrintString(g_BtnX + 18.0f, g_BtnY + 8.0f, textBuf);

    // 2. Menu Pop-up saat tombol "AML" diketuk
    if (g_MenuOpen) {
        float panelRect[4] = { g_BtnX, g_BtnY + 130.0f, g_BtnX + 260.0f, g_BtnY + 55.0f };
        CSprite2d_DrawRect(panelRect, &btnAccent);

        if (CFont_SetScale) CFont_SetScale(0.6f, 1.2f);
        AsciiToGxt("Spawn Turismo", textBuf, 64);
        CFont_PrintString(g_BtnX + 15.0f, g_BtnY + 75.0f, textBuf);
    }
}

// Memproses sentuhan layar (0=Down, 1=Up, 2=Move)
void HandleTouch(int action, float x, float y) {
    bool insideBtn = (x >= g_BtnX && x <= g_BtnX + 110.0f && y >= g_BtnY && y <= g_BtnY + 45.0f);

    if (action == 0) { // Jari menyentuh layar
        if (insideBtn) {
            g_Dragging = true;
            g_HasMoved = false;
            g_StartX = x;
            g_StartY = y;
            g_OffsetX = x - g_BtnX;
            g_OffsetY = y - g_BtnY;
        } else if (g_MenuOpen) {
            bool insideMenu = (x >= g_BtnX && x <= g_BtnX + 260.0f && y >= g_BtnY + 55.0f && y <= g_BtnY + 130.0f);
            if (insideMenu) {
                SpawnTurismo();
            }
        }
    } else if (action == 2 && g_Dragging) { // Jari digeser
        if (fabsf(x - g_StartX) > 12.0f || fabsf(y - g_StartY) > 12.0f) {
            g_HasMoved = true;
        }
        if (g_HasMoved) {
            g_BtnX = x - g_OffsetX;
            g_BtnY = y - g_OffsetY;
        }
    } else if (action == 1 && g_Dragging) { // Jari diangkat
        if (!g_HasMoved) {
            g_MenuOpen = !g_MenuOpen;
        }
        g_Dragging = false;
    }
}

extern "C" void OnModLoad() {
    void* handle = dlopen("libGTASA.so", RTLD_LAZY);
    if (!handle) return;

    CSprite2d_DrawRect = (DrawRect_t)dlsym(handle, "_ZN9CSprite2d8DrawRectERK5CRectRK5CRGBA");
    CFont_SetScale = (SetScale_t)dlsym(handle, "_ZN5CFont8SetScaleEff");
    CFont_SetColor = (SetColor_t)dlsym(handle, "_ZN5CFont8SetColorE5CRGBA");
    CFont_PrintString = (PrintString_t)dlsym(handle, "_ZN5CFont11PrintStringEffPt");
    CStreaming_RequestModel = (RequestModel_t)dlsym(handle, "_ZN10CStreaming12RequestModelEii");
    CStreaming_LoadAllRequestedModels = (LoadAllModels_t)dlsym(handle, "_ZN10CStreaming22LoadAllRequestedModelsEb");
    FindPlayerPed = (FindPlayerPed_t)dlsym(handle, "_Z13FindPlayerPedi");

    LOGI("Symbol GTA SA berhasil dimuat.");
}
