//
// koptions.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2023  R. Stange <rsta2@o2online.de>
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
#include <circle/koptions.h>
#include <circle/logger.h>
#include <circle/util.h>
#include <circle/sysconfig.h>

#define INVALID_VALUE	((unsigned) -1)

CKernelOptions *CKernelOptions::s_pThis = 0;

CKernelOptions::CKernelOptions (void)
:	m_nWidth (0),
	m_nHeight (0),
	m_nLogLevel (LogDebug),
	m_nUSBPowerDelay (0),
	m_bUSBFullSpeed (FALSE),
	m_bUSBBoost (FALSE),
	m_USBSoundChannels {0, 0},
	m_nSoundOption (0),
	m_CPUSpeed (CPUSpeedLow),
	m_nSoCMaxTemp (60),
	m_nGPIOFanPin (0),
	m_bTouchScreenValid (FALSE),
	m_CursorType (0),
	m_nCursorColor (0),
	m_nBootMode (0),
	m_CyclesPerFrame (1800),
	m_NoVSyncDelay(0),
	m_NTPSyncIntervalMinutes(-1)
{
	strcpy (m_LogDevice, "tty1");
	strcpy (m_KeyMap, DEFAULT_KEYMAP);
	m_USBIgnore[0] = '\0';
	m_SoundDevice[0] = '\0';

	s_pThis = this;

	CBcmPropertyTags Tags;
	if (!Tags.GetTag (PROPTAG_GET_COMMAND_LINE, &m_TagCommandLine, sizeof m_TagCommandLine))
	{
		return;
	}

	if (m_TagCommandLine.Tag.nValueLength >= sizeof m_TagCommandLine.String)
	{
		return;
	}
	m_TagCommandLine.String[m_TagCommandLine.Tag.nValueLength] = '\0';
	
	m_pOptions = (char *) m_TagCommandLine.String;

	char *pOption;
	while ((pOption = GetToken ()) != 0)
	{
		char *pValue = GetOptionValue (pOption);

		if (strcmp (pOption, "width") == 0)
		{
			unsigned nValue;
			if ((nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 640 <= nValue && nValue <= 1920)
			{
				m_nWidth = nValue;
			}
		}
		else if (strcmp (pOption, "height") == 0)
		{
			unsigned nValue;
			if ((nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 480 <= nValue && nValue <= 1080)
			{
				m_nHeight = nValue;
			}
		}
		else if (strcmp (pOption, "logdev") == 0)
		{
			strncpy (m_LogDevice, pValue, sizeof m_LogDevice-1);
			m_LogDevice[sizeof m_LogDevice-1] = '\0';
		}
		else if (strcmp (pOption, "loglevel") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && nValue <= LogDebug)
			{
				m_nLogLevel = nValue;
			}
		}
		else if (strcmp (pOption, "keymap") == 0)
		{
			strncpy (m_KeyMap, pValue, sizeof m_KeyMap-1);
			m_KeyMap[sizeof m_KeyMap-1] = '\0';
		}
		else if (strcmp (pOption, "usbpowerdelay") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 200 <= nValue && nValue <= 8000)
			{
				m_nUSBPowerDelay = nValue;
			}
		}
		else if (strcmp (pOption, "usbspeed") == 0)
		{
			if (strcmp (pValue, "full") == 0)
			{
				m_bUSBFullSpeed = TRUE;
			}
		}
		else if (strcmp (pOption, "usbboost") == 0)
		{
			if (strcmp (pValue, "true") == 0)
			{
				m_bUSBBoost = TRUE;
			}
		}
		else if (strcmp (pOption, "usbignore") == 0)
		{
			strncpy (m_USBIgnore, pValue, sizeof m_USBIgnore-1);
			m_USBIgnore[sizeof m_USBIgnore-1] = '\0';
		}
		else if (strcmp (pOption, "usbsoundchannels") == 0)
		{
			if (!GetDecimals (pValue, m_USBSoundChannels, 2))
			{
				m_USBSoundChannels[0] = 0;
				m_USBSoundChannels[1] = 0;
			}
		}
		else if (strcmp (pOption, "sounddev") == 0)
		{
			strncpy (m_SoundDevice, pValue, sizeof m_SoundDevice-1);
			m_SoundDevice[sizeof m_SoundDevice-1] = '\0';
		}
		else if (strcmp (pOption, "soundopt") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && (nValue <= 2 || nValue == 16 || nValue == 24))
			{
				m_nSoundOption = nValue;
			}
		}
		else if (strcmp (pOption, "fast") == 0)
		{
			if (strcmp (pValue, "true") == 0)
			{
				m_CPUSpeed = CPUSpeedMaximum;
			}
		}
		else if (strcmp (pOption, "socmaxtemp") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 40 <= nValue && nValue <= 78)
			{
				m_nSoCMaxTemp = nValue;
			}
		}
		else if (strcmp (pOption, "gpiofanpin") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 2 <= nValue && nValue <= 27)
			{
				m_nGPIOFanPin = nValue;
			}
		}
		else if (strcmp (pOption, "touchscreen") == 0)
		{
			m_bTouchScreenValid = GetDecimals (pValue, m_TouchScreen, 4);
		}

		else if (strcmp (pOption, "cursortype") == 0)
		{
			if (strncmp(pValue, "hw", sizeof "hw") == 0)
				m_CursorType = 1; // hardware cursor
			else
				m_CursorType = 0; // software cursor
		}
		else if (strcmp (pOption, "cursorcolor") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetHex (pValue)) != INVALID_VALUE)
			{
				m_nCursorColor = nValue;
			} else 
				m_nCursorColor = 0;
		}
		else if (strcmp (pOption, "bootmode") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 0 <= nValue && nValue <= 2)
			{
				m_nBootMode = nValue;
			}
		}
		else if (strcmp (pOption, "cycles") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 100 <= nValue && nValue <= 20000)  // default is 1800, provide generous limits for variations
			{
				m_CyclesPerFrame = nValue;
			}
		}
		else if (strcmp (pOption, "nvdelay") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 0 <= nValue && nValue <= 1000)  // default is 0, with 8 (ms) as a recommended alternative
			{
				m_NoVSyncDelay = nValue;
			}
		}
		else if (strcmp (pOption, "ntp") == 0)
		{
			unsigned nValue;
			if (   (nValue = GetDecimal (pValue)) != INVALID_VALUE
			    && 0 <= nValue && nValue <= 1440)  // default is -1 (for off), 0 would be just once (after start up), 1440 would be once per day (24h)
			{
				m_NTPSyncIntervalMinutes = nValue;
			}
		}
	}
}

CKernelOptions::~CKernelOptions (void)
{
	s_pThis = 0;
}

unsigned CKernelOptions::GetWidth (void) const
{
	return m_nWidth;
}

unsigned CKernelOptions::GetHeight (void) const
{
	return m_nHeight;
}

const char *CKernelOptions::GetLogDevice (void) const
{
	return m_LogDevice;
}

unsigned CKernelOptions::GetLogLevel (void) const
{
	return m_nLogLevel;
}

const char *CKernelOptions::GetKeyMap (void) const
{
	return m_KeyMap;
}

unsigned CKernelOptions::GetUSBPowerDelay (void) const
{
	return m_nUSBPowerDelay;
}

boolean CKernelOptions::GetUSBFullSpeed (void) const
{
	return m_bUSBFullSpeed;
}

boolean CKernelOptions::GetUSBBoost (void) const
{
	return m_bUSBBoost;
}

const char *CKernelOptions::GetUSBIgnore (void) const
{
	return m_USBIgnore;
}

const unsigned *CKernelOptions::GetUSBSoundChannels (void) const
{
	return m_USBSoundChannels;
}

const char *CKernelOptions::GetSoundDevice (void) const
{
	return m_SoundDevice;
}

unsigned CKernelOptions::GetSoundOption (void) const
{
	return m_nSoundOption;
}

TCPUSpeed CKernelOptions::GetCPUSpeed (void) const
{
	return m_CPUSpeed;
}

unsigned CKernelOptions::GetSoCMaxTemp (void) const
{
	return m_nSoCMaxTemp;
}

unsigned CKernelOptions::GetGPIOFanPin (void) const
{
	return m_nGPIOFanPin;
}

const unsigned *CKernelOptions::GetTouchScreen (void) const
{
	return m_bTouchScreenValid ? m_TouchScreen : nullptr;
}

unsigned CKernelOptions::GetCursorType (void) const
{
	return m_CursorType;
}

unsigned CKernelOptions::GetCursorColor (void) const
{
	return m_nCursorColor;
}

unsigned CKernelOptions::GetBootMode (void) const
{
	return m_nBootMode;
}

unsigned CKernelOptions::GetCyclesPerFrame (void) const
{
	return m_CyclesPerFrame;
}

unsigned CKernelOptions::GetNoVSyncDelay (void) const
{
	return m_NoVSyncDelay;
}

int CKernelOptions::GetNTPSyncIntervalMinutes (void) const
{
	return m_NTPSyncIntervalMinutes;
}

CKernelOptions *CKernelOptions::Get (void)
{
	return s_pThis;
}

char *CKernelOptions::GetToken (void)
{
	while (*m_pOptions != '\0')
	{
		if (*m_pOptions != ' ')
		{
			break;
		}

		m_pOptions++;
	}

	if (*m_pOptions == '\0')
	{
		return 0;
	}

	char *pToken = m_pOptions;

	while (*m_pOptions != '\0')
	{
		if (*m_pOptions == ' ')
		{
			*m_pOptions++ = '\0';

			break;
		}

		m_pOptions++;
	}

	return pToken;
}

char *CKernelOptions::GetOptionValue (char *pOption)
{
	while (*pOption != '\0')
	{
		if (*pOption == '=')
		{
			break;
		}

		pOption++;
	}

	if (*pOption == '\0')
	{
		return 0;
	}

	*pOption++ = '\0';

	return pOption;
}

unsigned CKernelOptions::GetDecimal (char *pString)
{
	if (   pString == 0
	    || *pString == '\0')
	{
		return INVALID_VALUE;
	}

	unsigned nResult = 0;

	char chChar;
	while ((chChar = *pString++) != '\0')
	{
		if (!('0' <= chChar && chChar <= '9'))
		{
			return INVALID_VALUE;
		}

		unsigned nPrevResult = nResult;

		nResult = nResult * 10 + (chChar - '0');
		if (   nResult < nPrevResult
		    || nResult == INVALID_VALUE)
		{
			return INVALID_VALUE;
		}
	}

	return nResult;
}

boolean CKernelOptions::GetDecimals (char *pString, unsigned *pResult, unsigned nCount)
{
	static const char Delim[] = ",";

	char *pSavePtr;
	while (nCount--)
	{
		char *pToken = strtok_r (pString, Delim, &pSavePtr);
		if (!pToken)
		{
			return FALSE;
		}

		unsigned nValue = GetDecimal (pToken);
		if (nValue == INVALID_VALUE)
		{
			return FALSE;
		}

		*pResult++ = nValue;

		pString = nullptr;
	}

	return !strtok_r (nullptr, Delim, &pSavePtr);
}

unsigned CKernelOptions::GetHex (char *pString)
{
	if (   pString == 0
	    || *pString == '\0')
	{
		return INVALID_VALUE;
	}

	unsigned nResult = 0;

	char chChar;
	while ((chChar = *pString++) != '\0')
	{
		if (!(('0' <= chChar && chChar <= '9') || ('a' <= chChar && chChar <= 'f') || ('A' <= chChar && chChar <= 'F')))
		{
			return INVALID_VALUE;
		}

		unsigned nPrevResult = nResult;

		if (chChar >= '0' && chChar <= '9') nResult = nResult * 16 + (chChar - '0');
		if (chChar >= 'a' && chChar <= 'f') nResult = nResult * 16 + (chChar - 'a' + 10);
		if (chChar >= 'A' && chChar <= 'F') nResult = nResult * 16 + (chChar - 'A' + 10);

		if (   nResult < nPrevResult
		    || nResult == INVALID_VALUE)
		{
			return INVALID_VALUE;
		}
	}

	return nResult;
}
