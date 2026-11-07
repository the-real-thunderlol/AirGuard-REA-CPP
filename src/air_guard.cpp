#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <fstream>
#include <string>
#include <vector>

namespace {

enum : int {
    ID_RESET = 100,
    ID_START,
    ID_RES_640,
    ID_RES_800,
    ID_RES_1024,
    ID_SCENE_LOW,
    ID_SCENE_MEDIUM,
    ID_SCENE_HIGH,
    ID_SHADOWS,
    ID_PARTICLES,
    ID_SPECULAR,
    ID_SOUND,
    ID_SOUND_3D,
    ID_SOUND_HW,
    ID_MUSIC,
    ID_INVERT,
    ID_JOYSTICK,
    ID_DRAW_DISTANCE,
    ID_MOUSE_X,
    ID_MOUSE_Y
};

HINSTANCE instanceHandle;
HWND mainWindow;
HFONT uiFont;
HWND resolution[3];
HWND sceneDetail[3];
HWND shadows;
HWND particleEffects;
HWND specularEffects;
HWND soundEnabled;
HWND sound3DEnabled;
HWND hardwareSound;
HWND musicEnabled;
HWND invertY;
HWND joystickEnabled;
HWND drawDistance;
HWND mouseX;
HWND mouseY;

int px(int twips) { return twips / 15; }

std::string executableDirectory() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *slash = 0;
    return path;
}

HWND control(DWORD exStyle, const char* klass, const char* text, DWORD style,
             int x, int y, int w, int h, HWND parent, int id) {
    HWND result = CreateWindowExA(exStyle, klass, text, WS_CHILD | WS_VISIBLE | style,
                                  x, y, w, h, parent,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  instanceHandle, nullptr);
    if (uiFont) SendMessageA(result, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
    return result;
}

HWND label(const char* text, int x, int y, int w, int h, HWND parent, DWORD style = SS_LEFT) {
    return control(0, "STATIC", text, style, x, y, w, h, parent, -1);
}

HWND frame(const char* text, int x, int y, int w, int h, HWND parent) {
    return control(0, "BUTTON", text, BS_GROUPBOX, x, y, w, h, parent, -1);
}

HWND option(const char* text, int x, int y, int w, int h, HWND parent, int id) {
    return control(0, "BUTTON", text, BS_AUTORADIOBUTTON | WS_TABSTOP,
                   x, y, w, h, parent, id);
}

HWND check(const char* text, int x, int y, int w, int h, HWND parent, int id) {
    return control(0, "BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP,
                   x, y, w, h, parent, id);
}

bool checked(HWND window) {
    return SendMessageA(window, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void setChecked(HWND window, bool value) {
    SendMessageA(window, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
}

int selected(HWND* group, int count) {
    for (int i = 0; i < count; ++i) if (checked(group[i])) return i;
    return 0;
}

void select(HWND* group, int count, int value) {
    for (int i = 0; i < count; ++i) setChecked(group[i], i == value);
}

void loadConfiguration(const std::string& path) {
    std::ifstream input(path);
    std::string key;
    int value;
    while (input >> key >> value) {
        if (key == "Resolution") select(resolution, 3, value);
        else if (key == "DrawDistance") SendMessageA(drawDistance, TBM_SETPOS, TRUE, value / 1000);
        else if (key == "SceneDetail") select(sceneDetail, 3, value);
        else if (key == "Shadows") setChecked(shadows, value != 0);
        else if (key == "ParticleEffects") setChecked(particleEffects, value != 0);
        else if (key == "SpecularEffects") setChecked(specularEffects, value != 0);
        else if (key == "SoundEnabled") setChecked(soundEnabled, value != 0);
        else if (key == "Sound3DEnabled") setChecked(sound3DEnabled, value != 0);
        else if (key == "HardwareSound") setChecked(hardwareSound, value != 0);
        else if (key == "MusicEnabled") setChecked(musicEnabled, value != 0);
        else if (key == "MouseX") SendMessageA(mouseX, TBM_SETPOS, TRUE, value);
        else if (key == "MouseY") SendMessageA(mouseY, TBM_SETPOS, TRUE, value);
        else if (key == "Invert") setChecked(invertY, value != 0);
        else if (key == "JoyEnabled") setChecked(joystickEnabled, value != 0);
    }
}

void saveConfiguration(const std::string& path) {
    std::ofstream output(path, std::ios::trunc);
    output << "Resolution " << selected(resolution, 3) << "\n";
    output << "DrawDistance " << SendMessageA(drawDistance, TBM_GETPOS, 0, 0) * 1000 << "\n";
    output << "SceneDetail " << selected(sceneDetail, 3) << "\n";
    output << "Shadows " << (checked(shadows) ? 1 : 0) << "\n";
    output << "ParticleEffects " << (checked(particleEffects) ? 1 : 0) << "\n";
    output << "SpecularEffects " << (checked(specularEffects) ? 1 : 0) << "\n";
    output << "SoundEnabled " << (checked(soundEnabled) ? 1 : 0) << "\n";
    output << "Sound3DEnabled " << (checked(sound3DEnabled) ? 1 : 0) << "\n";
    output << "HardwareSound " << (checked(hardwareSound) ? 1 : 0) << "\n";
    output << "MusicEnabled " << (checked(musicEnabled) ? 1 : 0) << "\n";
    output << "MouseX " << SendMessageA(mouseX, TBM_GETPOS, 0, 0) << "\n";
    output << "MouseY " << SendMessageA(mouseY, TBM_GETPOS, 0, 0) << "\n";
    output << "Invert " << (checked(invertY) ? 1 : 0) << "\n";
    output << "JoyEnabled " << (checked(joystickEnabled) ? 1 : 0) << "\n";
    output << "Language 0\n";
}

void createInterface(HWND window) {
    uiFont = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, "MS Sans Serif");

    HWND graphics = frame("Graphics", px(120), px(0), px(4575), px(4215), window);
    HWND resolutionFrame = frame("", px(120), px(360), px(3735), px(855), graphics);
    label("Resolution", px(120), px(0), px(1095), px(255), graphics);
    resolution[0] = option("640x480", px(240), px(240), px(975), px(375), resolutionFrame, ID_RES_640);
    resolution[1] = option("800x600", px(1440), px(240), px(1095), px(375), resolutionFrame, ID_RES_800);
    resolution[2] = option("1024x768", px(2640), px(240), px(1335), px(375), resolutionFrame, ID_RES_1024);

    HWND detailFrame = frame("", px(120), px(1080), px(3975), px(615), graphics);
    label("Scene detail", px(120), px(0), px(1575), px(255), detailFrame);
    sceneDetail[0] = option("Low", px(240), px(240), px(975), px(375), detailFrame, ID_SCENE_LOW);
    sceneDetail[1] = option("Medium", px(1440), px(240), px(1095), px(375), detailFrame, ID_SCENE_MEDIUM);
    sceneDetail[2] = option("High", px(2640), px(240), px(1335), px(375), detailFrame, ID_SCENE_HIGH);

    label("Field of view", px(240), px(1800), px(2295), px(255), graphics);
    label("Small", px(360), px(2520), px(615), px(255), graphics);
    label("Big", px(2760), px(2520), px(495), px(255), graphics);
    drawDistance = control(0, TRACKBAR_CLASSA, "", TBS_AUTOTICKS | WS_TABSTOP,
                           px(360), px(2160), px(2895), px(255), graphics, ID_DRAW_DISTANCE);
    SendMessageA(drawDistance, TBM_SETRANGE, TRUE, MAKELPARAM(2, 13));
    SendMessageA(drawDistance, TBM_SETPOS, TRUE, 12);

    label("Others", px(240), px(2880), px(1335), px(255), graphics);
    shadows = check("Shadows", px(360), px(3120), px(1695), px(255), graphics, ID_SHADOWS);
    particleEffects = check("Special effects", px(360), px(3480), px(1575), px(255), graphics, ID_PARTICLES);
    specularEffects = check("Reflections", px(360), px(3840), px(1575), px(255), graphics, ID_SPECULAR);
    HWND gray1 = label("", px(2160), px(3120), px(2295), px(255), graphics);
    HWND gray2 = label("cool effect if enabled", px(2160), px(3480), px(1935), px(255), graphics);
    SendMessageA(gray1, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(""));
    EnableWindow(gray1, FALSE);
    EnableWindow(gray2, FALSE);

    HWND controlsFrame = frame("Controls", px(4800), px(0), px(3135), px(2895), window);
    label("Sensitivity X axis", px(240), px(360), px(2295), px(255), controlsFrame);
    mouseX = control(0, TRACKBAR_CLASSA, "", TBS_AUTOTICKS | WS_TABSTOP,
                     px(480), px(600), px(2055), px(255), controlsFrame, ID_MOUSE_X);
    SendMessageA(mouseX, TBM_SETRANGE, TRUE, MAKELPARAM(1, 10));
    SendMessageA(mouseX, TBM_SETPOS, TRUE, 7);
    label("Small", px(600), px(960), px(615), px(255), controlsFrame);
    label("High", px(2040), px(960), px(495), px(255), controlsFrame);

    label("Sensitivity Y axis", px(240), px(1200), px(1935), px(255), controlsFrame);
    mouseY = control(0, TRACKBAR_CLASSA, "", TBS_AUTOTICKS | WS_TABSTOP,
                     px(480), px(1440), px(2055), px(255), controlsFrame, ID_MOUSE_Y);
    SendMessageA(mouseY, TBM_SETRANGE, TRUE, MAKELPARAM(1, 10));
    SendMessageA(mouseY, TBM_SETPOS, TRUE, 7);
    label("Small", px(600), px(1800), px(615), px(255), controlsFrame);
    label("High", px(2040), px(1800), px(495), px(255), controlsFrame);
    invertY = check("Invert Y axis", px(240), px(2040), px(2055), px(255), controlsFrame, ID_INVERT);
    joystickEnabled = check("Enabled joystick", px(240), px(2400), px(2535), px(255), controlsFrame, ID_JOYSTICK);

    HWND soundsFrame = frame("Sounds", px(4800), px(2880), px(3135), px(1815), window);
    soundEnabled = check("Enabled sounds", px(240), px(360), px(2055), px(255), soundsFrame, ID_SOUND);
    musicEnabled = check("Enabled Music", px(240), px(720), px(2055), px(255), soundsFrame, ID_MUSIC);
    sound3DEnabled = check("Enabled 3D sound ", px(240), px(1080), px(2055), px(255), soundsFrame, ID_SOUND_3D);
    hardwareSound = check("Enabled hardware sound", px(240), px(1440), px(2535), px(255), soundsFrame, ID_SOUND_HW);

    control(0, "BUTTON", "Start Game", BS_DEFPUSHBUTTON | WS_TABSTOP,
            px(1440), px(4320), px(3255), px(375), window, ID_START);
    control(0, "BUTTON", "Reset", BS_PUSHBUTTON | WS_TABSTOP,
            px(120), px(4320), px(1215), px(375), window, ID_RESET);

    loadConfiguration(executableDirectory() + "\\config.ini");
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        createInterface(window);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_RESET && HIWORD(wParam) == BN_CLICKED) {
            loadConfiguration(executableDirectory() + "\\default.ini");
            return 0;
        }
        if (LOWORD(wParam) == ID_START && HIWORD(wParam) == BN_CLICKED) {
            const std::string directory = executableDirectory();
            saveConfiguration(directory + "\\config.ini");
            ShellExecuteA(window, "open", (directory + "\\pa.exe").c_str(), nullptr,
                          directory.c_str(), SW_SHOWNORMAL);
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (uiFont) DeleteObject(uiFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    instanceHandle = instance;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXA klass{};
    klass.cbSize = sizeof(klass);
    klass.style = CS_DBLCLKS;
    klass.lpfnWndProc = windowProcedure;
    klass.hInstance = instance;
    klass.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(1));
    klass.hIconSm = static_cast<HICON>(LoadImageA(instance, MAKEINTRESOURCEA(1), IMAGE_ICON,
                                                   16, 16, LR_DEFAULTCOLOR));
    klass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    klass.lpszClassName = "MainForm";
    RegisterClassExA(&klass);

    RECT bounds{0, 0, px(8010), px(4785)};
    AdjustWindowRectEx(&bounds, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
    mainWindow = CreateWindowExA(0, klass.lpszClassName, "Air Guard - Setup",
                                 WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 bounds.right - bounds.left, bounds.bottom - bounds.top,
                                 nullptr, nullptr, instance, nullptr);
    if (!mainWindow) return 1;
    ShowWindow(mainWindow, show);
    UpdateWindow(mainWindow);

    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(mainWindow, &message)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    return static_cast<int>(message.wParam);
}
