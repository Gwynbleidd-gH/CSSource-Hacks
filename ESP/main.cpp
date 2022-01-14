#include <iostream>
#include <Windows.h>
#define DEBUG

//HBRUSH TeamBrush = CreateSolidBrush(0x000000FF);
HBRUSH EnemyBrush = CreateSolidBrush(0x000000FF);

HWND hwndCSGO;
//HBRUSH Brush;
HDC hdc;

int screenX = 800;
int screenY = 600;

struct Vector3
{
    float x, y, z;
};

struct ViewMatrix
{
    float* operator[ ](int index)
    {
        return matrix[index];
    }

    float matrix[4][4];
};

struct Offsets
{
    DWORD dwLocalPlayer = 0x4C6708;
    DWORD m_iTeamNum = 0x9C;
    DWORD m_iHealth = 0x94;
    DWORD bDormant = 0x17E;
    DWORD m_dwBoneMatrix = 0x578;
    DWORD m_vecOrigin = 0x260;
    DWORD m_vecViewAngles = 0x4791B4;
    DWORD dwEntityList = 0x4D3904;
    DWORD m_viewMatrix = 0x5ADBF8;

} off;

Vector3 WorldToScreen(const Vector3 pos, ViewMatrix matrix)
{
    float _x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
    float _y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];
    float w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];

    float inv_w = 1.f / w;
    _x *= inv_w;
    _y *= inv_w;

    float x = screenX * .5f;
    float y = screenY * .5f;

    x += 0.5f * _x * screenX + 0.5f;
    y -= 0.5f * _y * screenY + 0.5f;

    return { x, y, w };
}

void DrawFilledRect(int x, int y, int w, int h)
{
    RECT rect = { x, y, x + w, y + h };
    FillRect(hdc, &rect, EnemyBrush);
}

void DrawBorderBox(int x, int y, int w, int h, int thickness)
{
    DrawFilledRect(x, y, w, thickness);
    DrawFilledRect(x, y, thickness, h);
    DrawFilledRect((x + w), y, thickness, h);
    DrawFilledRect(x, y + h, w + thickness, thickness);
}

class Player
{
private:
    Player();
public:
    static Player* getLocalPlayer()
    {
        static DWORD clientModule = (DWORD)(GetModuleHandle(L"client.dll"));
        static Player* localPlayer = (Player*)(clientModule + off.dwLocalPlayer);
        return localPlayer;
    }

    static DWORD getEngineModule()
    {
        static DWORD clientModule = (DWORD)(GetModuleHandle(L"engine.dll"));
        return clientModule;
    }

    int* getTeam()
    {
        return (int*)(*(DWORD*)this + off.m_iTeamNum);
    }

    int* getHealth()
    {
        return (int*)(*(DWORD*)this + off.m_iHealth);
    }

    Vector3* getPosition()
    {
        return (Vector3*)(*(DWORD*)this + off.m_vecOrigin);
    }

    static Vector3* getViewAngles()
    {
        static DWORD engineModule = (DWORD)GetModuleHandle(L"engine.dll");
        static Vector3* myAngles = (Vector3*)(engineModule + off.m_vecViewAngles);
        return myAngles;
    }
};

class Entity
{
private:
    Entity();
public:
    static Entity* getEntity(int i)
    {
        static DWORD clientModule = (DWORD)(GetModuleHandle(L"client.dll"));
        static DWORD entityList = clientModule + off.dwEntityList;

        return (Entity*)(entityList + i * 0x10);
    }

    int* getTeam()
    {
        return (int*)(*(DWORD*)this + off.m_iTeamNum);
    }

    int* getHealth()
    {
        return (int*)(*(DWORD*)this + off.m_iHealth);
    }

    bool* isDormant()
    {
        return (bool*)(*(DWORD*)this + off.bDormant);
    }

    Vector3* getOrigin()
    {
        return (Vector3*)(*(DWORD*)this + off.m_vecOrigin);
    }

    Vector3* getBonePos(int boneId)
    {
        DWORD boneMatrix = *(DWORD*)(*(DWORD*)this + off.m_dwBoneMatrix);
        static Vector3 bonePos;

        bonePos.x = *(float*)(boneMatrix + 0x30 * boneId + 0x0c);
        bonePos.y = *(float*)(boneMatrix + 0x30 * boneId + 0x1c);
        bonePos.z = *(float*)(boneMatrix + 0x30 * boneId + 0x2c);

        return &bonePos;
    }
};

void DrawESP()
{
    Player* localPlayer = Player::getLocalPlayer();
    hdc = GetDC(hwndCSGO);

    ViewMatrix viewM;
    memcpy(&viewM, (PBYTE*)(Player::getEngineModule() + off.m_viewMatrix), sizeof(viewM));

    for (int i = 1; i < 2; ++i)
    {
        Entity* entity = Entity::getEntity(i);

        if (entity == NULL || !*(DWORD*)(entity) || (DWORD)entity == (DWORD)localPlayer)
            continue;

        //if (*entity->getTeam() == *localPlayer->getTeam())
            //continue;

        if (*entity->getHealth() < 2 || *localPlayer->getHealth() < 2)
            continue;

        if (*entity->isDormant())
            continue;

        Vector3 playerPos = *entity->getOrigin();
        Vector3 boneHead;
        boneHead.x = playerPos.x;
        boneHead.y = playerPos.y;
        boneHead.z = playerPos.z + 75.0f;

        Vector3 screenPos = WorldToScreen(playerPos, viewM);
        Vector3 screenHead = WorldToScreen(boneHead, viewM);

        float height = screenHead.y - screenPos.y;
        float width = height / 2.4f;

        if (screenPos.z >= 0.01f)
            DrawBorderBox(screenPos.x - (width / 2), screenPos.y, width, height, 2);
    }
    DeleteObject(hdc);
}

DWORD WINAPI HackThread(HMODULE hModule)
{
#ifdef DEBUG
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    hwndCSGO = FindWindow(0, (L"Counter-Strike Source"));

#endif

    while (!(GetAsyncKeyState(VK_END) & 1))
    {
        DrawESP();
        Sleep(1);
    }

#ifdef DEBUG
    fclose(f);
    FreeConsole();
#endif

    FreeLibraryAndExitThread(hModule, 0);
    CloseHandle(hModule);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)HackThread, hModule, 0, nullptr);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

