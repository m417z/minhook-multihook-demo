#include <windows.h>
#include "MinHook/MinHook.h"
#include "resource.h"

#define HOOKS_AMOUNT 5

void *pMessageBoxWOriginal[HOOKS_AMOUNT];

struct
{
	MH_STATUS(WINAPI *MH_Initialize)();
	MH_STATUS(WINAPI *MH_Uninitialize)();
	MH_STATUS(WINAPI *MH_CreateHook)(void* pTarget, void* const pDetour, void** ppOriginal);
	MH_STATUS(WINAPI *MH_RemoveHook)(void* pTarget);
	MH_STATUS(WINAPI *MH_EnableHook)(void* pTarget);
} MinHookFunctions[HOOKS_AMOUNT];

BOOL LoadMinHookLibraries(void);
LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int WINAPI MessageBoxW_Hook(int nHook, HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);

#define MessageBoxW_Hookn(n) \
	int WINAPI MessageBoxW_Hook##n(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) \
			{ return MessageBoxW_Hook(n, hWnd, lpText, lpCaption, uType); }

MessageBoxW_Hookn(0);
MessageBoxW_Hookn(1);
MessageBoxW_Hookn(2);
MessageBoxW_Hookn(3);
MessageBoxW_Hookn(4);

void *MessageBoxW_Hooks[HOOKS_AMOUNT] = {
	MessageBoxW_Hook0,
	MessageBoxW_Hook1,
	MessageBoxW_Hook2,
	MessageBoxW_Hook3,
	MessageBoxW_Hook4,
};

int CALLBACK WinMain(
	_In_  HINSTANCE hInstance,
	_In_  HINSTANCE hPrevInstance,
	_In_  LPSTR lpCmdLine,
	_In_  int nCmdShow
	)
{
	if(!LoadMinHookLibraries())
		ExitProcess(1);

	for(int i = 0; i < HOOKS_AMOUNT; i++)
		if(MinHookFunctions[i].MH_Initialize() != MH_OK)
			ExitProcess(1);

	INT_PTR ret = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)DlgProc);

	for(int i = 0; i < HOOKS_AMOUNT; i++)
		if(MinHookFunctions[i].MH_Uninitialize() != MH_OK)
			ExitProcess(1);

	ExitProcess((UINT)ret);
}

BOOL LoadMinHookLibraries(void)
{
#ifdef _WIN64
	const WCHAR *szLibFileName = L"MinHook.x64.dll";
#else
	const WCHAR *szLibFileName = L"MinHook.x86.dll";
#endif

	WCHAR szCurrentPath[MAX_PATH];
	DWORD dwCurrentPathLen = GetModuleFileName(NULL, szCurrentPath, MAX_PATH);
	if(dwCurrentPathLen == 0)
		return FALSE;

	do
	{
		dwCurrentPathLen--;
		if(dwCurrentPathLen == 0)
			return FALSE;
	}
	while(szCurrentPath[dwCurrentPathLen] != L'\\');

	lstrcpy(szCurrentPath + dwCurrentPathLen + 1, szLibFileName);

	WCHAR szTempPath[MAX_PATH];
	DWORD dwTempPathLen = GetTempPath(MAX_PATH, szTempPath);
	if(dwTempPathLen == 0)
		return FALSE;

	lstrcpy(szTempPath + dwTempPathLen, szLibFileName);
	dwTempPathLen += lstrlen(szLibFileName);

	for(int i = 0; i < HOOKS_AMOUNT; i++)
	{
		wsprintf(szTempPath + dwTempPathLen, L"%d", i);

		CopyFile(szCurrentPath, szTempPath, FALSE);

		HMODULE hModule = LoadLibrary(szTempPath);
		if(!hModule)
			return FALSE;

		*(FARPROC *)&MinHookFunctions[i].MH_Initialize = GetProcAddress(hModule, "MH_Initialize");
		*(FARPROC *)&MinHookFunctions[i].MH_Uninitialize = GetProcAddress(hModule, "MH_Uninitialize");
		*(FARPROC *)&MinHookFunctions[i].MH_CreateHook = GetProcAddress(hModule, "MH_CreateHook");
		*(FARPROC *)&MinHookFunctions[i].MH_RemoveHook = GetProcAddress(hModule, "MH_RemoveHook");
		*(FARPROC *)&MinHookFunctions[i].MH_EnableHook = GetProcAddress(hModule, "MH_EnableHook");
	}

	return TRUE;
}

LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_TEST:
			MessageBoxW(hWnd, L"This is a test", L"Test", MB_OK);
			break;

		default:
			if(LOWORD(wParam) >= IDC_CHECK1 && LOWORD(wParam) < IDC_CHECK1 + HOOKS_AMOUNT)
			{
				int nHook = LOWORD(wParam) - IDC_CHECK1;
				if(IsDlgButtonChecked(hWnd, LOWORD(wParam)))
				{
					if(MinHookFunctions[nHook].MH_CreateHook(MessageBoxW, MessageBoxW_Hooks[nHook], &pMessageBoxWOriginal[nHook]) == MH_OK)
						MinHookFunctions[nHook].MH_EnableHook(MessageBoxW);
				}
				else
				{
					MinHookFunctions[nHook].MH_RemoveHook(MessageBoxW);
				}
			}
			break;

		case IDCANCEL:
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_DESTROY:
		break;
	}

	return FALSE;
}

int WINAPI MessageBoxW_Hook(int nHook, HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
	TCHAR szNewText[1024 + 1];
	wsprintf(szNewText, L"HOOK%d:\n%s", nHook + 1, lpText ? lpText : L"");
	lpText = szNewText;

	return
		((int (WINAPI *)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType))pMessageBoxWOriginal[nHook])
		(hWnd, lpText, lpCaption, uType);
}
