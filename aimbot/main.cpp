#include <iostream>
#include <Windows.h>

#define KeyDown -32768
#define KeyUp 0

const float PI = 3.14159265358f;

struct Vector3
{
    float x, y, z;
};

struct offsets
{
    DWORD dw_PlayerCountOffs = 0x5E96BC;
    DWORD Player_Base = 0x4C6708;
    DWORD dw_mTeamOffset = 0x9C;
    DWORD dw_Health = 0x94;
    DWORD dw_Pos = 0x260;
    DWORD bDormant = 0x17E;
    DWORD bBoneBase = 0x578;
    DWORD dwAngle = 0x4791B4;
    DWORD EntityPlayer_Base = 0x4D3904;

} off;

struct Player
{
    DWORD maxPlayers;
    DWORD iTeam;
    DWORD localPlayer;
    DWORD gameModule;
    DWORD engineModule;
    DWORD iHealth;
    Vector3 mPosition;
    Vector3 iAngles;

    void getPlayer()
    {
        gameModule = (DWORD)GetModuleHandle(L"client.dll");
        engineModule = (DWORD)GetModuleHandle(L"engine.dll");

        localPlayer = *(DWORD*)(gameModule + off.Player_Base);
        iHealth = *(DWORD*)(localPlayer + off.dw_Health);
        iTeam = *(DWORD*)(localPlayer + off.dw_mTeamOffset);
        mPosition = *(Vector3*)(localPlayer + off.dw_Pos);
        iAngles = *(Vector3*)(engineModule + off.dwAngle);
        maxPlayers = *(DWORD*)(engineModule + off.dw_PlayerCountOffs);
    }
} player;

struct Enemy
{
    DWORD gameModule;
    DWORD entity;
    DWORD iHealth;
    DWORD iTeam;
    DWORD boneBase;
    bool iDormant;

    bool getEntity(int i)
    {
        gameModule = (DWORD)GetModuleHandle(L"client.dll");

        entity = *(DWORD*)(gameModule + off.EntityPlayer_Base + (i * 0x10));
        if (entity == NULL)
        {
            return false;
        }

        iHealth = *(DWORD*)(entity + off.dw_Health);
        iTeam = *(DWORD*)(entity + off.dw_mTeamOffset);
        boneBase = *(DWORD*)(entity + off.bBoneBase);
        iDormant = *(bool*)(entity + off.bDormant);

        return true;
    }
} enemy;

int getMaxPlayers()
{
    DWORD engineModule = (DWORD)GetModuleHandle(L"engine.dll");
    DWORD maxPlayers = *(DWORD*)(engineModule + off.dw_PlayerCountOffs);

    return maxPlayers;
}

float getAngleDistance(Vector3 myAngles, Vector3 enemyAngles)
{
    float distance = sqrt(pow(myAngles.x - enemyAngles.x, 2) + pow(myAngles.y - enemyAngles.y, 2) + pow(myAngles.y - enemyAngles.y, 2));

    return distance;
}

Vector3 CalcAngles(Vector3 enemy, Vector3 player)
{
    Vector3 viewAngles{};
    Vector3 deltaVec = { enemy.x - player.x, enemy.y - player.y, enemy.z - player.z };
    float deltaVecLength = sqrt(deltaVec.x * deltaVec.x + deltaVec.y * deltaVec.y + deltaVec.z * deltaVec.z);

    float pitch = -asin(deltaVec.z / deltaVecLength) * (180 / PI);
    float yaw = atan2(deltaVec.y, deltaVec.x) * (180 / PI);

    if (pitch >= -89 && pitch <= 89 && yaw >= -180 && yaw <= 180)
    {
        viewAngles.x = pitch;
        viewAngles.y = yaw;
        viewAngles.z = 0.0f;
    }

    return viewAngles;
}

Vector3 getBonePosition(int boneId)
{
    Vector3 bonePos;

    bonePos.x = *(float*)(enemy.boneBase + 0x30 * boneId + 0x0c);
    bonePos.y = *(float*)(enemy.boneBase + 0x30 * boneId + 0x1c);
    bonePos.z = *(float*)(enemy.boneBase + 0x30 * boneId + 0x2c);

    return bonePos;
}

Vector3 getClosestEnemy()
{
    Vector3 closestEnemyAngles{};
    float closestDistance = FLT_MAX;
    float currentDistance = 0;

    for (int i = 1; i < getMaxPlayers(); ++i)
    {
        bool succeeded = enemy.getEntity(i);

        if (!succeeded || enemy.entity == player.localPlayer)
            continue;

        if (enemy.iHealth < 2 || player.iHealth < 2)
            continue;

        if (enemy.iTeam == player.iTeam)
            continue;

        if (enemy.iDormant)
            continue;

        Vector3 bonePos = getBonePosition(14);
        bonePos.z -= 60;

        Vector3 enemyAngles = CalcAngles(bonePos, player.mPosition);

        currentDistance = getAngleDistance(player.iAngles, enemyAngles);

        if (currentDistance < closestDistance)
        {
            closestDistance = currentDistance;
            closestEnemyAngles = enemyAngles;
        }
    }
    return closestEnemyAngles;
}

void aimAt(Vector3 angles)
{
    DWORD engineModule = (DWORD)GetModuleHandle(L"engine.dll");
    *(Vector3*)(engineModule + off.dwAngle) = angles;
}

DWORD WINAPI HackThread(HMODULE hModule)
{
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    bool aimbotOn = false, keyHeld = false;

    while (!(GetAsyncKeyState(VK_END) & 1))
    {
        if (GetAsyncKeyState(VK_XBUTTON2) == KeyDown && !keyHeld)
        {
            aimbotOn = true;
            keyHeld = true;
        }

        if (GetAsyncKeyState(VK_XBUTTON2) == KeyUp && keyHeld)
        {
            aimbotOn = false;
            keyHeld = false;
        }

        if (aimbotOn)
        {
            player.getPlayer();

            Vector3 coords = getClosestEnemy();
            if (coords.x != NULL || coords.y != NULL)
            {
                aimAt(coords);
            }
        }
        Sleep(1);
    }

    fclose(f);
    FreeConsole();
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
