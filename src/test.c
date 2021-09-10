#include <windows.h>
#include "MinHook/MinHook.h"
#include "resource.h"

#define HOOKS_AMOUNT 5

#define STRESS_TEST_DEFAULT_THREADS 4
#define STRESS_TEST_MAX_THREADS 256
#define STRESS_TEST_DEFAULT_SECONDS 10

DWORD dwStressTestEnd;
DWORD nStressTestIterations;

// Use this nop function for stress tests - the instruction pointer can
// land on any of the single byte nop instructions, which is good for testing.
// The code works for both 32-bit and 64-bit x86.

#define NOP_FUNCTION_CODE \
	"\x90\x90\x90\x90" \
	"\x90\x90\x90\x90" \
	"\x90\x90\x90\x90" \
	"\x90\x90\x90\x90" \
	"\xC3"

__pragma(code_seg(push, stack1, ".text"))
__declspec(allocate(".text")) char NopFunctionCode[] = NOP_FUNCTION_CODE;
__pragma(code_seg(pop, stack1))

typedef void(WINAPI* NoArgsFunction)(void);

NoArgsFunction NopFunction = (NoArgsFunction)NopFunctionCode;

void* pNopFunction1Original;
void* pNopFunction2Original;

LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int WINAPI MessageBoxW_Hook(int nHook, HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
int StressTest(HWND hWnd, int seconds, int threads);
DWORD WINAPI StressTestThread(LPVOID lpThreadParameter);
DWORD WINAPI StressTestSecondaryThread(LPVOID lpThreadParameter);
void WINAPI NopFunction1Hook(void);
void WINAPI NopFunction2Hook(void);

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

void *pMessageBoxWOriginal[HOOKS_AMOUNT];

int CALLBACK WinMain(
	_In_  HINSTANCE hInstance,
	_In_  HINSTANCE hPrevInstance,
	_In_  LPSTR lpCmdLine,
	_In_  int nCmdShow
	)
{
	if (MH_Initialize() != MH_OK)
		__debugbreak();

	INT_PTR ret = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)DlgProc);

	if (MH_Uninitialize() != MH_OK)
		__debugbreak();

	ExitProcess((UINT)ret);
}

LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		CheckDlgButton(hWnd, IDC_FREEZE_METHOD_ORIGINAL, BST_CHECKED);

		SetDlgItemInt(hWnd, IDC_STRESS_THREADS, STRESS_TEST_DEFAULT_THREADS, TRUE);
		SetDlgItemInt(hWnd, IDC_STRESS_SECONDS, STRESS_TEST_DEFAULT_SECONDS, TRUE);
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_TEST:
			MessageBoxW(hWnd, L"This is a test", L"Test", MB_OK);
			break;
			
		case IDC_STRESS_TEST:
			{
				int threads = GetDlgItemInt(hWnd, IDC_STRESS_THREADS, NULL, TRUE);
				if (threads < 1 || threads > STRESS_TEST_MAX_THREADS)
				{
					MessageBoxW(hWnd, L"Too many or too little threads", L"Stress test", MB_OK);
					break;
				}

				int seconds = GetDlgItemInt(hWnd, IDC_STRESS_SECONDS, NULL, TRUE);

				int iterations = StressTest(hWnd, seconds, threads);

				WCHAR szMsg[1025];
				wsprintf(szMsg, L"Done, iterations: %d", iterations);
				MessageBoxW(hWnd, szMsg, L"Stress test", MB_OK);
			}
			break;

		case IDC_FREEZE_METHOD_ORIGINAL:
			MH_SetThreadFreezeMethod(MH_FREEZE_METHOD_ORIGINAL);
			break;

		case IDC_FREEZE_METHOD_FAST_UNDOCUMENTED:
			MH_SetThreadFreezeMethod(MH_FREEZE_METHOD_FAST_UNDOCUMENTED);
			break;

		case IDC_FREEZE_METHOD_NONE_UNSAFE:
			MH_SetThreadFreezeMethod(MH_FREEZE_METHOD_NONE_UNSAFE);
			break;

		default:
			if (LOWORD(wParam) >= IDC_CHECK1 && LOWORD(wParam) < IDC_CHECK1 + HOOKS_AMOUNT)
			{
				int nHook = LOWORD(wParam) - IDC_CHECK1;
				if (IsDlgButtonChecked(hWnd, LOWORD(wParam)))
				{
					if (MH_CreateHookEx(nHook, MessageBoxW, MessageBoxW_Hooks[nHook], &pMessageBoxWOriginal[nHook]) != MH_OK)
						__debugbreak();

					if (MH_EnableHookEx(nHook, MessageBoxW) != MH_OK)
						__debugbreak();
				}
				else
				{
					if (MH_RemoveHookEx(nHook, MessageBoxW) != MH_OK)
						__debugbreak();
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

int StressTest(HWND hWnd, int seconds, int threads)
{
	dwStressTestEnd = GetTickCount() + 1000 * seconds;

	// Hook the same function twice to stress the multihook mechanism, which
	// makes sure the hooks are unhooked correctly even if not done in FILO order.
	if (MH_CreateHookEx(1, NopFunction, NopFunction1Hook, &pNopFunction1Original) != MH_OK)
		__debugbreak();
	
	if (MH_CreateHookEx(2, NopFunction, NopFunction2Hook, &pNopFunction2Original) != MH_OK)
		__debugbreak();

	HANDLE hThreads[STRESS_TEST_MAX_THREADS];

	for (int i = 0; i < threads; i++)
	{
		hThreads[i] = CreateThread(NULL, 0, StressTestThread, (LPVOID)(LONG_PTR)i, 0, NULL);
		if (!hThreads[i])
			__debugbreak();
	}

	EnableWindow(hWnd, FALSE);

	for (int i = 0; i < threads; i++)
	{
		MSG msg;
		while (1)
		{
			DWORD dwWait = MsgWaitForMultipleObjects(1, &hThreads[i], FALSE, INFINITE, QS_ALLINPUT);
			if (dwWait == WAIT_OBJECT_0)
			{
				CloseHandle(hThreads[i]);
				break;
			}

			if (dwWait == WAIT_OBJECT_0 + 1)
			{
				// We have a message - peek and dispatch it.
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (!IsDialogMessage(hWnd, &msg))
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			}
		}
	}

	EnableWindow(hWnd, TRUE);

	if (MH_RemoveHookEx(1, NopFunction) != MH_OK)
		__debugbreak();

	if (MH_RemoveHookEx(2, NopFunction) != MH_OK)
		__debugbreak();

	return nStressTestIterations;
}

DWORD WINAPI StressTestThread(LPVOID lpThreadParameter)
{
	DWORD dwEnd = dwStressTestEnd;

	int nThread = (int)(LONG_PTR)lpThreadParameter;
	if (nThread == 0)
	{
		int iterations = 0;

		while (GetTickCount() < dwEnd)
		{
			// Enable and disable not in FILO order.
			MH_EnableHookEx(1, NopFunction);
			MH_EnableHookEx(2, NopFunction);
			MH_DisableHookEx(1, NopFunction);
			MH_EnableHookEx(1, NopFunction);
			MH_DisableHookEx(2, NopFunction);
			MH_DisableHookEx(1, NopFunction);

			iterations += 6;
		}

		nStressTestIterations = iterations;
	}
	else
	{
		HANDLE hSecondaryThread = NULL;
		DWORD dwSecondaryThreadNext = GetTickCount();

		while (GetTickCount() < dwEnd)
		{
			for (int i = 0; i < 1000; i++)
				NopFunction();

			if (nThread == 1 && GetTickCount() >= dwSecondaryThreadNext)
			{
				if (hSecondaryThread && WaitForSingleObject(hSecondaryThread, 0) == WAIT_OBJECT_0)
				{
					CloseHandle(hSecondaryThread);
					hSecondaryThread = NULL;
				}

				DWORD dwDelay = 200;

				if (!hSecondaryThread)
				{
					// Throw in a thread in between.
					// The idea is to stress test thread creation in the middle of
					// MinHook enumerating for threads, as it might miss a newly
					// created thread, which can in turn cause a crash due to an
					// unadjusted instruction pointer register.
					hSecondaryThread = CreateThread(NULL, 0, StressTestSecondaryThread, (LPVOID)(LONG_PTR)dwDelay, 0, NULL);
					if (!hSecondaryThread)
						__debugbreak();
				}

				dwSecondaryThreadNext = GetTickCount() + dwDelay + 100;
			}
		}

		if (hSecondaryThread)
		{
			WaitForSingleObject(hSecondaryThread, INFINITE);
			CloseHandle(hSecondaryThread);
		}
	}

	return 0;
}

DWORD WINAPI StressTestSecondaryThread(LPVOID lpThreadParameter)
{
	DWORD dwDelay = (DWORD)(LONG_PTR)lpThreadParameter;
	DWORD dwEnd = GetTickCount() + dwDelay;
	while (GetTickCount() < dwEnd)
	{
		for (int i = 0; i < 1000; i++)
			NopFunction();
	}

	return 0;
}

void WINAPI NopFunction1Hook(void)
{
	((void (WINAPI*)(void))pNopFunction1Original)();
}

void WINAPI NopFunction2Hook(void)
{
	((void (WINAPI*)(void))pNopFunction2Original)();
}
