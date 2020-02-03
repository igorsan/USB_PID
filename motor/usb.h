#ifndef __USB_H__
#define __USB_H__

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "resource.h"
extern "C" {
#include "hidsdi.h"
#include "hidpi.h"
#include <setupapi.h>
}
#include <process.h>
#include <dbt.h>
#include <initguid.h>

/* global constants */

typedef union _DATOS
{
	WORD word0;
    struct
    {
		BYTE byte0;
		BYTE byte1;
    };
}DATOS;
typedef struct _usb{
	BOOL		endthread;			
	BOOL		TMThreadActive;		
	HANDLE		HidDevHandle;
	BOOLEAN		HidAttached;
	HIDP_CAPS	Capabilities;
	OVERLAPPED	HidOverlapped;
	HANDLE		ReportEvent;
	HWND		hwnd;
	HDC			hdc;
	char		referencia;
}usb;

class CBaseTimer {
public: 
    CBaseTimer(int nIntervalMs=1000) { m_nIntervalMs=nIntervalMs; };
    ~CBaseTimer()               { Kill(); };                         
       int m_nID; 
       long m_nIntervalMs; 
 
    static void CALLBACK MyTimerProc( UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2 ) { 
        CBaseTimer* pThis= (CBaseTimer*)dwUser; 
        pThis->OnTimer(); 
    } 
 
    void Start() {  
        m_nID= timeSetEvent( m_nIntervalMs, 10, MyTimerProc, (DWORD)this, TIME_PERIODIC ); //<<--- Note: this 
    } 
    void Kill()  { 
        ::timeKillEvent( m_nID ); 
    } 
    virtual void OnTimer()=0;  
}; 

BOOL bOpenHidDevice(HANDLE*);
BOOL bHidDeviceNotify(HWND, HDEVNOTIFY);
void InitializeParam(LPVOID);
void OutputReport(LPVOID);
void ImputReport(LPVOID);
void read(LPVOID);
void TMeasure(LPVOID);
void WriteFeatureReport(LPVOID);
void ReadFeatureReport(LPVOID);
void step(LPVOID);
void PID(LPVOID);

#endif