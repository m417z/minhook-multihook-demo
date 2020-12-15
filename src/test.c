#include <windows.h>
#include "MinHook/MinHook.h"
#include "resource.h"

#define HOOKS_AMOUNT 5

void *pMessageBoxWOriginal[HOOKS_AMOUNT];

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
	if(MH_Initialize() != MH_OK)
		ExitProcess(1);

	INT_PTR ret = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)DlgProc);

	if(MH_Uninitialize() != MH_OK)
		ExitProcess(1);

	ExitProcess((UINT)ret);
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
					if(MH_CreateHookEx(nHook, MessageBoxW, MessageBoxW_Hooks[nHook], &pMessageBoxWOriginal[nHook]) == MH_OK)
						MH_EnableHookEx(nHook, MessageBoxW);
				}
				else
				{
					MH_RemoveHookEx(nHook, MessageBoxW);
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
