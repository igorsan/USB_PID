#include "usb.h"
#include<time.h>
#include <stdlib.h>
#include <fstream>
#include <Mmsystem.h> 
#include <math.h>

#pragma comment(lib, "Winmm.lib" )

#define	Nmuestras	200
#define Tmuestreo 80
extern HANDLE	hStdOut;
typedef double real;
static int m_started;
static real m_kp, m_ki, m_kd, m_h, m_inv_h, m_prev_error,
        m_error_thresh, m_integral;


void PID_Initialize(real kp, real ki,
    real kd, real error_thresh, real step_time)
{
    m_kp = kp;
    m_ki = ki;
    m_kd = kd;
    m_error_thresh = error_thresh;

    m_h = step_time;
    m_inv_h = 1 / step_time;

    m_integral = 0;
    m_started = 0;
}

real PID_Update(real error)
{
    real q, deriv;

    if (fabs(error) < m_error_thresh)
        q = 1;
    else
        q = 0;

    m_integral += m_h*q*error;

    if (!m_started)
    {
        m_started = 1;
        deriv = 0;
    }
    else
        deriv = (error - m_prev_error) * m_inv_h;

    m_prev_error = error;
    return m_kp*(error + m_ki*m_integral + m_kd*deriv);
}

class CMsgTickTimer: public CBaseTimer { 
public: 
    CMsgTickTimer( int nIntervalMs, LPVOID lpParameter) { 
        m_nIntervalMs= nIntervalMs;
		bandera = FALSE;
    }
    bool	bandera;
    void OnTimer() {
		DWORD	Cuenta=0;
		bandera = TRUE;
	}
}; 

BOOL SaveFile( LPCTSTR pszFileName, unsigned int muestras[])
{
	BOOL bSuccess = FALSE;
	char msg[10];
	std::ofstream test(pszFileName);       
	for(int i=0; i < Nmuestras; i++){				
		sprintf_s(msg,10,"\n %u",muestras[i]);
		test <<msg;
	}
	bSuccess = TRUE;
	test.close();	
	return bSuccess;
}

void FileSave(HWND hwnd,unsigned int muestras[])
{
	OPENFILENAME ofn;
	char szFileName[MAX_PATH] = "";

	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "txt";
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if(GetSaveFileName(&ofn))
	{		
		SaveFile( szFileName,muestras);
	}
}

BOOL bOpenHidDevice(HANDLE *HidDevHandle)
{
static GUID HidGuid;
HDEVINFO HidDevInfo;
SP_DEVICE_INTERFACE_DATA devInfoData;
BOOLEAN Result;
DWORD Index;
DWORD DataSize;
BOOLEAN GotRequiredSize;
PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = 0;
DWORD RequiredSize;
BOOLEAN DIDResult;
HIDD_ATTRIBUTES HIDAttrib;
const unsigned short int VID = 0x0925;
const unsigned short int PID = 0x1299;

		GotRequiredSize = FALSE;
		HidD_GetHidGuid(&HidGuid);
		HidDevInfo = SetupDiGetClassDevs(&HidGuid,NULL,NULL,DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
		Index = 0;
		devInfoData.cbSize = sizeof(devInfoData);				
		do {
				Result = SetupDiEnumDeviceInterfaces(HidDevInfo,0,&HidGuid,Index, &devInfoData);
				if(Result == FALSE)
				{
					if(detailData != NULL)
						free(detailData);
					SetupDiDestroyDeviceInfoList(HidDevInfo);
					return FALSE;
				}
				if(GotRequiredSize == FALSE)
				{
					DIDResult = SetupDiGetDeviceInterfaceDetail( HidDevInfo,&devInfoData,NULL,0,&DataSize,NULL);
					GotRequiredSize = TRUE;
					detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(DataSize);
					detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
				}
				DIDResult = SetupDiGetDeviceInterfaceDetail( HidDevInfo, &devInfoData, detailData, DataSize, &RequiredSize,	NULL);
				*HidDevHandle = CreateFile( detailData->DevicePath,	GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,	NULL);

				if(*HidDevHandle != INVALID_HANDLE_VALUE)
				{
					HIDAttrib.Size = sizeof(HIDAttrib);
					HidD_GetAttributes(	*HidDevHandle, &HIDAttrib);
					if((HIDAttrib.VendorID == VID) && (HIDAttrib.ProductID == PID))
					{
						if(detailData != NULL)
							free(detailData);
						SetupDiDestroyDeviceInfoList(HidDevInfo);
						return TRUE;	// found HID device 
					}
					CloseHandle(*HidDevHandle);
				}
				Index++;	
		} while(Result == TRUE);
	if(detailData != NULL)
		free(detailData);

	SetupDiDestroyDeviceInfoList(HidDevInfo);
	return FALSE;
}

BOOL bHidDeviceNotify(HWND hWnd, HDEVNOTIFY hDevNotify)
{
GUID HidGuid;
DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	HidD_GetHidGuid(&HidGuid);
	ZeroMemory( &NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	NotificationFilter.dbcc_classguid = HidGuid;
	hDevNotify = RegisterDeviceNotification(	hWnd, 
												&NotificationFilter, 
												DEVICE_NOTIFY_WINDOW_HANDLE );
	if(!hDevNotify)
		return FALSE;

	return TRUE;
}
void InitializeParam(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	PHIDP_PREPARSED_DATA	HidParsedData;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];
	daq->TMThreadActive = TRUE;
	WriteConsole(hStdOut,_T("\ndevice parammeters "),20,&Cuenta,NULL);

	if(daq->HidAttached)
	{
		HidD_GetPreparsedData(daq->HidDevHandle, &HidParsedData);
		HidP_GetCaps( HidParsedData ,&daq->Capabilities);
		HidD_FreePreparsedData(HidParsedData);

		sprintf_s(msg,100,"\nUsage Page: %X ",daq->Capabilities.UsagePage);
		WriteConsole(hStdOut,msg,17,&Cuenta,NULL);
		sprintf_s(msg,100,"\nOutputReportByteLength %d ",daq->Capabilities.OutputReportByteLength);
		WriteConsole(hStdOut,msg,26,&Cuenta,NULL);
		sprintf_s(msg,100,"\nInputReportByteLength %d ",daq->Capabilities.InputReportByteLength);
		WriteConsole(hStdOut,msg,25,&Cuenta,NULL);
		sprintf_s(msg,100,"\nFeatureReportByteLength %d ",daq->Capabilities.FeatureReportByteLength);
		WriteConsole(hStdOut,msg,27,&Cuenta,NULL);

		daq->ReportEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		daq->HidOverlapped.hEvent = daq->ReportEvent;
		daq->HidOverlapped.Offset = 0;
		daq->HidOverlapped.OffsetHigh = 0;
	}
	daq->TMThreadActive = FALSE;
}

void OutputReport(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	unsigned long	numBytesReturned;
	unsigned char   outbuffer[9];
	BOOL			bResult;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];

	daq->TMThreadActive = TRUE;

	if(daq->HidAttached)
	{
		outbuffer[0] = 0;
		outbuffer[1] = 5;
		outbuffer[2] = 7;
		sprintf_s(msg,100,"\nsending output report %X %X ",outbuffer[1],outbuffer[2]);
		WriteConsole(hStdOut,msg,27,&Cuenta,NULL);

		bResult = WriteFile(	daq->HidDevHandle, 
								&outbuffer[0], 
								daq->Capabilities.OutputReportByteLength, 
								&numBytesReturned, 
								NULL);
		
		bResult = WaitForSingleObject(ReportEvent, 200);
		if(bResult)
			WriteConsole(hStdOut,_T("\nwrite OK "),10,&Cuenta,NULL);
	}
	daq->TMThreadActive = FALSE;
}

void ImputReport(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	unsigned long	numBytesReturned;
	unsigned char	inbuffer[9];
	BOOL			bResult;
	short			temperature = 0;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];

	daq->TMThreadActive = TRUE;
	WriteConsole(hStdOut,_T("\nReading an imput report "),24,&Cuenta,NULL);

	if(daq->HidAttached)
	{
		while(daq->endthread == FALSE)
		{

			bResult = ReadFile(	daq->HidDevHandle,							
								&inbuffer[0],						
								daq->Capabilities.InputReportByteLength,
								&numBytesReturned,					
								(LPOVERLAPPED) &daq->HidOverlapped );
			bResult = WaitForSingleObject( ReportEvent,	300);	
			if(bResult == WAIT_TIMEOUT || bResult == WAIT_ABANDONED){
				CancelIo(&daq->HidDevHandle);
				WriteConsole(hStdOut,_T("\nReading FAIL "),13,&Cuenta,NULL);
			}

			if(bResult == WAIT_OBJECT_0)
			{
				temperature = (short)inbuffer[0] + ((short)inbuffer[1] << 8);
				sprintf_s(msg,100,"\nse recibio %X ",temperature);
				WriteConsole(hStdOut,msg,15,&Cuenta,NULL);				
			}
		}
	}
	daq->TMThreadActive = FALSE;
}
void TMeasure2(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	unsigned long	numBytesReturned;
	unsigned char   outbuffer[9];		
	unsigned char	inbuffer[9];		
	short			temperature = 0;	
	BOOL			bResult;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];
	unsigned int i=0;

	daq->TMThreadActive = TRUE;
	if(daq->HidAttached)
	{
			outbuffer[0] = 0;
			outbuffer[1] = 5;
			outbuffer[2] = 5;

		while(i<200)
		{
			
			sprintf_s(msg,100,"\noutput %d = %X %X      ",i,outbuffer[1],outbuffer[2]);
			WriteConsole(hStdOut,msg,18,&Cuenta,NULL);


			bResult = WriteFile(daq->HidDevHandle, 
								&outbuffer[0], 
								daq->Capabilities.OutputReportByteLength, 
								&numBytesReturned, 
								(LPOVERLAPPED) &daq->HidOverlapped);
		
			if(bResult){
				i++;
				bResult = WaitForSingleObject(ReportEvent, 200);
			}
			bResult = ReadFile(	daq->HidDevHandle,							
								&inbuffer[0],						
								daq->Capabilities.InputReportByteLength,
								&numBytesReturned,					
								(LPOVERLAPPED) &daq->HidOverlapped );

			bResult = WaitForSingleObject( 	daq->ReportEvent,300);
			if(bResult == WAIT_TIMEOUT || bResult == WAIT_ABANDONED)
				CancelIo(&daq->HidDevHandle);

			if(bResult == WAIT_OBJECT_0)
			{				
				sprintf_s(msg,100,"\nentrada %u = %u bufer1= %X bufer2=%X           ",i,(unsigned)((inbuffer[1]*4)+(inbuffer[2])),inbuffer[1],inbuffer[2]);
				outbuffer[1] = inbuffer[1];
				outbuffer[2] = inbuffer[1];
				WriteConsole(hStdOut,msg,38,&Cuenta,NULL);				
			}				
			Sleep(200);
		}
	}
	daq->TMThreadActive = FALSE;
}

void TMeasure5(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	unsigned long	numBytesReturned;
	unsigned char   outbuffer[9];
	unsigned char	inbuffer[9];
	short			temperature = 0;
	BOOL			bResult;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];
	unsigned int i=0;
	DATOS data;
	unsigned int MuestraIn[Nmuestras+1];
	unsigned int MuestraOut[Nmuestras+1];

	srand( (unsigned)time(NULL));
	sprintf_s(msg,100,"\nTiempo de muestreo =  %d    ",Tmuestreo);
	WriteConsole(hStdOut,msg,28,&Cuenta,NULL);
	for(int j=0;j<Nmuestras;j++)
		MuestraOut[j]=(char)(rand()*255);


	daq->TMThreadActive = TRUE;
	if(daq->HidAttached)
	{
		CMsgTickTimer cTimer(Tmuestreo, "1/2 second is up" );
		cTimer.Start();

		while(i<Nmuestras)
		{
			if(cTimer.bandera==TRUE){
				outbuffer[0] = 0;
				data.word0=i;
				outbuffer[1] = MuestraOut[i];
				outbuffer[2] = MuestraOut[i];
			
				bResult = WriteFile(daq->HidDevHandle, 
								&outbuffer[0], 
								daq->Capabilities.OutputReportByteLength, 
								&numBytesReturned, 
								(LPOVERLAPPED) &daq->HidOverlapped);

				bResult = WaitForSingleObject(ReportEvent, 200);

				bResult = ReadFile(	daq->HidDevHandle,							
								&inbuffer[0],						
								daq->Capabilities.InputReportByteLength,	
								&numBytesReturned,					
								(LPOVERLAPPED) &daq->HidOverlapped );

				bResult = WaitForSingleObject( 	daq->ReportEvent,300);	
				if(bResult == WAIT_TIMEOUT || bResult == WAIT_ABANDONED)
					CancelIo(&daq->HidDevHandle);

				if(bResult == WAIT_OBJECT_0)
				{
					MuestraIn[i]=inbuffer[2];

					i++;
				}
				cTimer.bandera=FALSE;
			}
			Sleep(0);
		}
	}
	daq->TMThreadActive = FALSE;
	FileSave(daq->hwnd,MuestraOut);
	FileSave(daq->hwnd,MuestraIn);
}

void TMeasure(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	unsigned long	numBytesReturned;
	unsigned char   outbuffer[9];
	unsigned char	inbuffer[9];
	short			temperature = 0;
	BOOL			bResult;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];
	unsigned int i=0;	
	unsigned int MuestraIn[Nmuestras+1];
	unsigned int MuestraOut[Nmuestras+1];

	srand( (unsigned)time(NULL));
	sprintf_s(msg,100,"\nTiempo de muestreo =  %d    ",Tmuestreo);
	WriteConsole(hStdOut,msg,28,&Cuenta,NULL);

	for(int j=0;j<Nmuestras;j++)
		MuestraOut[j]=(unsigned int)(rand()/129);

	MuestraOut[Nmuestras-1]=0x00;
	daq->TMThreadActive = TRUE;
	if(daq->HidAttached)
	{
		CMsgTickTimer cTimer( Tmuestreo, "1/2 second is up" );
		cTimer.Start();

		while(i<1000)
		{
			if(cTimer.bandera==TRUE){
				outbuffer[0] = 0;				
				outbuffer[1] = MuestraOut[i];
				outbuffer[2] = MuestraOut[i];
			
				bResult = WriteFile(daq->HidDevHandle, 
								&outbuffer[0], 
								daq->Capabilities.OutputReportByteLength, 
								&numBytesReturned, 
								(LPOVERLAPPED) &daq->HidOverlapped);

				bResult = WaitForSingleObject(ReportEvent, 200);

				bResult = ReadFile(	daq->HidDevHandle,						
								&inbuffer[0],						
								daq->Capabilities.InputReportByteLength,
								&numBytesReturned,					
								(LPOVERLAPPED) &daq->HidOverlapped );

				bResult = WaitForSingleObject( 	daq->ReportEvent,300);	
				if(bResult == WAIT_TIMEOUT || bResult == WAIT_ABANDONED)
					CancelIo(&daq->HidDevHandle);

				if(bResult == WAIT_OBJECT_0)
				{
					MuestraIn[i]=(unsigned)inbuffer[2];
					i++;
				}
				cTimer.bandera=FALSE;
			}
			Sleep(0);
		}
	}
	daq->TMThreadActive = FALSE;
	FileSave(daq->hwnd,MuestraOut);
	FileSave(daq->hwnd,MuestraIn);
}

void step(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	HWND			hVelGraph;
	unsigned long	numBytesReturned;
	unsigned char   outbuffer[9];		
	unsigned char	inbuffer[9];		
	short			temperature = 0;	
	BOOL			bResult;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];
	unsigned int i=0,y;
	DATOS data;
	unsigned int MuestraIn[Nmuestras+1];
	unsigned int MuestraOut[Nmuestras+1];

	sprintf_s(msg,100,"\nTiempo de muestreo =  %d    ",Tmuestreo);
	WriteConsole(hStdOut,msg,28,&Cuenta,NULL);

	for(int j=0;j<Nmuestras;j++)
		MuestraOut[j]=0xFF;

	MuestraOut[Nmuestras-1]=0x00;
	daq->TMThreadActive = TRUE;
	if(daq->HidAttached)
	{

		CMsgTickTimer cTimer( Tmuestreo, "1/2 second is up" );
		cTimer.Start();
		hVelGraph = GetDlgItem (daq->hwnd, IDC_VELGRAPH);
		MoveToEx (daq->hdc, 50,355, NULL) ;
		while(i<Nmuestras)
		{
			if(cTimer.bandera==TRUE){
				outbuffer[0] = 0;
				data.word0=i;
				outbuffer[1] = MuestraOut[i];
				outbuffer[2] = MuestraOut[i];
			
				bResult = WriteFile(daq->HidDevHandle, 
								&outbuffer[0], 
								daq->Capabilities.OutputReportByteLength, 
								&numBytesReturned, 
								(LPOVERLAPPED) &daq->HidOverlapped);

				bResult = WaitForSingleObject(ReportEvent, 200);

				bResult = ReadFile(	daq->HidDevHandle,							
								&inbuffer[0],						
								daq->Capabilities.InputReportByteLength,
								&numBytesReturned,					
								(LPOVERLAPPED) &daq->HidOverlapped );

				bResult = WaitForSingleObject( 	daq->ReportEvent,300);	
				if(bResult == WAIT_TIMEOUT || bResult == WAIT_ABANDONED)
					CancelIo(&daq->HidDevHandle);

				if(bResult == WAIT_OBJECT_0)
				{
					MuestraIn[i]=(unsigned)inbuffer[2];
					if(i%5==0)
					SendMessage(hVelGraph, PBM_SETPOS,MuestraIn[i], 0);
					y = 355-MuestraIn[i];
					LineTo(daq->hdc,i+50,y);
					i++;					
				}
				cTimer.bandera=FALSE;
			}
			Sleep(0);
		}
	}
	daq->TMThreadActive = FALSE;
	FileSave(daq->hwnd,MuestraOut);
	FileSave(daq->hwnd,MuestraIn);
}

void PID(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	HWND			hVelGraph;
	unsigned long	numBytesReturned;
	unsigned char   outbuffer[9];		
	unsigned char	inbuffer[9];		
	short			temperature = 0;	
	BOOL			bResult;
	DWORD			Leght=0, Cuenta=0;
	unsigned int	e[3],MuestraIn,aux;
	unsigned char	u[3];
	double			m0,m1,m2;
	unsigned int	referencia, in;
	const double	Kp = 0.9308;
	const double	Ki = 1;
	const double	Kd = 0.12;
	CMsgTickTimer	cTimer( 500, "1/2 second is up" );
   
	daq->TMThreadActive = TRUE;	

	if(daq->HidAttached)
	{
		outbuffer[0]=0;
		e[1] = 0;
		e[2] = 0;
		u[1]=0;
		u[0]=0;

		hVelGraph = GetDlgItem (daq->hwnd, IDC_VELGRAPH);
		cTimer.Start();

		while(TRUE)
		{
			if(cTimer.bandera==TRUE){
				
				bResult = WriteFile(daq->HidDevHandle, 
								&outbuffer[0], 
								daq->Capabilities.OutputReportByteLength, 
								&numBytesReturned, 
								(LPOVERLAPPED) &daq->HidOverlapped);
		
				bResult = WaitForSingleObject(ReportEvent, 200);

				bResult = ReadFile(	daq->HidDevHandle,							
								&inbuffer[0],						
								daq->Capabilities.InputReportByteLength,	
								&numBytesReturned,					
								(LPOVERLAPPED) &daq->HidOverlapped );	

				bResult = WaitForSingleObject( 	daq->ReportEvent,300);
				if(bResult == WAIT_TIMEOUT || bResult == WAIT_ABANDONED)
					CancelIo(&daq->HidDevHandle);

				if(bResult == WAIT_OBJECT_0)
				{
					if(daq->referencia<0)
						referencia=256+daq->referencia;
					else
						referencia=daq->referencia;
					if(inbuffer[2]<0)
						in=256+inbuffer[2];
					else
						in=inbuffer[2];

					e[0]=(referencia-in);
					m0=(Kp+Ki+Kd)*e[0];
					m1=(Kp+(2*Kd))*e[1];
					m2= Kd*e[2];
					aux= (m0 - m1 + m2 + u[1]);
					if(aux>255){
						u[0]=255;
					}else{
						if (aux<0){
							u[0]=0;
						}else{
						u[0]=(unsigned char)aux;
						}
					}
					outbuffer[1] = u[0];
					outbuffer[2] = u[0];
					u[2]=u[1];
					u[1]=u[0];
					e[2]=e[1];
					e[1]=e[0];					
					SendMessage(hVelGraph, PBM_SETPOS,in, 0);

				}	
				cTimer.bandera=FALSE;
			}
			Sleep(0);
		}
	}
	daq->TMThreadActive = FALSE;
}


void WriteFeatureReport(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	unsigned char   FeatureBuffer[9];
	BOOL			bResult;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];

	daq->TMThreadActive = TRUE;

	if(daq->HidAttached)
	{
		FeatureBuffer[0] = 0;	
		FeatureBuffer[1] = 3;	
		FeatureBuffer[2] = 2;
		sprintf_s(msg,100,"\nsending Feature Report %X %X ",FeatureBuffer[1],FeatureBuffer[2]);
		WriteConsole(hStdOut,msg,29,&Cuenta,NULL);

		bResult = HidD_SetFeature
		(daq->HidDevHandle,
		FeatureBuffer,
		daq->Capabilities.FeatureReportByteLength);

		if (!bResult){		
			WriteConsole(hStdOut,_T("\nCan't write to device "),23,&Cuenta,NULL);
		}else{
			WriteConsole(hStdOut,_T("\nwrite OK "),9,&Cuenta,NULL);	
		}
	}
	daq->TMThreadActive = FALSE;
}

void ReadFeatureReport(LPVOID lpParameter)
{
	usb				*daq = (usb*) lpParameter;
	unsigned char	FeatureBuffer[9];
	BOOL			bResult;
	short			temperature = 0;
	DWORD	Leght=0, Cuenta=0;
	TCHAR msg[100];

	daq->TMThreadActive = TRUE;
	WriteConsole(hStdOut,_T("\nReading Feature Report "),24,&Cuenta,NULL);

	if(daq->HidAttached)
	{
		FeatureBuffer[0]=0;

			bResult = HidD_GetFeature
			(daq->HidDevHandle,
			FeatureBuffer,
			daq->Capabilities.FeatureReportByteLength);
	
			if(bResult)
			{				
				temperature = (short)FeatureBuffer[2] + ((short)FeatureBuffer[1] << 8);
				sprintf_s(msg,100,"\nse recibio %X ",temperature);
				WriteConsole(hStdOut,msg,16,&Cuenta,NULL);
			}

	}
	daq->TMThreadActive = FALSE;
}