/*  OnePAD - author: arcum42(@gmail.com)
 *  Copyright (C) 2009
 *
 *  Based on ZeroPAD, author zerofrog@gmail.com
 *  Copyright (C) 2006-2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

 /*
  * Theoretically, this header is for anything to do with keyboard input.
  * Pragmatically, event handing's going in here too.
  */
  
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "keyboard.h"

int FindKey(int key, int pad)
{
	for (int p = 0; p < MAX_SUB_KEYS; p++)
		for (int i = 0; i < MAX_KEYS; i++)
			if (key ==  get_key(PadEnum[pad][p], i)) return i;
	return -1;
}

char* KeysymToChar(int keysym)
{
 #ifdef __LINUX__
	return XKeysymToString(keysym);
#else
	LPWORD temp;

	ToAscii((UINT) keysym, NULL, NULL, temp, NULL);
	return (char*)temp;
	#endif
}

void PollForKeyboardInput(int pad)
{
 #ifdef __LINUX__
        PollForX11KeyboardInput(pad);
#endif
}

void SetAutoRepeat(bool autorep)
{
 #ifdef __LINUX__
    if (toggleAutoRepeat)
    {
        if (autorep)
            XAutoRepeatOn(GSdsp);
        else
            XAutoRepeatOff(GSdsp);
    }
#endif
}

#ifdef __LINUX__
static bool s_grab_input = false;
static bool s_Shift = false;
void AnalyzeKeyEvent(int pad, keyEvent &evt, int& keyPress, int& keyRelease)
{
	int i;
	KeySym key = (KeySym)evt.key;

	switch (evt.evt)
	{
		case KeyPress:
			// Shift F12 is not yet use by pcsx2. So keep it to grab/ungrab input
			// I found it very handy vs the automatic fullscreen detection
			// 1/ Does not need to detect full-screen
			// 2/ Can use a debugger in full-screen
			// 3/ Can grab input in window without the need of a pixelated full-screen
			if (key == XK_Shift_R || key == XK_Shift_L) s_Shift = true;
			if (key == XK_F12 && s_Shift) {
				if(!s_grab_input) {
					s_grab_input = true;
					XGrabPointer(GSdsp, GSwin, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, GSwin, None, CurrentTime);
					XGrabKeyboard(GSdsp, GSwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
				} else {
					s_grab_input = false;
					XUngrabPointer(GSdsp, CurrentTime);
					XUngrabKeyboard(GSdsp, CurrentTime);
				}
			}

			i = FindKey(key, pad);

			// Analog controls.
			if ((i > PAD_RY) && (i <= PAD_R_LEFT))
			{
				switch (i)
				{
					case PAD_R_LEFT:
					case PAD_R_UP:
					case PAD_L_LEFT:
					case PAD_L_UP:
						Analog::ConfigurePad(pad, Analog::AnalogToPad(i), DEF_VALUE);
						break;
					case PAD_R_RIGHT:
					case PAD_R_DOWN:
					case PAD_L_RIGHT:
					case PAD_L_DOWN:
						Analog::ConfigurePad(pad, Analog::AnalogToPad(i), -DEF_VALUE);
						break;
				}
				i += 0xff00;
			}

			if (i != -1)
			{
				clear_bit(keyRelease, i);
				set_bit(keyPress, i);
			}
			//PAD_LOG("Key pressed:%d\n", i);

			event.evt = KEYPRESS;
			event.key = key;
			break;

		case KeyRelease:
			if (key == XK_Shift_R || key == XK_Shift_L) s_Shift = false;

			i = FindKey(key, pad);

			// Analog Controls.
			if ((i > PAD_RY) && (i <= PAD_R_LEFT))
			{
				Analog::ResetPad(pad, Analog::AnalogToPad(i));
				i += 0xff00;
			}

			if (i != -1)
			{
				clear_bit(keyPress, i);
				set_bit(keyRelease, i);
			}

			event.evt = KEYRELEASE;
			event.key = key;
			break;

		case FocusIn:
			XAutoRepeatOff(GSdsp);
			break;

		case FocusOut:
			XAutoRepeatOn(GSdsp);
			break;
	}
}
void PollForX11KeyboardInput(int pad)
{
	keyEvent evt;
	XEvent E;
	int keyPress = 0, keyRelease = 0;

	// Keyboard input send by PCSX2
	pthread_mutex_lock(&mutex_KeyEvent);
	vector<keyEvent>::iterator it = ev_fifo.begin();
	while ( it != ev_fifo.end() ) {
		AnalyzeKeyEvent(pad, *it, keyPress, keyRelease);
		it++;
	}
	ev_fifo.clear();
	pthread_mutex_unlock(&mutex_KeyEvent);

	// keyboard input
	while (XPending(GSdsp) > 0)
	{
		XNextEvent(GSdsp, &E);
		evt.evt = E.type;
		evt.key = (int)XLookupKeysym((XKeyEvent *) & E, 0);
		AnalyzeKeyEvent(pad, evt, keyPress, keyRelease);
	}

	UpdateKeys(pad, keyPress, keyRelease);
}

bool PollX11Keyboard(char* &temp, u32 &pkey)
{
	GdkEvent *ev = gdk_event_get();

	if (ev != NULL)
	{
		if (ev->type == GDK_KEY_PRESS)
		{

			if (ev->key.keyval == GDK_Escape)
			{
				temp = "Unknown";
				pkey = NULL;
			}
			else
			{
				temp = KeysymToChar(ev->key.keyval);
				pkey = ev->key.keyval;
			}

			return true;
		}
	}

	return false;
}
#else
LRESULT WINAPI PADwndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int keyPress[2] = {0}, keyRelease[2] = {0};
	static bool lbutton = false, rbutton = false;

	switch (msg)
	{
		case WM_KEYDOWN:
			if (lParam & 0x40000000) return TRUE;

			for (int pad = 0; pad < 2; ++pad)
			{
				for (int i = 0; i < MAX_KEYS; i++)
				{
					if (wParam ==  get_key(pad, i))
					{
						set_bit(keyPress[pad], i);
						clear_bit(keyRelease[pad], i);
						break;
					}
				}
			}

			event.evt = KEYPRESS;
			event.key = wParam;
			break;

		case WM_KEYUP:
			for (int pad = 0; pad < 2; ++pad)
			{
				for (int i = 0; i < MAX_KEYS; i++)
				{
					if (wParam == get_key(pad,i))
					{
						set_bit(keyRelease[pad], i);
						clear_bit(keyPress[pad], i);
						break;
					}
				}
			}


			event.evt = KEYRELEASE;
			event.key = wParam;
			break;

		case WM_DESTROY:
		case WM_QUIT:
			event.evt = KEYPRESS;
			event.key = VK_ESCAPE;
			return GSwndProc(hWnd, msg, wParam, lParam);

		default:
			return GSwndProc(hWnd, msg, wParam, lParam);
	}

	for (int pad = 0; pad < 2; ++pad)
	{
		UpdateKeys(pad, keyPress[pad], keyRelease[pad]);
	}

	return TRUE;
}
#endif
