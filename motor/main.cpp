#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <commctrl.h>
#include "resource.h"
#include <process.h>
#include "usb.h"

LPCTSTR ClassName = _T("Clase Igor");
LPCTSTR titulo = _T("Programa de Control");
HANDLE	hStdOut;


LRESULT CALLBACK Procedimiento(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CREATESTRUCT	*cs;
	static   HWND	hWndSlider;
	static	HWND	hWndProgress;
	static LRESULT res = 0;
	static HDEVNOTIFY *hDevNotify;
	static usb daq;
	HWND hwndLeftLabel;
	HWND hwndRightLabel;	
	DWORD	dwLeght=0, dwCuenta=0;
	char msgstring[64];
	HWND hStatus;

	static HDC hdcScreen;
	static HBRUSH hBrush; 
	static PAINTSTRUCT ps ;
	static RECT   rect;

	switch(msg)
	{
        case WM_CREATE:			
			daq.hdc = GetDC (hwnd);
			cs = (LPCREATESTRUCT)lParam;
			SetRect (&rect,50,100,430,355) ;

			hwndLeftLabel = CreateWindow("STATIC", "  Ref", 
                         WS_CHILD | WS_VISIBLE,
                         0, 0, 40, 20,
						 hwnd, (HMENU)IDC_TEXTLEFT, cs->hInstance, NULL);
	
			hwndRightLabel = CreateWindow("STATIC", "value", 
                         WS_CHILD | WS_VISIBLE,
                         0, 0, 40, 20,
                         hwnd, (HMENU)IDC_TEXTRIGTH, cs->hInstance, NULL);

		hWndProgress = CreateWindow(PROGRESS_CLASS,"test",WS_CHILD|
				WS_VISIBLE| WS_BORDER | PBS_SMOOTH, 50,50,380,20,hwnd,(HMENU)IDC_VELGRAPH,cs->hInstance,NULL);

		SendMessage(hWndProgress,PBM_SETRANGE,0, MAKELONG(0,255));
		SendMessage(hWndProgress,PBM_SETSTEP, WPARAM(0),0);

      hWndSlider = CreateWindowEx(0,TRACKBAR_CLASS, "slider",
         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
         50,10 , 380, 40,
         hwnd, (HMENU)ID_SPLITTER, cs->hInstance, NULL);

			SendMessage(hWndSlider, TBM_SETRANGE,  TRUE,  MAKELONG(0, 255)); 
			SendMessage(hWndSlider, TBM_SETPAGESIZE, 0,  20); 
			SendMessage(hWndSlider, TBM_SETTICFREQ, 20, 0); 
			SendMessage(hWndSlider, TBM_SETPOS, FALSE, 255); 
			SendMessage(hWndSlider, TBM_SETBUDDY, TRUE, (LPARAM) hwndLeftLabel); 
			SendMessage(hWndSlider, TBM_SETBUDDY, FALSE, (LPARAM) hwndRightLabel); 


			hStatus= CreateStatusWindow(WS_CHILD| WS_VISIBLE | SBARS_SIZEGRIP, "Status",
					hwnd,ID_STATUSBAR);


         return 0;

		case WM_PAINT:
		hdcScreen = BeginPaint (hwnd, &ps) ;
	    hBrush = CreateSolidBrush (RGB(180,180,180));
		FillRect (hdcScreen, &rect, hBrush) ;
		DeleteObject (hBrush) ;
		TextOut(hdcScreen, 80, 85, "                           Voltaje de entrada                              ",70);
		for (int x = 0; x < rect.right ; x+= 50)
		{
			MoveToEx (hdcScreen, x, 100, NULL) ;
			LineTo (hdcScreen, x, 300) ;
		}
		for (int y = 100 ; y <= 300 ; y += 50)
		{
			MoveToEx (hdcScreen, 50, y, NULL) ;
			LineTo (hdcScreen, 250, y) ;
		}	
		EndPaint (hwnd, &ps) ;
          return 0 ;


		case WM_HSCROLL:
			res = SendMessage(hWndSlider, TBM_GETPOS, 0, 0);
			daq.referencia = (unsigned int)res;
			sprintf(msgstring,"%d v",(BYTE) res);
			SetDlgItemText(hwnd, IDC_TEXTRIGTH,msgstring);	
         break;

       case WM_COMMAND:
           switch(LOWORD(wParam)) {
              case CM_CONECTAR:
				daq.HidAttached = bOpenHidDevice(&daq.HidDevHandle);
				if(daq.HidAttached){
					daq.endthread = FALSE;
					daq.TMThreadActive = FALSE;
					daq.hwnd=hwnd;
					WriteConsole(hStdOut,_T("\nconectado"),10,&dwCuenta,NULL);
				}else
					WriteConsole(hStdOut,_T("\ndesconectado"),13,&dwCuenta,NULL);

				if( bHidDeviceNotify(hwnd, hDevNotify) == FALSE)
					WriteConsole(hStdOut,_T("\nfalla al registrar el dispositivo"),34,&dwCuenta,NULL);

				 InitializeParam(&daq);
                 break;
              case CM_SET:
  					daq.endthread = FALSE;
					_beginthread(PID, 0,(void*)&daq);
                 break;
              case CM_GET:
  					daq.endthread = FALSE;
					_beginthread(PID2, 0,(void*)&daq);
                 break;
              case CM_ENVIA:
  					daq.endthread = FALSE;
					_beginthread(TMeasure, 0,(void*)&daq);
                 break;
              case CM_RECIBE:
					daq.endthread = FALSE;
					_beginthread(step, 0,(void*)&daq);
				  break;
           }
           break;

		case WM_DEVICECHANGE:
		{
			daq.HidAttached = bOpenHidDevice(&daq.HidDevHandle);
			switch(LOWORD(wParam)) {
				case DBT_DEVICEARRIVAL:
				{
					if(daq.endthread == TRUE && daq.HidAttached == TRUE)
					{
						daq.endthread = FALSE;
						daq.TMThreadActive = FALSE;
						WriteConsole(hStdOut,_T("\nse conecto"),11,&dwCuenta,NULL);
					}
					break;
				}
				case DBT_DEVICEREMOVECOMPLETE:
				{
					if(daq.endthread == FALSE && daq.HidAttached == FALSE)
					{
						daq.endthread = TRUE;
						WriteConsole(hStdOut,_T("\nse desconecto"),14,&dwCuenta,NULL);
					}
					break;
				}				
				default:
					break;
			}
			break;
		}
		case WM_CLOSE:
			daq.endthread = TRUE;
			if(daq.TMThreadActive == TRUE)
			{
				daq.TMThreadActive = FALSE;
				PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			}
			if(daq.HidDevHandle != INVALID_HANDLE_VALUE)
				CloseHandle(daq.HidDevHandle);
			UnregisterDeviceNotification( hDevNotify );

			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;
	DWORD	Leght=0, Cuenta=0;

	HANDLE hMutexOneInstance;
	hMutexOneInstance = CreateMutex(NULL, TRUE, _T("EvitarSegundaInstancia"));
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		return FALSE;
	if (hMutexOneInstance) 
		ReleaseMutex(hMutexOneInstance);


	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.style		 = CS_DBLCLKS;
	wc.lpfnWndProc	 = Procedimiento;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = hInstance;	
	wc.hIcon		 = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground =  GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName  =  NULL;
	wc.lpszClassName = ClassName;
	wc.hIconSm  = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 16, 16, 0);


	if(!RegisterClassEx(&wc))
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"),
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		ClassName,
		titulo,
		WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, 500, 440,
		HWND_DESKTOP, LoadMenu(hInstance, _T("Menu")), hInstance, NULL);

	if(hwnd == NULL){
		MessageBox(NULL, _T("Fallo la creacion de Ventana"), _T("Error!"),
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	if(!AllocConsole()){
		MessageBox(hwnd, _T("No inicio consola"), _T("Error!"),
		MB_ICONEXCLAMATION | MB_OK);
	}	
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	Leght =(DWORD) _tcslen(titulo);
	WriteConsole(hStdOut,titulo,Leght,&Cuenta,NULL);
	WriteConsole(hStdOut,_T("\n\n"),2,&Cuenta,NULL);


	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return (int )Msg.wParam;
}