//
// kernel.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2018  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/memory.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
#include <circle/input/mouse.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <circle/types.h>

#include <circle/usb/usbkeyboard.h>  // required by cooked keyboard handling

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);

	TShutdownMode Run (void);

	static CKernel *Get (void);
	CMouseDevice *GetMouseDevice (void);

        void GetMouseState (int *x, int *y, unsigned *buttons);
	void GetCookedKeyboardKey (char *keySeq);
	void UpdateKeyboardLEDs (void);
	unsigned GetTicks (void);
	unsigned GetEpochTime (void);

        void SetMouseState (int x, int y);

	unsigned GetCursorType (void);
	unsigned GetCursorColor (void);

	void SleepMs (unsigned);

private:
        static void KeyPressedHandlerStub (const char *pString);
        void KeyPressedHandler (const char *pString);

	static void MouseEventStub (TMouseEvent Event, unsigned nButtons, unsigned nPosX, unsigned nPosY, int nWheelMove);
	void MouseEventHandler (TMouseEvent Event, unsigned nButtons, unsigned nPosX, unsigned nPosY, int nWheelMove);

	void DrawLine (int nPosX1, int nPosY1, int nPosX2, int nPosY2, TScreenColor Color);

	void smalltalk(void);

private:
	// do not change this order
	CMemorySystem		m_Memory;
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CScreenDevice		m_Screen;
	CSerialDevice		m_Serial;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CUSBHCIDevice		m_USBHCI;
	CScheduler              m_Scheduler;
	CNetSubSystem           m_Net;
        CEMMCDevice             m_EMMC;
        FATFS                   m_FileSystem;

	int m_nPosX = 0;
	int m_nPosY = 0;
	unsigned m_nButtons = 0;
	CMouseDevice *m_pMouse;

	unsigned m_CursorType = 0;
	unsigned m_CursorColor = 0;

	unsigned m_nBootMode = 0;
	int m_NTPSyncInterval = -1;

	CUSBKeyboardDevice *m_pKeyboard;
	char m_CookedKeySeq[6] = {0};  // special keys report sequences of up to 6 characters like <ESC>[12~<NUL>

	volatile TShutdownMode m_ShutdownMode;

	static CKernel *s_pThis;
};

#endif
