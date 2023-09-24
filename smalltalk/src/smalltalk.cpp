//
//  main.cpp
//  Smalltalk-80
//
//  Created by Dan Banay on 2/20/20.
//  Copyright © 2020 Dan Banay. All rights reserved.
//
//  MIT License
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <stdlib.h>
#include <time.h>
#include "smalltalk.h"
#include "interpreter.h"
#include "fatfilesystem.h"
#include "hal.h"
#include <queue>

#include "usb_hid_keys.h"


#define GERMAN_RPI400_KEYBOARD

typedef std::uint16_t Pixel;

static inline void expand_pixel(Pixel *destPixel, std::uint16_t srcWord, int srcBit)
{
    *destPixel = -((srcWord & (1<<srcBit) ) == 0);
}

    void VirtualMachine::set_input_semaphore(int semaphore)
    {
        input_semaphore = semaphore;
    }
    
    // the number of seconds since 00:00 in the morning of January 1, 1901
    std::uint32_t VirtualMachine::get_smalltalk_epoch_time()
    {
        // Seconds between 1/1/1901 00:00 and 1/1/1970 00:00

        const std::uint32_t TIME_OFFSET = 2177452800;
        time_t unix_epoch_time =  time(0);
        return (std::uint32_t) unix_epoch_time + TIME_OFFSET;
    }
    
    // the number of milliseconds since the millisecond clock was
    // last reset or rolled over (a 32-bit unsigned number)
    std::uint32_t VirtualMachine::get_msclock()
    {
        return CKernel::Get()->GetTicks();
    }
    
    void VirtualMachine::check_scheduled_semaphore()
    {
        if( scheduled_semaphore && (get_msclock() > scheduled_time))
        {
            interpreter.asynchronousSignal(scheduled_semaphore);
            scheduled_semaphore = 0;
        }
    }

    // Schedule a semaphore to be signaled at a time. Only one outstanding
    // request may be scheduled at anytime. When called any outstanding
    // request will be replaced (or canceled if semaphore is 0).
    // Will signal immediate if scheduled time has passed.
    void VirtualMachine::signal_at(int semaphore, std::uint32_t msClockTime)
    {
        scheduled_semaphore = semaphore;
        scheduled_time = msClockTime;
        if (semaphore)
        {
            // Just in case the time passed
            check_scheduled_semaphore();
        }
    }
    
    void VirtualMachine::update_mouse_cursor(const std::uint16_t* cursor_bits)
    { 
    }
    
    // Set the cursor image
    // (a 16 word form)
    void VirtualMachine::set_cursor_image(std::uint16_t *image)
    {
        for (int y=0; y<16; y++) {
            MyCursorSymbol[y] = *image++;
        }
    }
    
    // Set the mouse cursor location
    void VirtualMachine::set_cursor_location(int x, int y)
    {
        CKernel::Get()->SetMouseState(x+off_x, y+off_y); // add offset back to mouse position
    }
    
    void VirtualMachine::get_cursor_location(int *x, int *y)
    {
        int tx, ty; unsigned tb;
        CKernel::Get()->GetMouseState(&tx, &ty, &tb);

        if (tx<off_x) tx=off_x;
        if (tx>=off_x+display_width) tx=off_x+display_width-1;
        if (ty<off_y) ty=off_y;
        if (ty>=off_y+display_height) ty=off_y+display_height-1;

        *x = (tx-off_x); // / vm_options.display_scale;
        *y = (ty-off_y); // / vm_options.display_scale; 
    }
    
    void VirtualMachine::set_link_cursor(bool link)
    {
    }
    
    void VirtualMachine::initialize_texture()
    {
        // There may be many frames before Smalltalk renders, so initialize the screen texture
        // with something that looks like the Smalltalk desktop pattern
#if 0
        for(int h = 0; h < display_height; h++)
        {
            for(int i = 0; i < display_width; i++)
            {
                m_Screen.SetPixel(off_x+i, off_y+h, (h & 1) ^ (i & 1) ? BLACK_COLOR : NORMAL_COLOR);
            }
        }
#endif
    }
    
    bool VirtualMachine::set_display_size(int width, int height)
    {
        if (display_width != width || display_height != height)
        {
            display_width = width;
            display_height = height;
            dirty_rect.x = 0;
            dirty_rect.y = 0;
            dirty_rect.w = width;
            dirty_rect.h = height;
            
            if (!screen_initialized)
                screen_initialized = 1;
            
            initialize_texture();
        }
        return true;
    }
    
    void VirtualMachine::update_cursor(int m_x, int m_y)
    {
        int displayBitmap = interpreter.getDisplayBits(display_width, display_height);
        if (displayBitmap == 0) return; // bail

        static int prev_x = 0;
        static int prev_y = 0;

        int source_word_left = prev_x / 16;
        int source_word_right = (prev_x + 15) / 16;
        int display_width_words = (display_width + 15) / 16;

        int source_index = 0;
        std::uint16_t source_pixel_l = 0;
        std::uint16_t source_pixel_r = 0;
        std::uint16_t cursor_pixel_l = 0;
        std::uint16_t cursor_pixel_r = 0;

        int hoffset = m_x % 16;

        // restore prev background
        for (int v=0; v<16; v++) {
            if ((prev_y+v) >= display_height) continue;

            source_index = source_word_left + (prev_y+v) * display_width_words;

            source_pixel_l = interpreter.fetchWord_ofDisplayBits(source_index  , displayBitmap);
            for (int p = 0; p<16; p++) {
                    if ((source_word_left*16+(15-p)) >= display_width) continue;
                    if ((source_pixel_l & (1<<p)) != 0)
                        m_Screen.SetPixel(off_x+source_word_left*16+(15-p), off_y+prev_y+v, BLACK_COLOR);
                    else
                        m_Screen.SetPixel(off_x+source_word_left*16+(15-p), off_y+prev_y+v, NORMAL_COLOR);
            }

            if (source_word_left == source_word_right) continue;

            source_pixel_r = interpreter.fetchWord_ofDisplayBits(source_index+1, displayBitmap);
            for (int p = 0; p<16; p++) {
                    if ((source_word_right*16+(15-p)) >= display_width) continue;
                    if ((source_pixel_r & (1<<p)) != 0)
                        m_Screen.SetPixel(off_x+source_word_right*16+(15-p), off_y+prev_y+v, BLACK_COLOR);
                    else
                        m_Screen.SetPixel(off_x+source_word_right*16+(15-p), off_y+prev_y+v, NORMAL_COLOR);
            }
        }

        // draw mouse pointer... only
        for (int v=0; v<16; v++) {
            if (m_y+v  >= display_height) continue;
            for (int p = 0; p<16; p++) {
                    if (m_x+(15-p) >= display_width) continue;

                    if ((MyCursorSymbol[v] & (1<<p)) != 0) 
                        m_Screen.SetPixel(off_x+m_x+(15-p), off_y+m_y+v, CKernel::Get()->GetCursorColor());
            }
        }

        prev_x = m_x;
        prev_y = m_y;
    }

    void VirtualMachine::update_texture()
    {
        int source_word_left = dirty_rect.x / 16;
        int source_word_right = ( dirty_rect.x +  dirty_rect.w - 1) / 16;
        int display_width_words = (display_width + 15) / 16;
        
        int source_index_row = source_word_left + (dirty_rect.y * display_width_words);
        int update_word_width =  source_word_right - source_word_left + 1;
        
        // We transfer pixels in groups of WORDS from the display form,
        // so we need to set texture update rectangle so the left and right edges are on
        // a word boundary
        Rect update_rect;
        update_rect.x = source_word_left * 16;
        update_rect.y = dirty_rect.y;
        update_rect.w = update_word_width * 16;
        update_rect.h = dirty_rect.h;
        
        int displayBitmap = interpreter.getDisplayBits(display_width, display_height);
        
        if (displayBitmap == 0) return; // bail
        
        int xp = 0, yp = 0;
        yp = update_rect.y;

        for(int h = 0; h < update_rect.h; h++)
        {
            int source_index = source_index_row;

            xp = update_rect.x;
            yp = update_rect.y;

            for(int i = 0; i < update_word_width; i++)
            {
                if ((yp+h < display_height) && (yp >= 0)) {
                    std::uint16_t source_pixel = interpreter.fetchWord_ofDisplayBits(source_index, displayBitmap);
                    for(int p = 0; p<16; p++) {
                        if ((xp < display_width) && (xp >= 0)) {
                            if ((source_pixel & (1<<p)) != 0)
                                m_Screen.SetPixel(off_x+xp+(15-p), off_y+yp+h, BLACK_COLOR);
                            else
                                m_Screen.SetPixel(off_x+xp+(15-p), off_y+yp+h, NORMAL_COLOR);
                        }
                    }
                }
                xp += 16;
                source_index++;
            }
            xp = 0;
            source_index_row += display_width_words;
        }
    }

    
    void VirtualMachine::display_changed(int x, int y, int width, int height)
    {
        texture_needs_update = true;
        assert(x >= 0 && x < display_width);
        assert(y >= 0 && y < display_height);
        assert(x + width <= display_width);
        assert(y + height <= display_height);
        
        if ((&dirty_rect == 0) || dirty_rect.w == 0 || dirty_rect.h == 0)
        {
            dirty_rect.x = x;
            dirty_rect.y = y;
            dirty_rect.w = width;
            dirty_rect.h = height;
        }
        else
        {
            int x1,y1,x2,y2;
            x1 = (dirty_rect.x < x) ? dirty_rect.x : x;
            y1 = (dirty_rect.y < y) ? dirty_rect.y : y;

            x2 = ((dirty_rect.x+dirty_rect.w) > (x+width))  ? dirty_rect.x+dirty_rect.w : x+width;
            y2 = ((dirty_rect.y+dirty_rect.h) > (y+height)) ? dirty_rect.y+dirty_rect.h : y+height;

            dirty_rect.x = x1;
            dirty_rect.y = y1;
            dirty_rect.w = abs(x2-x1);
            dirty_rect.h = abs(y2-y1);
        }
    }
    
    void VirtualMachine::error(const char *message)
    {
        CLogger::Get ()->Write ("ERROR", LogDebug, message);
        abort();
    }
    
    //Input queue
    bool VirtualMachine::next_input_word(std::uint16_t *word)
    {
        if (input_queue.size() == 0)
            return false;
        
        *word = input_queue.front();
        input_queue.pop();
        
        return true;
    }
    
    // lifetime
    void VirtualMachine::signal_quit()
    {
        quit_signalled = true;
    }
    
    const char *VirtualMachine::get_image_name()
    {
        return image_name.c_str();
    }
    
    void VirtualMachine::set_image_name(const char *new_name)
    {
        image_name = new_name;
    }
    
    void VirtualMachine::exit_to_debugger()
    {
        abort();
    }
    
    bool VirtualMachine::init()
    {
        texture_needs_update = false;
        quit_signalled = false;
        return interpreter.init();
    }
    
    void VirtualMachine::queue_input_word(std::uint16_t word)
    {
        assert(input_semaphore);
        input_queue.push(word);
        interpreter.asynchronousSignal(input_semaphore);
    }
    
    void VirtualMachine::queue_input_word(std::uint16_t type, std::uint16_t parameter)
    {
        queue_input_word(((type & 0xf) << 12) | (parameter & 0xfff));
    }
    
    void VirtualMachine::queue_input_time_words()
    {
        std::uint32_t delta_time;
        std::uint32_t now = get_msclock();
        if (event_count++ == 0)
        {
            delta_time = 0;
        }
        else
        {
            delta_time = now - last_event_time;
        }
        
        if (delta_time <= 4095)
        {
            // can fit in 12 bits
            queue_input_word(0, delta_time);
        }
        else
        {
            std::uint32_t abs_time = get_smalltalk_epoch_time();
            // too large, use type 5 with absolute time
            queue_input_word(5, 0); // parameter is ignored
            queue_input_word((abs_time>>16) & 0xffff); // high word first
            queue_input_word(abs_time & 0xffff);  // low word next

        }
        
        last_event_time = now;
    }
   
    /*
     decoded keyboard:
     
     A decoded keyboard consists of some independent keys and some “meta" keys (shift and escape)
     that cannot be detected on their own, but that change the value of the other keys. The keys
     on a decoded keyboard only indicate their down transition, not their up transition.
     For a decoded keyboard, the full shifted and “controlled" ASCII should be used as a parameter
     and successive type 3 and 4 words should be produced for each keystroke.
      
     undecoded keyboard:
     
     (independent keys with up/down detection)
     On an undecoded keyboard, the standard keys produce parameters that are the ASCII code
     of the character on the keytop without shift or control information (i.e., the key with “A”
     on it produces the ASCII for  “a” and the key with “2” and “@“ on it produces the ASCII for “2”).
     */
    void VirtualMachine::handle_keyboard_event(int key)
    {
#ifdef GERMAN_RPI400_KEYBOARD
        static char scantoascii[128] = {  // only indexed with param, if param < 128
            0,   0,   0,   0,   'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
            'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'z', 'y', '1', '2',
            '3', '4', '5', '6', '7', '8', '9', '0',  13,  27,   8,  10,  32, '?', '`', 'u',
            '+', '#', '~', 'o', 'a', '^', ',', '.', '-', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
            'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 127, 'U', 'V', 'W',
            'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
            'n', 'o', 'p', 'q', '<', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2',
            '3', '4', '5', '6', '7', '8', '9', '!', '"', '@', '$', '%', '&', '/', '(', ')'
            // the repeated letters 'A' .. 'Z', 'a' .. 'z' digits and shifted digits
            // are only there to get better visual feedback for unexpected keycodes arriving
            // within this block are two relevant keys: '<' left of 'y' at scancode 0x64
            // and <DEL> forward delete at scancode 0x4c
            // the '~' after '#' at scancode 0x32 has not been pbserved yet with the German layout
        };
        // replaces invalid non-ASCII characters °, §, ß, ´ with the "other" valid
        // character on the same key ^, @, ?, `
        // unlauts are replaced with their base letter, but not ß to make shifted ß into ?
        //
        // TODO strange TAB key is listed with code 10 not 9, seems to select last input chunk, like <ESC> does
        //
        // still wrong:
        // FIXED < key --> black box
        // FIXED all Alt combinations
        // WORK AROUND after pressing Ctrl all input yields black boxes, Ctrl key remains pressed?
        //
        // first try wrong
        // y <--> z  ß --> -/_  ´ --> =/+  ^ --> `/~  ü --> [  + --> ]
        // ö --> ;/:  ä --> '/'  # --> \/|  < --> NUL  , --> ,/<  . --> ./>  - --> //?
        // TODO add support for German RPi keyboard with right Alt key for
        // brackets, braces etc. as printed on the keycaps

        // Map between a key scan code and it's shifted key value (if any)
        static char shift_map[128] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
            31, ' ', '!', '"', '\'', '$', '%', '&', '\'', '(',
            ')', '*', '*', ';', '_', ':', '/', '=', '!', '"',
            '@', '$', '%', '&', '/', '(', ')', ':', ';', '>',
            '=', '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F',
            'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
            '[', '\\', ']', '^', '_', '`', 'A', 'B', 'C', 'D',
            'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
            'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
            'Y', 'Z', '{', '|', '}', '~', 127
        };
#else
        /* static char scantoascii[256] = { */
        static char scantoascii[128] = {  // only indexed with param, if param < 128
            0,   0,   0,   0,   'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
            'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '1', '2',
            '3', '4', '5', '6', '7', '8', '9', '0',  13,  27,   8,  10,  32, '-', '=', '[',
            ']', '\\', '~', ';', '\'', '`', ',', '.', '/',  0
        };

        // TODO add support for German RPi keyboard with right Alt key for
        // brackets, braces etc. as printed on the keycaps

        // Map between a key scan code and it's shifted key value (if any)
        static char shift_map[128] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
            31, ' ', '!', '"', '#', '$', '%', '&', '\'', '(',
            ')', '*', '+', '<', '_', '>', '?', ')', '!', '@',
            '#', '$', '%', '^', '&', '*', '(', ':', ':', '<',
            '+', '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F',
            'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
            '{', '|', '}', '^', '_', '~', 'A', 'B', 'C', 'D',
            'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
            'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
            'Y', 'Z', '{', '|', '}', '~', 127
        };
#endif
        
        /* std::uint16_t type = 3; */
        // TODO how to distinguish between key repsses and releases? type == 3 versus type == 4
        std::uint16_t param = 0;
        /*
         left shift 136 right shift 137 control 138 alpha-lock 139
         backspace 8 tab 9 line feed 10 return 13 escape 27 space 32 delete 127
         */

        // My initial plan was to use go unencoded for everything, but
        // when I pressed shift 6, I saw a ~ appear!. It turns out the
        // ALTO keyboard has ~ above 6!
        // https://www.flickr.com/photos/walkingsf/31415192416
        
        /* if (((key >> 8) & 0xff == KEY_MOD_LCTRL) */ 
        /*  || ((key >> 8) & 0xff == KEY_MOD_RCTRL)) { */
        // NOTE the above has precedence errors: == evaluated before &
        /* if ((key >> 8) & 0xff & (KEY_MOD_LCTRL|KEY_MOD_RCTRL)) { */
        /*     param = 138; */
        /* } else { */
            switch (key & 0xff) {
                /* case KEY_LEFTCTRL: */
                /* case KEY_RIGHTCTRL: param = 138; break; */
                // FIXME the below keys only report presses, but are never "released"

                case KEY_CAPSLOCK: param = 139; capslock_down = !capslock_down; break;
                /* case KEY_DELETE:   param = 127; break; */
                default:
                    if ((key & 0xff) > 127)
                        return;  // Must be ascii
                                 // wrong: these are USB HID scan codes, not characters yet
                    param = key & 0x7f;
                    break;
            }
        /* } */
   
        if (param < 128)
        {
            /* if (type == 3) */
            {
                if ((key >> 8) & 0xff & (KEY_MOD_LMETA|KEY_MOD_RMETA))
                    return; // Ignore left/right GUI modifier key combinations

#ifdef GERMAN_RPI400_KEYBOARD
                if ((key >> 8) & 0xff & (KEY_MOD_LALT|KEY_MOD_RALT))
                {  // could limit this to MOD_RALT (AltGr) only, but MOD_LALT has no other uses
                    param = scantoascii[param];
                    switch (param) {
                        case '7': param = '{'; break;
                        case '8': param = '['; break;
                        case '9': param = ']'; break;
                        case '0': param = '}'; break;
                        case '?': param = '\\'; break;  // really ß (unshifted ?)
                        case 'q': param = '@'; break;
                        case '+': param = '~'; break;
                        case '<': param = '|'; break;
                        /* case 'd': param = 0x04; break;  // FIXME testing if Ctrl-D can be sent this way */
                        default: return; // Ignore
                    }
                    // would need to add SHIFT processing here,
                    // if there were any ALT+SHIFT combinations
                }
                else
#endif

                if ((key >> 8) & 0xff & (KEY_MOD_LCTRL|KEY_MOD_RCTRL)) {
                    param = scantoascii[param];
                    if ('a' <= param && param <= 'z')
                    {
                        // simulate CTRL down, letter down, letter up, CTRL up
                        queue_input_time_words();
                        queue_input_word(3, 138);
                        queue_input_word(3, param);
                        queue_input_word(4, param);
                        queue_input_word(4, 138);
                        return;  // do not report the letter key press and release again
                    }
                }
                else if ((key >> 8) & 0xff & (KEY_MOD_LSHIFT|KEY_MOD_RSHIFT)) {
                    param = shift_map[scantoascii[param]];
                }
                else {
                    param = scantoascii[param];
                }
                
                /*
                 For a decoded keyboard, the full shifted and “controlled" ASCII should be
                 used as a parameter and successive type 3 and 4 words should be produced for each keystroke.
                 */
                queue_input_time_words();
                queue_input_word(3, param);
                queue_input_word(4, param);
                // TODO maybe do not queue, if param == 0?
            }
        }
        else
        {
            // send undecoded
            queue_input_time_words();
            queue_input_word(capslock_down ? 3 : 4, param);
        }
    }
    
    void VirtualMachine::handle_mouse_button_event(unsigned buttons, int down)
    {
        // The bluebook got these wrong!
        const int RedButton =    130;  // select
        const int YellowButton = 129;  // doit etc.
        const int BlueButton =   128;  // frame, close
        
        unsigned mods = 0;
        int smalltalk_button = 0;
        int button_index;
        
        static unsigned button_down_mods[3] = {0}; // modifier state at button down

        switch(buttons)
        {
            case 1: smalltalk_button = RedButton;    button_index = 0; break;
            case 2: smalltalk_button = BlueButton;   button_index = 1; break;
            case 4: smalltalk_button = YellowButton; button_index = 2; break;
            default:
                return;
        }

        if (down)
        {
            // Save mod state when the button went down
            // when the button is released we will use these rather than active one
            mods = 0;
            button_down_mods[button_index] = mods;
        }
        else
            mods = button_down_mods[button_index];

        if (down)
        {
            queue_input_time_words();
            queue_input_word(3, smalltalk_button);
        }
        else if (down == 0)
        {
            button_down_mods[button_index] = 0;
            queue_input_time_words();
            queue_input_word(4, smalltalk_button);
        }
    }

    void VirtualMachine::handle_mouse_movement_event(int x, int y)
    {
        queue_input_time_words();
        queue_input_word(1, (std::uint16_t) x);
        queue_input_time_words();
        queue_input_word(1, (std::uint16_t) y);
    }

    
    void VirtualMachine::render()
    {
        if (screen_initialized)
        {
            if (texture_needs_update)
            {
                update_texture();
                texture_needs_update = false;
            }
            
            int mouseX, mouseY; unsigned mouseB;

            CKernel::Get()->GetMouseState(&mouseX, &mouseY, &mouseB);
            CKernel::Get()->GetMouseDevice()->UpdateCursor ();

            mouseX = mouseX - off_x;
            mouseY = mouseY - off_y; 

            mouseX = (mouseX >= display_width) ? display_width-1 : mouseX;
            mouseY = (mouseY >= display_height) ? display_height-1 : mouseY;
            mouseX = (mouseX < 0) ? 0 : mouseX;
            mouseY = (mouseY < 0) ? 0 : mouseY;

            update_cursor(mouseX, mouseY);

            dirty_rect.x = 0;
            dirty_rect.y = 0;
            dirty_rect.w = 0;
            dirty_rect.h = 0;
        }
    }
    
    void VirtualMachine::process_events()
    {
	int x, y; unsigned mouse_mb = 0;
	static unsigned mouse_oldmb = 0;

        if (!input_semaphore) return;

        unsigned keys[6]; 
        int nkeys;

        nkeys = CKernel::Get()->GetKeyboardState(keys);

        if (nkeys > 0) {
            KeyboardEvent key;
            for (int i=0; i<nkeys; i++) {
                handle_keyboard_event(keys[i]);
            }
        }

        CKernel::Get()->GetMouseState(&x, &y, &mouse_mb);
        x = x - off_x;
        y = y - off_y; 
            
        if (mouse_mb != mouse_oldmb) {
            int b;
            if ((mouse_mb & 1) != (mouse_oldmb & 1)) b=1;
            if ((mouse_mb & 2) != (mouse_oldmb & 2)) b=2;
            if ((mouse_mb & 4) != (mouse_oldmb & 4)) b=4;
            mouse_oldmb = mouse_mb;
            handle_mouse_button_event(b, mouse_mb & b);
        }

        CKernel::Get()->GetMouseDevice()->UpdateCursor ();
        handle_mouse_movement_event(x, y);
    }
    
    void VirtualMachine::run()
    {
        for(;;)
        {
            off_x = (m_Screen.GetWidth() - display_width)/2;
            if (off_x > display_width/2) off_x = 0;
            off_y = (m_Screen.GetHeight() - display_height)/2;
            if (off_y > display_height/2) off_y = 0;

            process_events();
 
            check_scheduled_semaphore();
            interpreter.checkLowMemoryConditions();

            for(int i = 0; i < vm_options.cycles_per_frame && !quit_signalled; i++)
            {
                interpreter.cycle();
            }

            if (quit_signalled) break;
            
            render();

#if 0
            if (!vm_options.vsync && vm_options.novsync_delay > 0)
                // Delay(vm_options.novsync_delay); // Don't kill CPU
#endif
        }
    }
