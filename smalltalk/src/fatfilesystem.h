//
//  fatfilesystem.h
//  Smalltalk-80
//
//  Created by Dan Banay on 5/3/20.
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

#ifndef __FATFS__
#define __FATFS__

#include "filesystem.h"
#include "kernel.h"
#include <string>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <array>
#include <cassert>

#include <unistd.h>
#include <errno.h>

#include <circle/logger.h>
#include <fatfs/ff.h>

#define MAX_OPEN_FILES 64
#define NO_FIL ((FIL*) 0)

static const char FromKernel[] = "runtime";

class FatST80FileSystem: public IFileSystem
{
public:
    FatST80FileSystem()
    {
        FatST80FileSystem("/");
    }

    FatST80FileSystem(const std::string& root) : root_directory(root)
    {
        init();
    }

    void init() {
        for (int i=0; i<MAX_OPEN_FILES; i++) fdtofil[i] = NO_FIL;
        curfd = 0;
    }

    std::string path_for_file(const char *name)
    {
#if 0
        // own version of short file names...
        char c;
        int i = 0;
        std::string s = "";

        while (*name && *name != '.') {
          if (i<8) s += *name;
          name++;
          i++;
        }
        if (*name == '.') {
          i=0;
          while (*name) {
            if (i<4) s += *name;
            name++;
            i++;
          }
        }
        return root_directory + "/" + s;
#endif
        return root_directory + "/" + name;
    }

    int getfd()
    {
        int initial_curfd = curfd;
        while (curfd < MAX_OPEN_FILES)
        {
            if (fdtofil[curfd] == NO_FIL)
                return curfd++;
            else
                curfd += 1;
        }
        curfd = 0;
        while (curfd < initial_curfd)
        {
            if (fdtofil[curfd] == NO_FIL)
                return curfd++;
            else
                curfd += 1;
        }
        CLogger::Get ()->Write ("fatfs", LogDebug, "getfd: too many open files %d", MAX_OPEN_FILES);
        return -1;
    }

    // File oriented operations
    int open_file(const char *name)
    {
        // CLogger::Get ()->Write ("fatfs", LogDebug, "Entry");

        int fd;
        FIL* fp = (FIL*)malloc(sizeof(FIL));
        if (fp == 0) CLogger::Get ()->Write ("fatfs", LogDebug, "open: malloc failed");
        FRESULT fres;

        // CLogger::Get ()->Write ("fatfs", LogDebug, "Entry2");

        std::string path = path_for_file(name);
        // CLogger::Get ()->Write ("fatfs", LogDebug, "Opening file: ");
        // CLogger::Get ()->Write ("fatfs", LogDebug, (char *)path.c_str());
        // CLogger::Get ()->Write ("fatfs", LogDebug, "\r\n");

        fres = f_open(fp, path.c_str(), FA_READ|FA_WRITE);
        if (fres != FR_OK) {
            CLogger::Get ()->Write ("fatfs", LogDebug, "open failed: %s", (char *) name); 
            return -1; 
        }

        fd = getfd();
        fdtofil[fd] = fp;
        return fd;
    }

    int create_file(const char *name)
    {
        int fd;
        FIL* fp = (FIL*)malloc(sizeof(FIL));
        if (fp == 0) CLogger::Get ()->Write ("fatfs", LogDebug, "create: malloc failed");
        FRESULT fres;
        std::string path = path_for_file(name);
        // CLogger::Get ()->Write (FromKernel, LogDebug, "Creating file: ");
        // CLogger::Get ()->Write (FromKernel, LogDebug, (char *)name);
        // CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");
        fres = f_open(fp, path.c_str(), FA_READ|FA_WRITE|FA_CREATE_ALWAYS);
        if (fres != FR_OK) return -1;
        fd = getfd();
        fdtofil[fd] = fp;
        // char s[80];
        // sprintf(s, "fd = %d\r\n", fd);
        // CLogger::Get ()->Write (FromKernel, LogDebug, (char *)s);
        return fd;
    }
    
    int close_file(int file_handle)
    {
        // char s[80];
        // sprintf(s, "Closing file: %d\r\n", file_handle);
        // CLogger::Get ()->Write (FromKernel, LogDebug, (char *)s);
        if (fdtofil[file_handle] != NO_FIL) {
            f_close(fdtofil[file_handle]);
            free(fdtofil[file_handle]);
            fdtofil[file_handle] = NO_FIL;
            return 0;
        } else { return -1; }
    }

    int read(int file_handle, char *buffer, int bytes)
    {
        FRESULT fres;
        int bytes_read;

        if (fdtofil[file_handle] != NO_FIL) {
            fres = f_read(fdtofil[file_handle], buffer, bytes, (UINT*)&bytes_read);
            if (fres != FR_OK) return -1;
            return bytes_read;
        } else { return -1; }
    }

    int write(int file_handle, const char *buffer, int bytes)
    {
        FRESULT fres;
        int bytes_written;

        if (fdtofil[file_handle] != NO_FIL) {
            fres = f_write(fdtofil[file_handle], buffer, bytes, (UINT*)&bytes_written);
            if (fres != FR_OK) { CLogger::Get ()->Write (FromKernel, LogDebug, "write err!\r\n"); return -1; }
            return bytes_written;
        } else { return -1; }
    }

    bool truncate_to(int file_handle, int length)
    {
        FRESULT fres;
        
        if (fdtofil[file_handle] != NO_FIL) {
            fres = f_lseek(fdtofil[file_handle], length);
            if (fres != FR_OK) return false;
            fres = f_truncate(fdtofil[file_handle]);
            if (fres != FR_OK) return false;
            return true; // TODO: real position may be different?
        } else { return false; }

    }

    int file_size(int file_handle)
    {
        int fsize;
        
        if (fdtofil[file_handle] != NO_FIL) {
            fsize = f_size(fdtofil[file_handle]);
            // CLogger::Get ()->Write (FromKernel, LogDebug, "Checking file size fd %d = %08d\r\n", file_handle, fsize);
            return fsize; // TODO: real position may be different?
        } else { return -1; }
    }

    bool file_flush(int file_handle)
    {
#if 1
        FRESULT fres;

        if (fdtofil[file_handle] != NO_FIL) {
            fres = f_sync(fdtofil[file_handle]);
            if (fres != FR_OK) return false;
            return true;
        } else { return false; }
#else
        CLogger::Get ()->Write (FromKernel, LogDebug, "file_flush unimplemented!\r\n");
        return false;
#endif
    }
    
    bool is_directory(const char *name)
    {
        FILINFO fi;

        // CLogger::Get ()->Write (FromKernel, LogDebug, "is_directory: ");
        // CLogger::Get ()->Write (FromKernel, LogDebug, (char *)name);
        // CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");

        std::string path = path_for_file(name);
        if (f_stat(path.c_str(), &fi) != FR_OK)
            return false;
        return (fi.fattrib & AM_DIR) == AM_DIR;
    }

#if 1
    // Directory orientated operations
    int num_files(void)
    {
        DIR dir;
        FRESULT fr;
        FILINFO fi;
	int n=0;

        fr = f_opendir(&dir, (const char*)root_directory.c_str());

        while ((fr = f_readdir(&dir, &fi)) == FR_OK)
        {
            if (fi.fname[0] == '\0') break;

            if (fi.fname[0] != '.' && !is_directory(fi.fname))
		n++;
        }
        f_closedir(&dir);
        return n;
    }

    void *open_dir(void)
    {
        DIR *dir;
        FRESULT fr;

        dir = (DIR*)malloc(sizeof(DIR));
        fr = f_opendir(dir, (const char*)root_directory.c_str());
        if (fr == FR_OK)
            return (void *)dir;
        else
            return 0;
    }

    void close_dir(void *p)
    {
        f_closedir((DIR *)p);
        free(p);
    }

    void next_file(void *dir, char *s)
    {
        FRESULT fr;
        FILINFO fi;

        if ((fr = f_readdir((DIR *)dir, &fi)) == FR_OK) {
		// CLogger::Get ()->Write (FromKernel, LogDebug, "FN: ");
		// CLogger::Get ()->Write (FromKernel, LogDebug, fi.fname);
		// CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");
		strncpy(s, fi.fname, 12);
        } else {
		char s[80];
		sprintf(s, "readdir err: %d\r\n", fr);
		strncpy(s, "???", 12);
	}
    }
#endif

    void enumerate_files(const std::function <void (const char *) >& each)
    {
        DIR dir;
        FRESULT fr;
        FILINFO fi;

//            CLogger::Get ()->Write (FromKernel, LogDebug, "enumerate_files - opening directory: ");
//            CLogger::Get ()->Write (FromKernel, LogDebug, (char *)root_directory.c_str());
//            CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");

        fr = f_opendir(&dir, (const char*)root_directory.c_str());

        while ((fr = f_readdir(&dir, &fi)) == FR_OK)
        {
#if 0
            if (fi.lfname == 0) {
              CLogger::Get ()->Write (FromKernel, LogDebug, "null long name?\r\n"); continue;
            }
#endif
            // skip hidden files and ., and ..
//            CLogger::Get ()->Write (FromKernel, LogDebug, "enumerate_files: ");
//            CLogger::Get ()->Write (FromKernel, LogDebug, fi.fname);
//            CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");

            if (fi.fname[0] == '\0') break;

            if (fi.fname[0] != '.' && !is_directory(fi.fname))
            {
//            CLogger::Get ()->Write (FromKernel, LogDebug, "calling fn");
//            CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");
                each(fi.fname);
//            CLogger::Get ()->Write (FromKernel, LogDebug, "returned from fn");
//            CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");
            }
        }
        f_closedir(&dir);
//            CLogger::Get ()->Write (FromKernel, LogDebug, "enumerate_files end");
//            CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");
        return;
    }

    bool rename_file(const char *old_name, const char *new_name)
    {
        std::string old_path = path_for_file(old_name);
        std::string new_path = path_for_file(new_name);

            // CLogger::Get ()->Write (FromKernel, LogDebug, "rename_file: ");
            // CLogger::Get ()->Write (FromKernel, LogDebug, (char *)old_path.c_str());
            // CLogger::Get ()->Write (FromKernel, LogDebug, " -> ");
            // CLogger::Get ()->Write (FromKernel, LogDebug, (char *)new_path.c_str());
            // CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");

        return f_rename(old_path.c_str(), new_path.c_str()) == FR_OK;
        return false;
    }

    bool delete_file(const char* file_name)
    {
        std::string path = path_for_file(file_name);

            // CLogger::Get ()->Write (FromKernel, LogDebug, "delete_file: ");
            // CLogger::Get ()->Write (FromKernel, LogDebug, (char *)path.c_str());
            // CLogger::Get ()->Write (FromKernel, LogDebug, "\r\n");
        return (f_unlink(path.c_str()) == FR_OK);
    }

    int seek_to(int file_handle, int position)
    {
        FRESULT fres;

        if (fdtofil[file_handle] != NO_FIL) {
            fres = f_lseek(fdtofil[file_handle], position);
            if (fres != FR_OK) { CLogger::Get ()->Write (FromKernel, LogDebug, "f_lseek failed!\r\n"); return -1; }
            return position; // TODO: real position may be different?
        } else { CLogger::Get ()->Write (FromKernel, LogDebug, "fd not found!\r\n"); return -1; }
    }
    
    int tell(int file_handle)
    {
        if (fdtofil[file_handle] != NO_FIL) {
            return f_tell(fdtofil[file_handle]);
        } else { CLogger::Get ()->Write (FromKernel, LogDebug, "fd not found!\r\n"); return -1; }

        CLogger::Get ()->Write (FromKernel, LogDebug, "tell unimplemented!\r\n");
        return 0;
    }

    // Error handling
    const int last_error()
    {
        return errno;
    }

    const char *error_text(int code)
    {
        return strerror(code);
    }

    bool shutdown()
    {
        bool ok = true;
        for (int i = 0; i < MAX_OPEN_FILES; i++)
        {
            FIL *fil = fdtofil[i];
            if (fil != NO_FIL)
            {
                FRESULT fres = f_close(fil);  // f_close calls f_sync internally
                if (fres != FR_OK) ok = false;
                fdtofil[i] = NO_FIL;
            }
        }
        return ok;
    }

private:
    std::string root_directory;

    FIL *fdtofil[MAX_OPEN_FILES];
    int curfd;

};
#endif // __FATFS__
