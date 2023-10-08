// kernel.cpp
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

#include "kernel.h"
#include <assert.h>
#include <circle/net/ntpdaemon.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/string.h>
#include <circle/util.h>
#include <smalltalk.h>

#define EXPAND_CHARACTERS


#define TIMER_TEST

#ifdef TIMER_TEST
#include <time.h>
#endif

// Network configuration
#define USE_DHCP

#ifndef USE_DHCP
static const u8 IPAddress[]      = {192, 168, 0, 250};
static const u8 NetMask[]        = {255, 255, 255, 0};
static const u8 DefaultGateway[] = {192, 168, 0, 1};
static const u8 DNSServer[]      = {192, 168, 0, 1};
#endif

// Time configuration
static const char NTPServer[]    = "pool.ntp.org";
static const int nTimeZone       = 0*60;           // minutes diff to UTC, ST-80 handles timezone in its Time class

#define PARTITION       "emmc1-1"

static const char ClearScreen[] = "\x1b[H\x1b[J\x1b[?25l";
static const char GoodByeMessage[] = "The world has come to an end.";

int losgehts = 0;

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
        m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel ()),
        m_USBHCI (&m_Interrupt, &m_Timer),
//      m_USBHCI (&m_Interrupt, &m_Timer, TRUE)         // TRUE: enable plug-and-play
#ifndef USE_DHCP
	m_Net (IPAddress, NetMask, DefaultGateway, DNSServer),
#endif
        m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_nPosX (0), m_nPosY (0),
	m_nBootMode (m_Options.GetBootMode ()),
	m_NTPSyncInterval (m_Options.GetNTPSyncIntervalMinutes ())
{
	s_pThis = this;

	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
	s_pThis = 0;
}

CKernel *CKernel::Get (void)
{
        return s_pThis;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

        	m_CursorType = m_Options.GetCursorType ();
	        m_CursorColor = m_Options.GetCursorColor ();

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

	if (m_NTPSyncInterval >= 0 && bOK)
	{
		bOK = m_Net.Initialize (FALSE);         // FALSE: do not wait for network to appear
        }

        if (bOK)
        {
                bOK = m_EMMC.Initialize ();
        }

	losgehts = 1;
	return bOK;
}

void CKernel::smalltalk (void)
{
	struct options vm_options;

	vm_options.snapshot_name = "snapshot.im";
	vm_options.root_directory = "/";
	vm_options.three_buttons = true;
	vm_options.vsync = false;
	vm_options.novsync_delay = m_Options.GetNoVSyncDelay();
		// Try -delay 8 arg if your CPU is unhappy
	vm_options.cycles_per_frame = m_Options.GetCyclesPerFrame();
		// 1800;
	vm_options.display_scale = 1;

	VirtualMachine *vm = new VirtualMachine(vm_options, m_Screen);

        m_Logger.Write (FromKernel, LogDebug, "VM init");
	if (vm->init())
	{
                m_Logger.Write (FromKernel, LogDebug, "VM run");
		vm->run();
	}
	else
	{
		m_Logger.Write (FromKernel, LogError, "VM failed to initialize (invalid/missing directory or snapshot?)");
	}
	delete vm;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	m_Timer.SetTimeZone (nTimeZone);

        CUSBKeyboardDevice *pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
        if (pKeyboard == 0)
        {
                m_Logger.Write (FromKernel, LogError, "Keyboard not found");

                return ShutdownHalt;
        }

	// pKeyboard->RegisterRemovedHandler (KeyboardRemovedHandler);
	m_pKeyboard = pKeyboard;
	// pKeyboard->RegisterShutdownHandler (ShutdownHandler);  // invoked on <Ctrl>+<Alt>+<Del>
        pKeyboard->RegisterKeyPressedHandler (KeyPressedHandlerStub);

	CMouseDevice *pMouse = (CMouseDevice *) m_DeviceNameService.GetDevice ("mouse1", FALSE);
	if (pMouse == 0)
	{
		m_Logger.Write (FromKernel, LogPanic, "Mouse not found");
	}
        m_pMouse = pMouse;

	m_Screen.Write (ClearScreen, sizeof ClearScreen-1);

	if (!pMouse->Setup (m_Screen.GetWidth (), m_Screen.GetHeight ()))
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot setup mouse");
	}

	m_nPosX = m_Screen.GetWidth () / 2;
	m_nPosY = m_Screen.GetHeight () / 2;

//	pMouse->SetCursor (m_nPosX, m_nPosY);
	pMouse->ShowCursor (FALSE);

	pMouse->RegisterEventHandler (MouseEventStub);

        CLogger::Get ()->Write ("runtime", LogDebug, "SD mount");

	FATFS m_FileSystem;

        if (f_mount (&m_FileSystem, "SD:", 1) != FR_OK)
        {
                CLogger::Get ()->Write ("ObjectMemory", LogDebug, "Cannot mount drive: %s", "sd:");
        }

#ifdef TIMER_TEST
	CLogger::Get ()->Write ("RTCTest", LogDebug, "*** Begin RTC Test");
	unsigned u1 = GetTicks();
	CLogger::Get ()->Write ("RTCTest", LogDebug, "GetTicks A: %u", u1);
	SleepMs(5000);
	unsigned u2 = GetTicks();
	CLogger::Get ()->Write ("RTCTest", LogDebug, "GetTicks B: %u", u2);
	SleepMs(5000);
	unsigned u3 = m_Timer.GetTime();
	CLogger::Get ()->Write ("RTCTest", LogDebug, "GetTime A: %u", u3);
	SleepMs(5000);
	unsigned u4 = m_Timer.GetTime();
	CLogger::Get ()->Write ("RTCTest", LogDebug, "GetTime B: %u", u4);
	SleepMs(5000);
	unsigned u5 = (unsigned) time(0);
	CLogger::Get ()->Write ("RTCTest", LogDebug, "libc time A: %u", u5);
	SleepMs(5000);
	unsigned u6 = (unsigned) time(0);
	CLogger::Get ()->Write ("RTCTest", LogDebug, "libc time B: %u", u6);
	CLogger::Get ()->Write ("RTCTest", LogDebug, "*** End RTC Test");
	SleepMs(5000);
#endif

	if (m_NTPSyncInterval >= 0)
	{
		new CNTPDaemon (NTPServer, &m_Net, 60 * m_NTPSyncInterval);
#ifdef TIMER_TEST
		CLogger::Get ()->Write ("RTCTest", LogDebug, "Click Left Mouse Button to launch smalltalk");
		while (!(m_nButtons & 0x01))
	       	{
			unsigned u7 = m_Timer.GetTime();
			CLogger::Get ()->Write ("RTCTest", LogDebug, "GetTime: %u", u7);
			unsigned u8 = (unsigned) time(0);
			CLogger::Get ()->Write ("RTCTest", LogDebug, "libc time: %u", u8);
			SleepMs(2500);
		}
#endif
	}
	m_Logger.Write (FromKernel, LogDebug, "Starting smalltalk");
	smalltalk();
	m_Logger.Write (FromKernel, LogDebug, "Ending smalltalk");

#if 1
	for (unsigned nCount = 0; m_ShutdownMode == ShutdownNone; nCount++)
	{
		pMouse->UpdateCursor ();
		pKeyboard->UpdateLEDs ();  // do we still need to update the Caps Lock LED in this stage?
		m_Screen.Rotor (0, nCount);
	}
#endif

	m_Screen.Write (ClearScreen, sizeof ClearScreen-1);
	m_Screen.Write(GoodByeMessage, sizeof GoodByeMessage-1);
	return m_ShutdownMode;
}

void CKernel::KeyPressedHandlerStub (const char *pString)
{
        assert (s_pThis != 0);
        // s_pThis->m_Screen.Write (pString, strlen (pString));
        // s_pThis->m_Logger.Write (FromKernel, LogDebug, "Keyboard: %s", pString);
        s_pThis->KeyPressedHandler (pString);
}

void CKernel::KeyPressedHandler (const char *pString)
{
	assert (s_pThis != 0);
	strcpy (m_CookedKeySeq, pString);
#ifdef LOG_KEYBOARD
#ifdef EXPAND_CHARACTERS
	while (*pString)
	{
		CString s;
		s.Format ("'%c' %d %02X\n", *pString, *pString, *pString);
		pString++;
		s_pThis->m_Screen.Write (s, strlen (s));
	}
#else
	s_pThis->m_Screen.Write (pString, strlen (pString));
#endif
#endif
}

void CKernel::MouseEventHandler (TMouseEvent Event, unsigned nButtons, unsigned nPosX, unsigned nPosY, int nWheelMove)
{
		m_nPosX = nPosX;
		m_nPosY = nPosY;

	switch (Event) {
    		case MouseEventMouseDown:
			if (nButtons & MOUSE_BUTTON_LEFT)
				m_nButtons = m_nButtons | 0x01;
			if (nButtons & MOUSE_BUTTON_MIDDLE)
				m_nButtons = m_nButtons | 0x02;
			if (nButtons & MOUSE_BUTTON_RIGHT)
				m_nButtons = m_nButtons | 0x04;
			break;
		case MouseEventMouseUp:
			if (nButtons & MOUSE_BUTTON_LEFT)
				m_nButtons = m_nButtons & ~0x01;
			if (nButtons & MOUSE_BUTTON_MIDDLE)
				m_nButtons = m_nButtons & ~0x02;
			if (nButtons & MOUSE_BUTTON_RIGHT)
				m_nButtons = m_nButtons & ~0x04;
			break;
		case MouseEventMouseMove:
		case MouseEventMouseWheel:  // TODO can we trigger scrolling with this somehow?
		case MouseEventUnknown:
			break;
        }
}

void CKernel::MouseEventStub (TMouseEvent Event, unsigned nButtons, unsigned nPosX, unsigned nPosY, int nWheelMove)
{
	assert (s_pThis != 0);
	s_pThis->MouseEventHandler (Event, nButtons, nPosX, nPosY, nWheelMove);
}

void CKernel::DrawLine (int nPosX1, int nPosY1, int nPosX2, int nPosY2, TScreenColor Color)
{
	// Bresenham algorithm

	int nDeltaX = nPosX2-nPosX1 >= 0 ? nPosX2-nPosX1 : nPosX1-nPosX2;
	int nSignX  = nPosX1 < nPosX2 ? 1 : -1;

	int nDeltaY = -(nPosY2-nPosY1 >= 0 ? nPosY2-nPosY1 : nPosY1-nPosY2);
	int nSignY  = nPosY1 < nPosY2 ? 1 : -1;

	int nError = nDeltaX + nDeltaY;

	while (1)
	{
		m_Screen.SetPixel ((unsigned) nPosX1, (unsigned) nPosY1, Color);

		if (   nPosX1 == nPosX2
		    && nPosY1 == nPosY2)
		{
			break;
		}

		int nError2 = nError + nError;

		if (nError2 > nDeltaY)
		{
			nError += nDeltaY;
			nPosX1 += nSignX;
		}

		if (nError2 < nDeltaX)
		{
			nError += nDeltaX;
			nPosY1 += nSignY;
		}
	}
}

CMouseDevice *CKernel::GetMouseDevice (void) {
   return m_pMouse;
}

void CKernel::GetMouseState (int *x, int *y, unsigned *buttons) {
   *x = m_nPosX;
   *y = m_nPosY;
   *buttons = m_nButtons;
}

unsigned CKernel::GetTicks (void) {
	unsigned t = m_Timer.GetClockTicks() / 1000;
	return t;
}

unsigned CKernel::GetEpochTime (void) {
	unsigned t = m_Timer.GetTime();
	return t;
}

extern "C" unsigned syscall_gettimeofday (void) {
	CKernel *ckp = CKernel::Get();
	if (ckp == 0)
		return 0;
	else
		return ckp->GetEpochTime();
}

void CKernel::GetCookedKeyboardKey (char *keySeq) {
	strcpy(keySeq, m_CookedKeySeq);
	m_CookedKeySeq[0] = 0;
}

void CKernel::UpdateKeyboardLEDs (void) {
	m_pKeyboard->UpdateLEDs();
}

void CKernel::SetMouseState (int x, int y) {
   m_nPosX = x;
   m_nPosY = y;
}

unsigned CKernel::GetCursorType (void) {
   return m_CursorType;
}

unsigned CKernel::GetCursorColor (void) {
   return m_CursorColor;
}

void CKernel::SleepMs (unsigned ms) {
	m_Scheduler.MsSleep(ms);
}

