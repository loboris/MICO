/**
 * file.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"

#include "platform.h"
#include "mico_platform.h"
#include "user_config.h"
#include "CheckSumUtils.h"
#include "StringUtils.h"

#include <spiffs.h>
#include <spiffs_nucleus.h>

extern void luaWdgReload( void );

#ifdef LOBO_SPIFFS_DBG
// Printing debug info, can be combined:
// 0x01 - print write info
// 0x10 - check written page
// 0x02 - print erase info
// 0x20 - check erase block
// 0x04 - print gc info
// 0x08 - print check info
// 0x40 - print general debug info
// 0x80 - print cache info
uint8_t spiffs_debug = 0;
#endif

#define LOG_BLOCK_SIZE_K 16
#define LOG_PAGE_SIZE    128
#define LOG_BLOCK_SIZE   LOG_BLOCK_SIZE_K*1024
#define PHYS_ERASE_BLOCK LOG_BLOCK_SIZE
#define MAX_DIR_DEPTH    5
#define FILE_NOT_OPENED  0
#define MAX_FILE_FD      5

static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];
static u8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*4];

spiffs fs;
static volatile spiffs_file file_fd[MAX_FILE_FD] = { FILE_NOT_OPENED };

char curr_dir[SPIFFS_OBJ_NAME_LEN] = { 0 };
static int *fs_check = NULL;

// Y-MODEM defines
#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3)
#define PACKET_TRAILER          (2)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)

#define FILE_SIZE_LENGTH        (16)

#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CA                      (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41)  /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61)  /* 'a' == 0x61, abort by user */

#define NAK_TIMEOUT             (1000)
#define MAX_ERRORS              (45)

// fnmatch defines
#define	FNM_NOMATCH	1	// Match failed.

#define	FNM_NOESCAPE	0x01	// Disable backslash escaping.
#define	FNM_PATHNAME	0x02	// Slash must be matched by slash.
#define	FNM_PERIOD	0x04	// Period must be matched by period.
#define	FNM_LEADING_DIR	0x08	// Ignore /<tail> after Imatch.
#define	FNM_CASEFOLD	0x10	// Case insensitive search.
#define FNM_PREFIX_DIRS	0x20	// Directory prefixes of pattern match too.

#define	EOS	        '\0'

//-----------------------------------------------------------------------
static const char * rangematch(const char *pattern, char test, int flags)
{
  int negate, ok;
  char c, c2;

  /*
   * A bracket expression starting with an unquoted circumflex
   * character produces unspecified results (IEEE 1003.2-1992,
   * 3.13.2).  This implementation treats it like '!', for
   * consistency with the regular expression syntax.
   * J.T. Conklin (conklin@ngai.kaleida.com)
   */
  if ( (negate = (*pattern == '!' || *pattern == '^')) ) ++pattern;

  if (flags & FNM_CASEFOLD) test = tolower((unsigned char)test);

  for (ok = 0; (c = *pattern++) != ']';) {
    if (c == '\\' && !(flags & FNM_NOESCAPE)) c = *pattern++;
    if (c == EOS) return (NULL);

    if (flags & FNM_CASEFOLD) c = tolower((unsigned char)c);

    if (*pattern == '-' && (c2 = *(pattern+1)) != EOS && c2 != ']') {
      pattern += 2;
      if (c2 == '\\' && !(flags & FNM_NOESCAPE)) c2 = *pattern++;
      if (c2 == EOS) return (NULL);

      if (flags & FNM_CASEFOLD) c2 = tolower((unsigned char)c2);

      if ((unsigned char)c <= (unsigned char)test &&
          (unsigned char)test <= (unsigned char)c2) ok = 1;
    }
    else if (c == test) ok = 1;
  }
  return (ok == negate ? NULL : pattern);
}

//-------------------------------------------------------------
int fnmatch(const char *pattern, const char *string, int flags)
{
  const char *stringstart;
  char c, test;

  for (stringstart = string;;)
    switch (c = *pattern++) {
    case EOS:
      if ((flags & FNM_LEADING_DIR) && *string == '/') return (0);
      return (*string == EOS ? 0 : FNM_NOMATCH);
    case '?':
      if (*string == EOS) return (FNM_NOMATCH);
      if (*string == '/' && (flags & FNM_PATHNAME)) return (FNM_NOMATCH);
      if (*string == '.' && (flags & FNM_PERIOD) &&
          (string == stringstart ||
          ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
              return (FNM_NOMATCH);
      ++string;
      break;
    case '*':
      c = *pattern;
      // Collapse multiple stars.
      while (c == '*') c = *++pattern;

      if (*string == '.' && (flags & FNM_PERIOD) &&
          (string == stringstart ||
          ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
              return (FNM_NOMATCH);

      // Optimize for pattern with * at end or before /.
      if (c == EOS)
        if (flags & FNM_PATHNAME)
          return ((flags & FNM_LEADING_DIR) ||
                    strchr(string, '/') == NULL ?
                    0 : FNM_NOMATCH);
        else return (0);
      else if (c == '/' && flags & FNM_PATHNAME) {
        if ((string = strchr(string, '/')) == NULL) return (FNM_NOMATCH);
        break;
      }

      // General case, use recursion.
      while ((test = *string) != EOS) {
        if (!fnmatch(pattern, string, flags & ~FNM_PERIOD)) return (0);
        if (test == '/' && flags & FNM_PATHNAME) break;
        ++string;
      }
      return (FNM_NOMATCH);
    case '[':
      if (*string == EOS) return (FNM_NOMATCH);
      if (*string == '/' && flags & FNM_PATHNAME) return (FNM_NOMATCH);
      if ((pattern = rangematch(pattern, *string, flags)) == NULL) return (FNM_NOMATCH);
      ++string;
      break;
    case '\\':
      if (!(flags & FNM_NOESCAPE)) {
        if ((c = *pattern++) == EOS) {
          c = '\\';
          --pattern;
        }
      }
      // FALLTHROUGH
    default:
      if (c == *string)
              ;
      else if ((flags & FNM_CASEFOLD) &&
               (tolower((unsigned char)c) ==
                tolower((unsigned char)*string)))
              ;
      else if ((flags & FNM_PREFIX_DIRS) && *string == EOS &&
           ((c == '/' && string != stringstart) ||
           (string == stringstart+1 && *stringstart == '/')))
              return (0);
      else
              return (FNM_NOMATCH);
      string++;
      break;
    }
  // NOTREACHED
}

//------------------------------------------------------------
static s32_t lspiffs_read(u32_t addr, u32_t size, u8_t *dst) {
  u32_t adr = addr;
  MicoFlashRead(MICO_PARTITION_LUA, &adr,dst,size);
  luaWdgReload();
  return SPIFFS_OK;
}

//-------------------------------------------------------------
static s32_t lspiffs_write(u32_t addr, u32_t size, u8_t *src) {
  uint32_t adr = addr;
  if (size > 0) {
    MicoFlashWrite(MICO_PARTITION_LUA,&adr,src,size);
#ifdef LOBO_SPIFFS_DBG
    if (spiffs_debug & 0x10) {
      // read back the written page and check
      int cmp = 0;
      uint8_t* bb = calloc(size, sizeof(uint8_t));
      adr = addr;
      MicoFlashRead(MICO_PARTITION_LUA, &adr,bb,size);
      if (size == 1) {
        if (src[0] != bb[0]) cmp = 1;
      }
      else cmp = memcmp(src, bb, size);
      free(bb);
      printf("WRITE: %06x, %d (%d)\r\n", addr, size, cmp);
    }
    else if (spiffs_debug & 0x01) {
      printf("WRITE: %06x, %d\r\n", addr, size);
    }
#endif
    luaWdgReload();
  }
  return SPIFFS_OK;
}

//--------------------------------------------------
static s32_t lspiffs_erase(u32_t addr, u32_t size) {
  uint32_t adr = addr;
  
#ifdef LOBO_SPIFFS_DBG
  uint32_t tmo = 0;

  if (spiffs_debug & 0x22) {
    printf("ERASING %06x bytes at %06x", size, addr);
    tmo = mico_get_time();
  }
#endif
  int err = MicoFlashErase(MICO_PARTITION_LUA, adr, size);
  if (!err) {
#ifdef LOBO_SPIFFS_DBG
    if (spiffs_debug & 0x20) {
      // test 4K blocks
      uint8_t* bb = calloc(4096, sizeof(uint8_t));
      int cmp = 0;
      for (uint8_t n=0;n<(LOG_BLOCK_SIZE_K/4);n++) {
        adr = addr + (buf_size*n);
        MicoFlashRead(MICO_PARTITION_LUA, &adr, bb, 4096);
        for (int i=0;i<4096;i++) {
          if (bb[i] != 0xFF) {
            cmp++;
            break;
          }
        }
        if (cmp > 0) break;
      }
      free(bb);
      printf(" res=%d time=%d ms\r\n", cmp, (mico_get_time()-tmo));
      mico_thread_msleep(2);
    }
    else if (spiffs_debug & 0x02) printf(" time=%d ms\r\n", (mico_get_time()-tmo));
#endif
  }
#ifdef LOBO_SPIFFS_DBG
  else if (spiffs_debug & 2) printf(" ERROR: %d\r\n", err);
#endif
  luaWdgReload();

  return SPIFFS_OK;
} 

//-----------------------------------------------------------------------
void lspiffs_check_cb(spiffs_check_type type, spiffs_check_report report,
                      u32_t arg1, u32_t arg2)
{
  if (fs_check == NULL) return;
  
  if (report == SPIFFS_CHECK_PROGRESS) fs_check[SPIFFS_CHECK_PROGRESS]++; 
  else if (report == SPIFFS_CHECK_ERROR) fs_check[SPIFFS_CHECK_ERROR]++;
  else if (report == SPIFFS_CHECK_FIX_INDEX) fs_check[SPIFFS_CHECK_FIX_INDEX]++;
  else if (report == SPIFFS_CHECK_FIX_LOOKUP) fs_check[SPIFFS_CHECK_FIX_LOOKUP]++;
  else if (report == SPIFFS_CHECK_DELETE_ORPHANED_INDEX) fs_check[SPIFFS_CHECK_DELETE_ORPHANED_INDEX]++;
  else if (report == SPIFFS_CHECK_DELETE_PAGE) fs_check[SPIFFS_CHECK_DELETE_PAGE]++;
  else if (report == SPIFFS_CHECK_DELETE_BAD_FILE) fs_check[SPIFFS_CHECK_DELETE_BAD_FILE]++;
}

//-------------------------
int mode2flag(char *mode) {
  if (strlen(mode)==1) {
    if (strcmp(mode,"w")==0)
      return SPIFFS_WRONLY|SPIFFS_CREAT|SPIFFS_TRUNC;
    else if (strcmp(mode, "r")==0)
      return SPIFFS_RDONLY;
    else if (strcmp(mode, "a")==0)
      return SPIFFS_WRONLY|SPIFFS_CREAT|SPIFFS_APPEND;
    else
      return SPIFFS_RDONLY;
  }
  else if (strlen(mode)==2) {
    if (strcmp(mode,"r+")==0)
      return SPIFFS_RDWR;
    else if (strcmp(mode, "w+")==0)
      return SPIFFS_RDWR|SPIFFS_CREAT|SPIFFS_TRUNC;
    else if (strcmp(mode, "a+")==0)
      return SPIFFS_RDWR|SPIFFS_CREAT|SPIFFS_APPEND;
    else
      return SPIFFS_RDONLY;
  }
  else return SPIFFS_RDONLY;
}

//---------------
void _mount(void)
{
  mico_logic_partition_t* part;
  spiffs_config cfg;    

  part = MicoFlashGetInfo( MICO_PARTITION_LUA );
     
  cfg.phys_size = part->partition_length;
  cfg.phys_addr = 0;                        // start at beginning of the LUA partition
  cfg.log_block_size = LOG_BLOCK_SIZE;      // logical block size
  cfg.phys_erase_block = PHYS_ERASE_BLOCK;  // logical erase block size
  cfg.log_page_size = LOG_PAGE_SIZE;        // logical page size
  
  cfg.hal_read_f = lspiffs_read;
  cfg.hal_write_f = lspiffs_write;
  cfg.hal_erase_f = lspiffs_erase;
  //fs.file_cb_f =

  int res = SPIFFS_mount(&fs,
    &cfg,
    spiffs_work_buf,
    spiffs_fds,
    sizeof(spiffs_fds),
    spiffs_cache_buf,
    sizeof(spiffs_cache_buf),
    lspiffs_check_cb);
  if (res != 0) printf("*** MOUNT ERROR: %d!\r\n", res);
}

//-----------------------------------
bool lua_spiffs_mount(uint8_t warn) {
    
  if (SPIFFS_mounted(&fs)) return true;
  
  _mount();
  
  if (!SPIFFS_mounted(&fs)) {
    if (warn) {
      printf("*** WARNING: File system NOT mounted! ***\r\n");
      printf("    execute: file.format()\r\n");
    }
    return false;
  }
  return true;
}

// Find and return index of the first not used file_fd
//-----------------------------------------
static uint8_t getFileIdx( uint32_t *free )
{
  if (!SPIFFS_mounted(&fs)) return 97;

  uint32_t total, used;
  SPIFFS_info(&fs, &total, &used);
  if ((total > 2000000) || (used > 2000000) || (used > total)) return 98;

  if (free != NULL) *free = total - used;
  
  for (uint8_t i=0;i<MAX_FILE_FD;i++) {
    if (FILE_NOT_OPENED == file_fd[i]) return i;
  }
  return 99;
}

//---------------------------------
static void _closeFile(uint8_t idx)
{
  if (FILE_NOT_OPENED != file_fd[idx]) {
    SPIFFS_close(&fs,file_fd[idx]);
    file_fd[idx] = FILE_NOT_OPENED;
  }
}

//------------------------------
static void _closeAllFiles(void)
{
  for (uint8_t i=0;i<MAX_FILE_FD;i++) {
    _closeFile(i);
  }
}

// Check if 'name' is directory, remove trailing '/' if it is
// if 'trim', returns only dir name
//--------------------------------------------
static uint8_t isDir(char* name, uint8_t trim)
{
  uint8_t len = strlen(name)-1;
  if (name[len] == '/') { // is dir
    name[len] = '\0';
    if (trim) {
      len--;
      while (len > 0) {
        if (name[len] == '/') {
          memmove(name, name+len+1, strlen(name)-len);
          break;
        }
        len--;
      }
    }
    return 1;
  }
  else return 0;
}

//------------------------------------------------
static void getFileName(char* name, char* outname)
{
  uint8_t len = strlen(outname)-1;
  while (len > 0) {
    if (name[len] == '/') {
      len++;
      break;
    }
    len--;
  }
  strcpy(outname, name+len);
  if (outname[strlen(outname)-1] == '/') outname[strlen(outname)-1] = '\0';
}

// appends '/' to 'name'
//---------------------------------
static void setName2Dir(char* name)
{
  uint8_t dlen = strlen(name);
  if (dlen > 0) {
    name[dlen] = '/';
    name[dlen+1] = '\0';
  }
}

//-----------------------------------------------
static void getFileDir(char* name, char* dirname)
{
  uint8_t len = strlen(name)-1;
  if (name[len] == '/') len--; // if directory
  while (len > 0) {
    if (name[len] == '/') break;
    len--;
  }
  if (len > 0) {
    memcpy(dirname, name, len+1);
    dirname[len+1] = '\0';
  }
  else dirname[0] = '\0';
}

// check number of '/' in 'name', not including last
//----------------------------------
static uint8_t fileInDir(char* name)
{
  uint8_t n = 0;
  if (strlen(name) > 0) {
    for (uint8_t i=0;i<(strlen(name)-1);i++) {
      if (name[i] == '/') n++;
    }
  }
  return n;
}

// Check if there is opened file with the given name
//----------------------------------
static uint8_t _isOpened(char* name)
{
  spiffs_stat s;
  for (uint8_t i=0;i<MAX_FILE_FD;i++) {
    if (file_fd[i] != FILE_NOT_OPENED) {
      SPIFFS_fstat(&fs, file_fd[i], &s);
      if (strcmp((char*)s.name, name) == 0) return i;
    }
  }
  return 99;
}

//----------------------------
uint8_t fileExists(char* name)
{
  // check if file exists
  spiffs_file ffd = FILE_NOT_OPENED;
  ffd = SPIFFS_open(&fs, name, SPIFFS_RDONLY, 0);
  if (ffd <= FILE_NOT_OPENED) return 0;
  else {
    SPIFFS_close(&fs, ffd);
    return 1;
  }
}

//-------------------------------------
static uint8_t hasParentDir(char* name)
{
  char dirname[SPIFFS_OBJ_NAME_LEN] = {0};
  getFileDir(name, dirname);
  if (strlen(dirname) > 0) {
    // contains dir
    if (fileExists(dirname) == 0) return 0;
  }
  return 1;
}

// Check if file name 'name' with length 'len' os ok to use
// if 'addcurrdir' prefixes the name with current directory
// if 'exists', check if the file exists
// resulting name is placed in 'newname'
//-------------------------------------------------------------------------------------------------
uint8_t checkFileName(int len, const char* name, char* newname, uint8_t addcurrdir, uint8_t exists)
{
  if ((len <= 0) || (len >= SPIFFS_OBJ_NAME_LEN)) return 0;

  strcpy(newname, name);
  
  if (strlen(newname) == 0) return 0;
  // remove trailing '/'
  while (newname[strlen(newname)-1] == '/') {
    newname[strlen(newname)-1] = '\0';
  }
  if (strlen(newname) == 0) return 0;

  if ((addcurrdir) && (fileInDir(newname) == 0) && (strlen(curr_dir) > 0)) {
    // add current dir if not arready in some directory
    if (strlen(newname) + strlen(curr_dir) >= SPIFFS_OBJ_NAME_LEN) return 0;
    memmove(newname+strlen(curr_dir), newname, strlen(newname)+1);
    memcpy(newname, curr_dir, strlen(curr_dir));
  }
  // remove leading '/', we don't use it for root dir
  while (newname[0] == '/') {
    memmove(newname, newname+1, strlen(newname));
  }

  //printf("'%s' -> '%s', len=%d\r\n",name, newname, len);
  if (exists) return (fileExists(newname) + 1);
  else return 1;
}

// Check if 'name' is in directory 'dir'
// if 'incdir' directories are included
// if 'trim', remove directory from file name
//-----------------------------------------------------------------------
static uint8_t inDir(char *name, char *dir, uint8_t incdir, uint8_t trim)
{
  uint8_t res = 0;
  uint8_t len = strlen(name)-1;
  if ((incdir) || (name[len] != '/')) {
    // file or include directories requested
    if (name[len] == '/') {
      // if dir, trim dir sufix
      name[len] = '\0';
      len--;
      res = 2;
    }
    else res = 1;
    
    // check file directory
    if (strlen(dir) > 0) {
      // current dir not root
      char dirname[SPIFFS_OBJ_NAME_LEN] = {0};
      getFileDir(name, dirname);
      if (strlen(dirname) == 0) { res=0; goto exit; }; // file in root

      if (strcmp(dir, dirname) != 0) { res=0; goto exit; };
      // in current dir
      if (trim)
        memmove(name, name+strlen(dirname), strlen(name)-strlen(dirname)+1);
    }
    else {
      // root is current dir
      for (uint8_t i=0; i<len; i++) {
        if (name[i] == '/') { res=0; goto exit; }; // in other dir
      }
    }
  }
exit:  
  return res;
}

//---------------------------------------
static uint8_t dirHasFiles(char* dirname)
{
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;
  uint8_t nfiles = 0;

  SPIFFS_opendir(&fs, "/", &d);
  // find number of files in directory
  while ((pe = SPIFFS_readdir(&d, pe))) {
    if ((strcmp(dirname, (char*)pe->name) != 0) &&
        (strstr((char*)pe->name, dirname) == (char*)pe->name)) nfiles++;
  }
  SPIFFS_closedir(&d);
  return nfiles;
}

//---------------------------------------
static void dirRemoveFiles(char* dirname)
{
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;
  char fullname[SPIFFS_OBJ_NAME_LEN];

start:
  SPIFFS_opendir(&fs, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    strcpy(fullname, (char*)pe->name);
    uint8_t res = inDir((char*)pe->name, dirname, 1, 0);
    if (res == 2) {
      // recurse to remove inside directory
      dirRemoveFiles(fullname);
      SPIFFS_remove(&fs, fullname);
      // restart to get new list
      SPIFFS_closedir(&d);
      goto start;
    }
    else if (res == 1) {
      uint8_t fidx = _isOpened(fullname);
      if (fidx < MAX_FILE_FD) _closeFile(fidx);
      SPIFFS_remove(&fs, fullname);
    }
  }
  SPIFFS_closedir(&d);
}

//-----------------------------------------------------------------
static void getDir(lua_State* L, char *dir, int tree, uint8_t pass)
{
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;
  char subdir[SPIFFS_OBJ_NAME_LEN];
  
  SPIFFS_opendir(&fs, "/", &d);
  
  uint8_t res = 0;
  if (pass == 0) {
    if (tree < 0) lua_pushstring(L, "all FS files");
    else if (strlen(dir) == 0) lua_pushstring(L, "/");
    else {
      sprintf(subdir, "/%s", dir);
      lua_pushstring(L, subdir);
    }
    lua_newtable( L );
  }

  if (tree < 0) {
    while ((pe = SPIFFS_readdir(&d, pe))) {
      lua_pushinteger(L, pe->size);
      lua_setfield( L, -2, (char*)pe->name );
    }
  }
  else {
    while ((pe = SPIFFS_readdir(&d, pe))) {
      strcpy(subdir, (char*)pe->name);
      res = inDir((char*)pe->name, dir, 1, 1);
      if (res == 2) {
        lua_pushinteger(L, -1);
        lua_setfield( L, -2, subdir );
        if (tree > 0) getDir(L, subdir, tree-1, pass+1);
      }
      else if (res == 1) {
        lua_pushinteger(L, pe->size);
        lua_setfield( L, -2, subdir );
      }
    }
  }
  SPIFFS_closedir(&d);
}

//-------------------------------------------------------
static void listDir(char *dir, int tree, uint8_t prevlen)
{
  if (SPIFFS_mounted(&fs) == false) return;
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;
  uint8_t len = 0;
  char subdir[SPIFFS_OBJ_NAME_LEN];
  
  SPIFFS_opendir(&fs, "/", &d);

  if (prevlen == 0) {
    if (tree < 0) printf("\r\nAll FS files:\r\n");
    else {
      printf("\r\nList of directory ");
      if (strlen(dir) == 0) printf("'/':\r\n");
      else printf("'/%s':\r\n", dir);
    }
  }

  // find max mame length
  while ((pe = SPIFFS_readdir(&d, pe))) {
    uint8_t trim = 1;
    if (tree < 0) trim = 0;
    if ((tree < 0) || (inDir((char*)pe->name, dir, 1, trim))) {
      uint8_t nlen = strlen((char*)pe->name);
      if (nlen > len) len = nlen;
    }
  }
  // print
  if (len > 0) {
    if (prevlen == 0) {
      if (len < 4) len = 4;
      for (uint8_t n=0;n<len;n++) printf("-");
      printf("  ----\r\n");
      printf("%*s  Size\r\n", len, "Name");
      for (uint8_t n=0;n<len;n++) printf("-");
      printf("  ----\r\n");
    }
    d.block = 0;
    d.entry = 0;
    memset(&e, 0x00, sizeof(e));
    pe = &e;
    
    uint8_t res = 0;
    if (tree < 0) {
      while ((pe = SPIFFS_readdir(&d, pe))) {
        if (isDir((char*)pe->name, 0)) printf("%-*s  DIR\r\n", len, pe->name);
        else printf("%-*s  %i\r\n", len, pe->name, pe->size);
      }
    }
    else {
      while ((pe = SPIFFS_readdir(&d, pe))) {
        strcpy(subdir, (char*)pe->name);
        res = inDir((char*)pe->name, dir, 1, 1);
        if (res == 2) { // directory
          uint8_t addlen = 0;
          if (prevlen > 0) {
            printf("%*s", prevlen+5, "|_ ");
            addlen = 5;
          }
          printf("%-*s  DIR\r\n", len, pe->name);
          if (tree > 0) listDir(subdir, tree-1, len+prevlen+addlen);
        }
        else if (res == 1) { // file
          if (prevlen > 0) printf("%*s", prevlen+5, "|_ ");
          printf("%-*s  %i\r\n", len, pe->name, pe->size);
        }
      }
    }
    if (prevlen == 0) {
      for (uint8_t n=0;n<len;n++) printf("-");
      printf("  ----\r\n");
    }
  }
  SPIFFS_closedir(&d);
}

//-----------------------------------------------------------
static uint16_t Cal_CRC16(const uint8_t* data, uint32_t size)
{
  CRC16_Context contex;
  uint16_t ret;
  
  CRC16_Init( &contex );
  CRC16_Update( &contex, data, size );
  CRC16_Final( &contex, &ret );
  return ret;
}

/*
//------------------------------------------------------------
static uint8_t CalChecksum(const uint8_t* data, uint32_t size)
{
  uint32_t sum = 0;
  const uint8_t* dataEnd = data+size;

  while(data < dataEnd )
    sum += *data++;

  return (sum & 0xffu);
}
*/

//------------------------------------------------------------------------
static unsigned short crc16(const unsigned char *buf, unsigned long count)
{
  unsigned short crc = 0;
  int i;

  while(count--) {
    crc = crc ^ *buf++ << 8;

    for (i=0; i<8; i++) {
      if (crc & 0x8000) crc = crc << 1 ^ 0x1021;
      else crc = crc << 1;
    }
  }
  return crc;
}

//--------------------------------------------------------
static int32_t Receive_Byte (uint8_t *c, uint32_t timeout)
{
  if (MicoUartRecv( MICO_UART_1, c, 1, timeout ) != kNoErr)
    return -1; // Timeout
  else
    return 0;  // ok
}

//-----------------------------------
static uint32_t Send_Byte (uint8_t c)
{
  MicoUartSend( MICO_UART_1, &c, 1 );
  return 0;
}

//----------------------------
static void send_CA ( void ) {
  Send_Byte(CA);
  Send_Byte(CA);
  mico_thread_msleep(500);
  luaWdgReload();
}


/**
  * @brief  Receive a packet from sender
  * @param  data
  * @param  length
  * @param  timeout
  *     0: end of transmission
  *    -1: abort by sender
  *    >0: packet length
  * @retval 0: normally return
  *        -1: timeout or packet error
  *        -2: crc error
  *         1: abort by user
  */
//------------------------------------------------------------------------------
static int32_t Receive_Packet (uint8_t *data, int32_t *length, uint32_t timeout)
{
  uint16_t i, packet_size;
  uint8_t c;
  //uint16_t tempCRC;
  *length = 0;
  
  luaWdgReload();
  if (Receive_Byte(&c, timeout) != 0)
  {
    luaWdgReload();
    return -1;
  }
  luaWdgReload();
  switch (c)
  {
    case SOH:
      packet_size = PACKET_SIZE;
      break;
    case STX:
      packet_size = PACKET_1K_SIZE;
      break;
    case EOT:
      return 0;
    case CA:
      if ((Receive_Byte(&c, timeout) == 0) && (c == CA))
      {
        *length = -1;
        luaWdgReload();
        return 0;
      }
      else
      {
        luaWdgReload();
        return -1;
      }
    case ABORT1:
    case ABORT2:
      luaWdgReload();
      return 1;
    default:
      luaWdgReload();
      return -1;
  }
  *data = c;
  luaWdgReload();
  for (i = 1; i < (packet_size + PACKET_OVERHEAD); i ++)
  {
    if (Receive_Byte(data + i, 10) != 0)
    {
      luaWdgReload();
      return -1;
    }
  }
  luaWdgReload();
  if (data[PACKET_SEQNO_INDEX] != ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
  {
    return -1;
  }
  if (crc16(&data[PACKET_HEADER], packet_size + PACKET_TRAILER) != 0) {
    return -1;
  }
  *length = packet_size;
  return 0;
}

/**
  * @brief  Receive a file using the ymodem protocol.
  * @param  buf: Address of the first byte.
  * @retval The size of the file.
  */
//---------------------------------------------------------------------------------
static int32_t Ymodem_Receive ( char* FileName, uint32_t maxsize, uint8_t getname )
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD], file_size[FILE_SIZE_LENGTH], *file_ptr;
  int32_t i, packet_length, file_len, write_len, session_done, file_done, packets_received, errors, session_begin, size = 0;
  spiffs_file ffd = FILE_NOT_OPENED;
  
  for (session_done = 0, errors = 0, session_begin = 0; ;) {
    for (packets_received = 0, file_done = 0; ;) {
      switch (Receive_Packet(packet_data, &packet_length, NAK_TIMEOUT)) {
        case 0:
          switch (packet_length) {
            // Abort by sender
            case -1:
              Send_Byte(ACK);
              size = 0;
              goto exit;
            // End of transmission
            case 0:
              Send_Byte(ACK);
              file_done = 1;
              break;
            // Normal packet
            default:
              if ((packet_data[PACKET_SEQNO_INDEX] & 0xff) != (packets_received & 0xff)) {
                errors ++;
                if (errors > MAX_ERRORS) {
                  send_CA();
                  size = 0;
                  goto exit;
                }
                Send_Byte(NAK);
              }
              else {
                errors = 0;
                if (packets_received == 0) {
                  // Filename packet
                  if (packet_data[PACKET_HEADER] != 0) {
                    // Filename packet has valid data
                    if (getname == 0) {
                      for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (i < SPIFFS_OBJ_NAME_LEN);) {
                        FileName[i++] = *file_ptr++;
                      }
                      FileName[i++] = '\0';
                    }
                    for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (i < packet_length);) {
                      file_ptr++;
                    }
                    for (i = 0, file_ptr ++; (*file_ptr != ' ') && (i < FILE_SIZE_LENGTH);) {
                      file_size[i++] = *file_ptr++;
                    }
                    file_size[i++] = '\0';
                    Str2Int(file_size, &size);

                    // Test the size of the file
                    if (size < 1 || size > maxsize) {
                      // End session
                      send_CA();
                      size = -4;
                      goto exit;
                    }

                    // *** Open the file for writing***
                    char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
                    // check name, receive to current dir, abort if file exists
                    if (checkFileName(strlen(FileName), FileName, fullname, 1, 0) == 1 ) {
                      ffd = SPIFFS_open(&fs, fullname, mode2flag("w"), 0);
                      if (ffd <= FILE_NOT_OPENED) {
                        // End session
                        send_CA();
                        size = -2;
                        goto exit;
                      }
                    }
                    else {
                      // End session
                      send_CA();
                      size = -5;
                      goto exit;
                    }
                    file_len = 0;
                    Send_Byte(ACK);
                    Send_Byte(CRC16);
                  }
                  // Filename packet is empty, end session
                  else {
                    Send_Byte(ACK);
                    file_done = 1;
                    session_done = 1;
                    break;
                  }
                }
                // Data packet
                else
                {
                  // Write received data to file
                  if (file_len < size) {
                    file_len = file_len + packet_length;
                    if (file_len > size) {
                      write_len = packet_length - (file_len - size);
                    }
                    else {
                      write_len = packet_length;
                    }
                    if (ffd <= FILE_NOT_OPENED) {
                      // File not opened, End session
                      send_CA();
                      size = -2;
                      goto exit;
                    }
                    if (SPIFFS_write(&fs, ffd, (char*)(packet_data + PACKET_HEADER), write_len) < 0)
                    { //failed
                      /* End session */
                      send_CA();
                      size = 1;
                      goto exit;
                    }
                  }
                  //success
                  Send_Byte(ACK);
                }
                packets_received ++;
                session_begin = 1;
              }
          }
          break;
        case 1:
          send_CA();
          size = -3;
          goto exit;
        default:
          if (session_begin >= 0) errors ++;
          if (errors > MAX_ERRORS) {
            send_CA();
            size = 0;
            goto exit;
          }
          Send_Byte(CRC16);
          break;
      }
      if (file_done != 0) break;
    }
    if (session_done != 0) break;
  }
exit:
  if (ffd > FILE_NOT_OPENED) SPIFFS_close(&fs, ffd);
  return (int32_t)size;
}

//-----------------------------------------------------------
static void Ymodem_SendPacket(uint8_t *data, uint16_t length)
{
  uint16_t i;

  luaWdgReload();
  i = 0;
  while (i < length)
  {
    Send_Byte(data[i]);
    i++;
  }
}

//----------------------------------------------------------------------------------------------
static void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t* fileName, uint32_t *length)
{
  uint16_t i, j;
  uint8_t file_ptr[10];
  uint16_t tempCRC;
  
  // Make first three packet
  data[0] = SOH;
  data[1] = 0x00;
  data[2] = 0xff;
  
  // Filename packet has valid data
  for (i = 0; (fileName[i] != '\0') && (i <SPIFFS_OBJ_NAME_LEN);i++)
  {
     data[i + PACKET_HEADER] = fileName[i];
  }

  data[i + PACKET_HEADER] = 0x00;
  
  Int2Str (file_ptr, *length);
  for (j =0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0' ; )
  {
     data[i++] = file_ptr[j++];
  }
  data[i++] = 0x20;
  
  for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
  {
    data[j] = 0;
  }
  tempCRC = Cal_CRC16(&data[PACKET_HEADER], PACKET_SIZE);
  data[PACKET_SIZE + PACKET_HEADER] = tempCRC >> 8;
  data[PACKET_SIZE + PACKET_HEADER + 1] = tempCRC & 0xFF;
}

//-------------------------------------------------
static void Ymodem_PrepareLastPacket(uint8_t *data)
{
  uint16_t i;
  uint16_t tempCRC;
  
  data[0] = SOH;
  data[1] = 0x00;
  data[2] = 0xff;
  for (i = PACKET_HEADER; i < (PACKET_SIZE + PACKET_HEADER); i++) {
    data[i] = 0x00;
  }
  tempCRC = Cal_CRC16(&data[PACKET_HEADER], PACKET_SIZE);
  data[PACKET_SIZE + PACKET_HEADER] = tempCRC >> 8;
  data[PACKET_SIZE + PACKET_HEADER + 1] = tempCRC & 0xFF;
}

//-----------------------------------------------------------------------------------------------
static void Ymodem_PreparePacket(uint8_t *data, uint8_t pktNo, uint32_t sizeBlk, spiffs_file ffd)
{
  uint16_t i, size;
  uint16_t tempCRC;
  
  data[0] = STX;
  data[1] = pktNo;
  data[2] = (~pktNo);

  size = sizeBlk < PACKET_1K_SIZE ? sizeBlk :PACKET_1K_SIZE;
  // Read block from file
  if (size > 0) SPIFFS_read(&fs, ffd, data + PACKET_HEADER, size);

  if ( size  <= PACKET_1K_SIZE)
  {
    for (i = size + PACKET_HEADER; i < PACKET_1K_SIZE + PACKET_HEADER; i++)
    {
      data[i] = 0x1A; // EOF (0x1A) or 0x00
    }
  }
  tempCRC = Cal_CRC16(&data[PACKET_HEADER], PACKET_1K_SIZE);
  data[PACKET_1K_SIZE + PACKET_HEADER] = tempCRC >> 8;
  data[PACKET_1K_SIZE + PACKET_HEADER + 1] = tempCRC & 0xFF;
}

//--------------------------------------------------------
static uint8_t Ymodem_WaitACK(uint8_t ackchr, uint8_t tmo)
{
  uint8_t receivedC[2];
  uint32_t errors = 0;

  do {
    if (Receive_Byte(&receivedC[0], NAK_TIMEOUT) == 0) {
      if (receivedC[0] == ackchr) {
        return 1;
      }
      else if (receivedC[0] == CA) {
        send_CA();
        return 2; // CA received, Sender abort
      }
      else if (receivedC[0] == NAK) {
        return 3;
      }
      else {
        return 4;
      }
    }
    else {
      errors++;
    }
    luaWdgReload();
  }while (errors < tmo);
  return 0;
}


//------------------------------------------------------------------------------------------
static uint8_t Ymodem_Transmit (const char* sendFileName, uint32_t sizeFile, spiffs_file ffd)
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD];
  uint8_t filename[SPIFFS_OBJ_NAME_LEN];
  uint16_t blkNumber;
  uint8_t receivedC[1], i, err;
  uint32_t size = 0;

  for (i = 0; i < (SPIFFS_OBJ_NAME_LEN - 1); i++)
  {
    filename[i] = sendFileName[i];
  }
    
  while (MicoUartRecv( MICO_UART_1, &receivedC[0], 1, 10 ) == kNoErr) {};

  // Wait for response from receiver
  err = 0;
  do {
    luaWdgReload();
    Send_Byte(CRC16);
  } while (Receive_Byte(&receivedC[0], NAK_TIMEOUT) < 0 && err++ < 45);
  if (err >= 45 || receivedC[0] != CRC16) {
    send_CA();
    return 99;
  }
  
  // === Prepare first block and send it =======================================
  /* When the receiving program receives this block and successfully
   * opened the output file, it shall acknowledge this block with an ACK
   * character and then proceed with a normal YMODEM file transfer
   * beginning with a "C" or NAK tranmsitted by the receiver.
   */
  Ymodem_PrepareIntialPacket(&packet_data[0], filename, &sizeFile);
  do 
  {
    // Send Packet
    Ymodem_SendPacket(packet_data, PACKET_SIZE + PACKET_OVERHEAD);
    // Wait for Ack
    err = Ymodem_WaitACK(ACK, 10);
    if (err == 0 || err == 4) {
      send_CA();
      return 90;                  // timeout or wrong response
    }
    else if (err == 2) return 98; // abort
  }while (err != 1);

  // After initial block the receiver sends 'C' after ACK
  if (Ymodem_WaitACK(CRC16, 10) != 1) {
    send_CA();
    return 90;
  }
  
  // === Send file blocks ======================================================
  size = sizeFile;
  blkNumber = 0x01;
  
  // Resend packet if NAK  for a count of 10 else end of communication
  while (size)
  {
    // Prepare and send next packet
    Ymodem_PreparePacket(&packet_data[0], blkNumber, size, ffd);
    do
    {
      Ymodem_SendPacket(packet_data, PACKET_1K_SIZE + PACKET_OVERHEAD);
      // Wait for Ack
      err = Ymodem_WaitACK(ACK, 10);
      if (err == 1) {
        blkNumber++;
        if (size > PACKET_1K_SIZE) size -= PACKET_1K_SIZE; // Next packet
        else size = 0; // Last packet sent
      }
      else if (err == 0 || err == 4) {
        send_CA();
        return 90;                  // timeout or wrong response
      }
      else if (err == 2) return 98; // abort
    }while(err != 1);
  }
  
  // === Send EOT ==============================================================
  Send_Byte(EOT); // Send (EOT)
  // Wait for Ack
  do 
  {
    // Wait for Ack
    err = Ymodem_WaitACK(ACK, 10);
    if (err == 3) {   // NAK
      Send_Byte(EOT); // Send (EOT)
    }
    else if (err == 0 || err == 4) {
      send_CA();
      return 90;                  // timeout or wrong response
    }
    else if (err == 2) return 98; // abort
  }while (err != 1);
  
  // === Receiver requests next file, prepare and send last packet =============
  if (Ymodem_WaitACK(CRC16, 10) != 1) {
    send_CA();
    return 90;
  }

  Ymodem_PrepareLastPacket(&packet_data[0]);
  do 
  {
    Ymodem_SendPacket(packet_data, PACKET_SIZE + PACKET_OVERHEAD); // Send Packet
    // Wait for Ack
    err = Ymodem_WaitACK(ACK, 10);
    if (err == 0 || err == 4) {
      send_CA();
      return 90;                  // timeout or wrong response
    }
    else if (err == 2) return 98; // abort
  }while (err != 1);
  
  return 0; // file transmitted successfully
}

//==================================
static int file_recv( lua_State* L )
{
  int32_t fsize = 0;
  uint8_t c, gnm;
  char fnm[SPIFFS_OBJ_NAME_LEN];
  uint32_t max_len = 0;

  gnm = 0;
  if (lua_gettop(L) == 1 && lua_type( L, 1 ) == LUA_TSTRING) {
    // file name is given
    size_t len;
    const char *fname = luaL_checklstring( L, 1, &len );
    if (checkFileName(len, fname, fnm, 1, 1) == 1) {
      // good name & file not found
      if (hasParentDir(fnm)) gnm = 1; // file name ok
      else {fsize=-5; goto exit; }
    }
    else {fsize=-5; goto exit; }
  }
  if (gnm == 0) memset(fnm, 0x00, SPIFFS_OBJ_NAME_LEN);
  
  l_message(NULL,"Start Ymodem file transfer...");

  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}
  
  fsize = Ymodem_Receive(fnm, max_len-10000, gnm);
  
  luaWdgReload();
  mico_thread_msleep(500);
  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}

  mico_thread_msleep(500);
  
exit:  
  if (fsize > 0) printf("\r\nReceived successfully, %d\r\n",fsize);
  else if (fsize == -1) printf("\r\nFile write error!\r\n");
  else if (fsize == -2) printf("\r\nFile open error!\r\n");
  else if (fsize == -3) printf("\r\nAborted.\r\n");
  else if (fsize == -4) printf("\r\nFile size too big, aborted.\r\n");
  else if (fsize == -5) printf("\r\nWrong file name or file exists!\r\n");
  else printf("\r\nReceive failed!\r\n");
  
  if (fsize > 0) listDir(curr_dir, 0, 0);

  return 0;
}


//==================================
static int file_send( lua_State* L )
{
  int8_t res = 0;
  int8_t newname = 0;
  uint8_t c;
  spiffs_stat s;
  const char *fname;
  size_t len;

  fname = luaL_checklstring( L, 1, &len );
  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  char newfullname[SPIFFS_OBJ_NAME_LEN] = {0};

  res = checkFileName(len, fname, fullname, 1, 1);
  if (res != 2) return luaL_error(L, "wrong file name");

  // close file if opened
  uint8_t fidx = _isOpened((char*)fullname);
  if (fidx < MAX_FILE_FD) _closeFile(fidx);

  if (lua_gettop(L) == 2 && lua_type( L, 2 ) == LUA_TSTRING) {
    size_t len;
    const char * newfname = luaL_checklstring( L, 2, &len );
    res = checkFileName(len, newfname, newfullname, 0, 0);
    if ((res > 0) && (!fileInDir(newfullname))) newname = 1;
    else printf("Wrong 'send as' name, sending as '%s'\r\n", fullname);
  }

  // Open the file
  spiffs_file ffd = FILE_NOT_OPENED;
  ffd = SPIFFS_open(&fs, fullname, SPIFFS_RDONLY, 0);
  if (ffd <= FILE_NOT_OPENED) {
    l_message(NULL,"Error opening file.");
    return 0;
  }

  // Get file size
  SPIFFS_fstat(&fs, ffd, &s);
  if (newname == 1) {
    printf("Sending '%s' as '%s'\r\n", fullname, newfullname);
    strcpy(fullname, newfullname);
  }
  
  l_message(NULL,"Start Ymodem file transfer...");

  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}
  
  res = Ymodem_Transmit(fullname, s.size, ffd);
  
  luaWdgReload();
  mico_thread_msleep(500);
  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}

  SPIFFS_close(&fs, ffd);

  if (res == 0) {
    l_message(NULL,"\r\nFile sent successfuly.");
  }
  else if (res == 99) {
    l_message(NULL,"\r\nNo response.");
  }
  else if (res == 98) {
    l_message(NULL,"\r\nAborted.");
  }
  else {
    l_message(NULL,"\r\nError sending file.");
  }
  return 0;
}

//-----------------------------------------
int  _getListArg(lua_State* L, char *dname)
{
  int tree = 0;
  strcpy(dname, curr_dir);
  uint8_t n = lua_gettop(L);
  
  if (n > 0) {
    if (lua_type(L, 1) == LUA_TSTRING) {
      n++;
      size_t len;
      const char *fname = luaL_checklstring( L, 1, &len );
      if ((len == 0) || (strcmp(fname, "/") == 0)) {
        dname[0] = '\0'; // root
      }
      else {
        char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
        if (checkFileName(len, fname, fullname, 1, 0) == 1) {
          setName2Dir(fullname);
          if (fileExists(fullname)) strcpy(dname, fullname);
        }
      }
      if (n < 2) n = 0;
      else n = 2;
    }
    else n = 1;
    
    if ((n > 0) && (lua_type(L, n) == LUA_TNUMBER)) {
      tree = luaL_checkinteger(L, n);
      if (tree > MAX_DIR_DEPTH) tree = MAX_DIR_DEPTH;
    }
  }
  return tree;
}

//table = file.list([path],[tree])
//==================================
static int file_list( lua_State* L )
{
  char dirname[SPIFFS_OBJ_NAME_LEN] = {0};
  int tree = _getListArg(L, dirname);

  if (SPIFFS_mounted(&fs) == false) {
    lua_pushstring(L, "Not mounted");
    lua_newtable( L );
    return 2;
  }
  
  getDir(L, dirname, tree, 0);
  return 2;
}

//file.slist([path],[tree])  Print file list
//===================================
static int file_slist( lua_State* L )
{
  char dirname[SPIFFS_OBJ_NAME_LEN] = {0};
  int tree = _getListArg(L, dirname);
  
  listDir(dirname, tree, 0);
  return 0;
}

//file.format()
//====================================
static int file_format( lua_State* L )
{
  if (SPIFFS_mounted(&fs)==false) lua_spiffs_mount(0);
  else _closeAllFiles();
  
  SPIFFS_unmount(&fs);
  
#ifdef LOBO_SPIFFS_DBG
  uint8_t bspiffs_debug = spiffs_debug;
  spiffs_debug = 0x22;
#endif
  printf("Formating, please wait...\r\n");
  int err = SPIFFS_format(&fs);
  if (err == 0) printf("Format done.\r\n");
  else printf("Format error: %d\r\n", err);
  
#ifdef LOBO_SPIFFS_DBG
  spiffs_debug = bspiffs_debug;
#endif  
  if (SPIFFS_mounted(&fs)==false) lua_spiffs_mount(1);
  
  return 0;
}

// file.open(filename, mode)
//==================================
static int file_open( lua_State* L )
{
  uint8_t fidx = FILE_NOT_OPENED;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  const char *mode = luaL_optstring(L, 2, "r");
  
  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if (checkFileName(len, fname, fullname, 1, 0) == 0) {
    l_message(NULL,"wrong file name");
    goto errexit;
  }
  if (_isOpened((char*)fullname) < MAX_FILE_FD) {
    l_message(NULL, "file already opened");
    goto errexit;
  }
  if (!hasParentDir(fullname)) {
    l_message(NULL, "file directory not found");
    goto errexit;
  }

  if (mode[0] == 'r') {
    // read mode, check if file exists
    if (fileExists(fullname) == 0) {
      l_message(NULL, "file not found");
      goto errexit;
    }
  }
  
  fidx = getFileIdx(NULL);
  if (fidx >= MAX_FILE_FD) {
    l_message(NULL, "too many opened files");
    goto errexit;
  }
  
  file_fd[fidx] = SPIFFS_open(&fs, (char*)fullname,mode2flag((char*)mode), 0);
  if (file_fd[fidx] <= FILE_NOT_OPENED) {
    file_fd[fidx] = FILE_NOT_OPENED;
    goto errexit;
  }
  
  lua_pushinteger(L, fidx);
  return 1;
  
errexit:
  lua_pushinteger(L, -1);;
  return 1;
}

// file.mkdir(dirname)
//===================================
static int file_mkdir( lua_State* L )
{
  spiffs_file dir_fd = FILE_NOT_OPENED;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  
  if (checkFileName(len, fname, fullname, 1, 0) == 0) {
    l_message(NULL, "wrong dir name");
    goto errexit;
  }
  if (!hasParentDir(fullname)) {
    l_message(NULL, "parent directory not found");
    goto errexit;
  }

  setName2Dir(fullname);
  if (fileExists(fullname)) goto okexit;

  dir_fd = SPIFFS_open(&fs, (char*)fullname, mode2flag("w"), 0);
  if (dir_fd <= FILE_NOT_OPENED) goto errexit;

  SPIFFS_close(&fs, dir_fd);

okexit:
  lua_pushboolean(L, true);
  return 1; 

errexit:
  lua_pushboolean(L, false);
  return 1;
}

// file.chdir(dirname)
//===================================
static int file_chdir( lua_State* L )
{
  if (lua_gettop(L) == 0) {
    // no arg, return current directory
    if (strlen(curr_dir) == 0) lua_pushstring(L, "/");
    else lua_pushstring(L, curr_dir);
    return 1;
  }

  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );

  if ((len == 1) && (strcmp(fname, "/") == 0)) {
    // root dir requested
    curr_dir[0] = '\0';
    lua_pushboolean(L, true);
    return 1;
  }

  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if ((len == 2) && (strcmp(fname, "..") == 0)) {
    if (strlen(curr_dir) > 0) {
      // parent dir requested
      strcpy(fullname, curr_dir);
      // get parent dir
      char dirname[SPIFFS_OBJ_NAME_LEN] = {0};
      getFileDir(fullname, dirname);
      if (strlen(dirname) > 0) {
        // parend dir is dir
        strcpy(curr_dir, dirname);
      }
      else {
        // parent dir is root
        curr_dir[0] = '\0';
      }
    }
    lua_pushboolean(L, true);
    return 1;
  }

  if (checkFileName(len, fname, fullname, 1, 0) == 0) {
    l_message(NULL, "wrong dir name");
    lua_pushboolean(L, false);
  }
  else {
    setName2Dir(fullname);
    if (!fileExists(fullname)) lua_pushboolean(L, false);
    else {
      strcpy(curr_dir, fullname);
      lua_pushboolean(L, true);
    }
  }
  return 1; 
}

//----------------------------------------------
static uint8_t _getFidx(lua_State* L, uint8_t n)
{
  if (lua_gettop(L) >= n) {
    int idx = luaL_checkinteger(L, 1);
    if ((idx < 0) || (idx >= MAX_FILE_FD)) return 99;
    return (uint8_t)idx;
  }
  else return 98;
}

// file.close(fd)
//===================================
static int file_close( lua_State* L )
{
  if (lua_gettop(L) > 0) {
    uint8_t fidx = _getFidx(L, 1);
    if (fidx >= MAX_FILE_FD) lua_pushboolean(L, false);
    else {
      if (file_fd[fidx] > FILE_NOT_OPENED) {
        _closeFile(fidx);
        lua_pushboolean(L, true);
      }
      else lua_pushboolean(L, false);
    }
  }
  else {
    _closeAllFiles();
    lua_pushboolean(L, true);
  }

  return 1;
}

// file.exists(filename)
//===================================
static int file_exists( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  
  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if (checkFileName(len, fname, fullname, 1, 1) == 2) lua_pushboolean(L, true);
  else lua_pushboolean(L, false);

  return 1;
}

// file.write(fd, "string")
//===================================
static int file_write( lua_State* L )
{
  size_t len;
  const char *s;
  uint8_t fidx = _getFidx(L, 2);
  if (fidx >= MAX_FILE_FD) {
    l_message(NULL, "wrong file handle");
    goto errexit;
  }
  if (FILE_NOT_OPENED == file_fd[fidx]) {
    l_message(NULL, "file not opened");
    goto errexit;
  }
  
  s = luaL_checklstring(L, 2, &len);
  if (len <= 0) goto okexit;
  
  if (SPIFFS_write(&fs,file_fd[fidx], (char*)s, len) < 0) { //failed
    _closeFile(fidx);
    goto errexit;
  }

okexit:
  lua_pushboolean(L, true);
  return 1;

errexit:
  lua_pushboolean(L, false);
  return 1;
}

// file.writeline(fd, "string")
//=======================================
static int file_writeline( lua_State* L )
{
  size_t len;
  const char *s;
  uint8_t fidx = _getFidx(L, 2);
  if (fidx >= MAX_FILE_FD) {
    l_message(NULL, "wrong file handle");
    goto errexit;
  }
  if (FILE_NOT_OPENED == file_fd[fidx]) {
    l_message(NULL, "file not opened");
    goto errexit;
  }
  
  s = luaL_checklstring(L, 2, &len);

  if (len > 0) {
    if (SPIFFS_write(&fs,file_fd[fidx], (char*)s, len) < 0) {
      _closeFile(fidx);
      goto errexit;
    }
  }

  if (SPIFFS_write(&fs, file_fd[fidx], "\r\n", 2) < 0) {
    _closeFile(fidx);
    goto errexit;
  }
  
  lua_pushboolean(L, true);
  return 1;

errexit:
  lua_pushboolean(L, false);
  return 1;
}

//---------------------------------------------------------------------------
static int file_g_read( lua_State* L, int n, int16_t end_char, uint8_t fidx )
{
  if ((n < 0) || (n > LUAL_BUFFERSIZE)) n = LUAL_BUFFERSIZE;
  if ((end_char < 0) || (end_char > 255)) end_char = EOF;
  int ec = (int)end_char;
  
  static luaL_Buffer b;

  luaL_buffinit(L, &b);
  char *p = luaL_prepbuffer(&b);
  int c = EOF;
  int i = 0;

  do {
    c = EOF;
    SPIFFS_read(&fs, (spiffs_file)file_fd[fidx], &c, 1);
    if (c == EOF) break;
    p[i++] = (char)(0xFF & c);
  } while((c!=EOF) && ((char)c !=(char)ec) && (i<n) );

#if 0
  if(i>0 && p[i-1] == '\n') i--;    // do not include `eol'
#endif
    
  if (i == 0) {
    luaL_pushresult(&b);            // close buffer
    return (lua_objlen(L, -1) > 0); // check whether read something
  }

  luaL_addsize(&b, i);
  luaL_pushresult(&b);              // close buffer
  return 1;                         // read at least an `eol'
}

// file.read()
// file.read() read all byte in file LUAL_BUFFERSIZE(512) max
// file.read(10) will read 10 byte from file, or EOF is reached.
// file.read('q') will read until 'q' or EOF is reached. 
//==================================
static int file_read( lua_State* L )
{
  unsigned need_len = LUAL_BUFFERSIZE;
  int16_t end_char = EOF;
  size_t el;
  uint8_t fidx = _getFidx(L, 1);

  if (fidx >= MAX_FILE_FD) return luaL_error(L, "wrong file handle");
  if (FILE_NOT_OPENED == file_fd[fidx]) return luaL_error(L, "file not opened");

  if (lua_type( L, 2 ) == LUA_TNUMBER) {
    need_len = (unsigned)luaL_checkinteger( L, 2 );
    if (need_len > LUAL_BUFFERSIZE) need_len = LUAL_BUFFERSIZE;
  }
  else if (lua_isstring(L, 2)) {
    const char *end = luaL_checklstring( L, 2, &el );
    if (el != 1) return luaL_error( L, "wrong arg range" );
    end_char = (int16_t)end[0];
  }
  
  return file_g_read(L, need_len, end_char, fidx);
}

// file.readline(fd)
//======================================
static int file_readline( lua_State* L )
{
  uint8_t fidx = _getFidx(L, 1);
  if (fidx >= MAX_FILE_FD) return luaL_error(L, "wrong file handle");
  if (FILE_NOT_OPENED == file_fd[fidx]) return luaL_error(L, "file not opened");
  
  return file_g_read(L, LUAL_BUFFERSIZE, '\n', fidx);
}

//file.seek(fd, whence, offset)
//=================================
static int file_seek (lua_State *L) 
{
  uint8_t fidx = _getFidx(L, 1);
  if (fidx >= MAX_FILE_FD) return luaL_error(L, "wrong file handle");
  if (FILE_NOT_OPENED == file_fd[fidx]) return luaL_error(L, "file not opened");

  static const int mode[] = {SPIFFS_SEEK_SET, SPIFFS_SEEK_CUR, SPIFFS_SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  
  int op = luaL_checkoption(L, 2, "cur", modenames);
  long offset = luaL_optlong(L, 3, 0);
  op = SPIFFS_lseek(&fs,file_fd[fidx], offset, mode[op]);
  if (op < 0) lua_pushnil(L);  // error
  else {
    spiffs_fd *fd;
    spiffs_fd_get(&fs, file_fd[fidx], &fd);
    lua_pushinteger(L, fd->fdoffset);
  }
  
  return 1;
}

//file.tell(fd)
//=================================
static int file_tell (lua_State *L) 
{
  uint8_t fidx = _getFidx(L, 1);
  if (fidx >= MAX_FILE_FD) return luaL_error(L, "wrong file handle");
  if (FILE_NOT_OPENED == file_fd[fidx]) return luaL_error(L, "file not opened");
  int pos = SPIFFS_tell(&fs, file_fd[fidx]);

  lua_pushinteger(L, pos);
  return 1;
}

// file.flush(fd)
//===================================
static int file_flush( lua_State* L )
{
  uint8_t fidx = _getFidx(L, 1);
  if (fidx >= MAX_FILE_FD) {
    l_message(NULL, "wrong file handle");
    return 0;
  }
  if (FILE_NOT_OPENED == file_fd[fidx]) {
    l_message(NULL, "file not opened");
    return 0;
  }
  
  SPIFFS_fflush(&fs,file_fd[fidx]);
  
  return 0;
}

// file.remove(filename)
//====================================
static int file_remove( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if (checkFileName(len, fname, fullname, 1, 0) == 1) {
    uint8_t fidx = _isOpened(fullname);
    if (fidx < MAX_FILE_FD) _closeFile(fidx);

    if (fileExists(fullname)) {
      if (SPIFFS_remove(&fs, fullname) == 0) lua_pushboolean(L, true);
      else lua_pushboolean(L, false);
    }
    else lua_pushboolean(L, false);
  }
  else lua_pushboolean(L, false);
  
  return 1;
}

// file.rename("oldname", "newname")
//====================================
static int file_rename( lua_State* L )
{
  size_t len;

  const char *oldname = luaL_checklstring( L, 1, &len );
  char oldfullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if (checkFileName(len, oldname, oldfullname, 1, 1) != 2)
    return luaL_error(L, "wrong old file name or file not found");
  
  const char *newname = luaL_checklstring( L, 2, &len );
  char newfullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if (checkFileName(len, newname, newfullname, 1, 0) == 0) return luaL_error(L, "wrong new file name");
  if (fileExists(newfullname)) {
    l_message(NULL, "file with new name already exists, not renamed");
    lua_pushboolean(L, false);
  }
  else {
    uint8_t fidx = _isOpened(oldfullname);
    if (fidx < MAX_FILE_FD) _closeFile(fidx);
    
    fidx = _isOpened(newfullname);
    if (fidx < MAX_FILE_FD) _closeFile(fidx);

    if (SPIFFS_OK == SPIFFS_rename(&fs, oldfullname, newfullname )) lua_pushboolean(L, true);
    else lua_pushboolean(L, false);
  }

  return 1;
}

// file.rmdir(dirname, ["removeall"])
//===================================
static int file_rmdir( lua_State* L )
{
  size_t len, rlen;
  uint8_t remfiles = 0;
  
  const char *fname = luaL_checklstring( L, 1, &len );
  if (lua_gettop(L) > 1) {
    const char *rmf = luaL_checklstring( L, 2, &rlen );
    if (strcmp(rmf, "removeall") == 0) remfiles = 1;
  }

  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if (checkFileName(len, fname, fullname, 1, 0) == 0) return luaL_error(L, "wrong file name");

  setName2Dir(fullname);
  if (!fileExists(fullname)) return luaL_error(L, "dir '%s' not found", fullname);
  
  if (remfiles) dirRemoveFiles(fullname);
  // check if there are files in directory
  if (dirHasFiles(fullname)) return luaL_error(L, "dir '%s' not empty", fullname);
  
  if (SPIFFS_remove(&fs, fullname) == 0) lua_pushboolean(L, true);
  else lua_pushboolean(L, false);
  
  return 1;
}

//file.info([print])
//==================================
static int file_info( lua_State* L )
{
  uint32_t total, used;
  int prn = 0;
  if (lua_gettop(L) > 0) {
    prn = luaL_checkinteger(L, 1);
    if (prn != 1) prn = 0;
  }

  SPIFFS_info(&fs, &total, &used);
  if ((!SPIFFS_mounted(&fs)) || (total > 2000000) || (used > 2000000) || (used > total)) {
    //printf("Free: %d, Used: %d, Total: %d\r\n", total-used, used, total);
    return luaL_error(L, "file system error");
  }
  if (prn == 1) {
    printf("\r\nFile system info :\r\n");
    printf("------------------\r\n");
    printf("  used bytes     : %i of %i total\r\n", used, total);
    printf("  free bytes     : %i\r\n", total-used);
    printf("  block size     : %iKB\r\n", LOG_BLOCK_SIZE_K);
    printf("  page size      : %iB\r\n", LOG_PAGE_SIZE);
    printf("  free blocks    : %i of %i\r\n", fs.free_blocks, fs.block_count);
    printf("  pages allocated: %i\r\n", fs.stats_p_allocated);
    printf("  pages deleted  : %i\r\n", fs.stats_p_deleted);
    #if SPIFFS_GC_STATS
    printf("  gc runs        : %i\r\n", fs.stats_gc_runs);
    #endif
    #if SPIFFS_CACHE
    #if SPIFFS_CACHE_STATS
    chits_tot += fs.cache_hits;
    cmiss_tot += fs.cache_misses;
    printf("  cache hits     : %i (sum %i)\r\n", fs.cache_hits, chits_tot);
    printf("  cache misses   : %i (sum %i)\r\n", fs.cache_misses, cmiss_tot);
    printf("  cache utiliz   : %f\r\n", ((float)chits_tot/(float)(chits_tot + cmiss_tot)));
    chits_tot = 0;
    cmiss_tot = 0;
    #endif
    #endif
    printf("------------------\r\n");
    return 0;
  }
  else {
    lua_pushinteger(L, total-used);
    lua_pushinteger(L, used);
    lua_pushinteger(L, total);
    return 3;
  }
}

#if SPIFFS_TEST_VISUALISATION
//=================================
static int file_vis( lua_State* L )
{
  SPIFFS_vis(&fs);
  return 0;
}
#endif

//=====================================
static int file_lasterr( lua_State* L )
{
  lua_pushinteger(L, SPIFFS_errno(&fs));
  return 1;
}

//file.state(fd)
//===================================
static int file_state( lua_State* L )
{
  uint8_t fidx = _getFidx(L, 1);
  if (fidx >= MAX_FILE_FD) return luaL_error(L, "wrong file handle");
  if (FILE_NOT_OPENED == file_fd[fidx]) return luaL_error(L, "file not opened");
  
  spiffs_stat s;
  SPIFFS_fstat(&fs, file_fd[fidx], &s);
  
  lua_pushstring(L,(char*)s.name);
  lua_pushinteger(L,s.size);
  
  return 2;
}

// file.find("name", ["all"])
//==================================
static int file_find( lua_State* L )
{
  uint8_t err = 1;
  uint8_t allfiles = 0;

  if (SPIFFS_mounted(&fs) == true) {
    size_t len;
    const char *fname = luaL_checklstring( L, 1, &len );
    if ((len > 0) && (fname != NULL)) {
      char filename[SPIFFS_OBJ_NAME_LEN] = {0};
      if (lua_gettop(L) > 1) {
        if (lua_isstring(L, 2)) {
          const char *sall = luaL_checklstring( L, 2, &len );
          if (strcmp(sall, "all") == 0) {
            allfiles = 1;
            lua_newtable( L );
          }
        }
      }
      err = 0;
      
      spiffs_DIR d;
      struct spiffs_dirent e;
      struct spiffs_dirent *pe = &e;
      int n = 0;

      SPIFFS_opendir(&fs, "/", &d);
      // find the file matching name
      while ((pe = SPIFFS_readdir(&d, pe))) {
        getFileName((char*)pe->name, filename);
        if (fnmatch(fname, filename, (FNM_PATHNAME || FNM_PERIOD)) == 0) {
          //printf("%s\r\n", (char*)pe->name);
          lua_pushstring(L, (char*)pe->name);
          if (allfiles) {
            lua_rawseti(L, -2, ++n);
          }
          else {
            break;
          }
        }
      }
      if (allfiles) lua_pushinteger(L, n);
      SPIFFS_closedir(&d);
    }
  }
  
  if (err) {
    lua_pushnil(L);
    return 1;
  }
  else if (allfiles) return 2;
  return 1;
}


#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"
//__________________________________________________________________
static int writer(lua_State* L, const void* p, size_t size, void* u)
{
  UNUSED(L);
  int file_fd = *( (int *)u );
  if ( FILE_NOT_OPENED == file_fd) return 1;
  //MCU_DBG("get fd:%d,size:%d\n", file_fd, size);

  if (size != 0 && (size != SPIFFS_write(&fs,file_fd, (char *)p, size)) ) return 1;
  //MCU_DBG("write fd:%d,size:%d\n", file_fd, size);
  return 0;
}

//rewrite lauxlib.c:luaL_loadfile
#define toproto(L,i) (clvalue(L->top+(i))->l.p)
//_____________________________________
static int file_compile( lua_State* L )
{
  Proto* f;
  int file_fd = FILE_NOT_OPENED;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );

  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  if (checkFileName(len, fname, fullname, 1, 0) == 0) return luaL_error(L, "wrong file name");
  if (!hasParentDir(fullname)) return luaL_error(L, "file directory not found");
  if (_isOpened((char*)fullname) < MAX_FILE_FD) return luaL_error(L, "file is opened");
  if (fileExists(fullname) == 0) return luaL_error(L, "file not found");
  len = strlen(fullname);
  // check here that filename end with ".lua".
  if (len < 4 || (strcmp( fullname + len - 4, ".lua") != 0) ) return luaL_error(L, "not a .lua file");

  char output[SPIFFS_OBJ_NAME_LEN];
  strcpy(output, fullname);

  output[strlen(output) - 2] = 'c';
  output[strlen(output) - 1] = '\0';
  
  if (luaL_loadfile(L, fullname) != 0) return luaL_error(L, lua_tostring(L, -1));

  f = toproto(L, -1);

  int stripping = 1;      // strip debug information?

  file_fd = SPIFFS_open(&fs, (char*)output, mode2flag("w+"), 0);
  if (file_fd <= FILE_NOT_OPENED) return luaL_error(L, "cannot open/write to file");

  lua_lock(L);
  int result = luaU_dump(L, f, writer, &file_fd, stripping);
  lua_unlock(L);

  SPIFFS_fflush(&fs,file_fd);
  SPIFFS_close(&fs,file_fd);
  file_fd =FILE_NOT_OPENED;

  if (result == LUA_ERR_CC_INTOVERFLOW) {
    return luaL_error(L, "value too big or small for target integer type");
  }
  if (result == LUA_ERR_CC_NOTINTEGER) {
    return luaL_error(L, "target lua_Number is integral but fractional value found");
  }

  return 0;
}

//================================
static int file_gc( lua_State* L )
{
  if (SPIFFS_mounted(&fs) == false) {
    if (!lua_spiffs_mount(1)) return 0;
  }
  int n = 1;
  if (lua_gettop(L) > 0) {
    n = luaL_checkinteger(L, 1);
    if ((n < 0) || (n > 24)) n = 1;
  }

  l_message(NULL,"Starting fs garbage collection, please wait...\r\n");
  int nf = 0;
  for (int i=0; i<n; i++) {
    if (SPIFFS_gc_quick(&fs, 0) < 0) break;
    nf++;
  }
  lua_pushinteger(L, nf);  
  printf("Finished, %d %dK block(s) erased.\r\n", nf, LOG_BLOCK_SIZE_K);
  return 1;
}

//===================================
static int file_check( lua_State* L )
{
  if (SPIFFS_mounted(&fs) == false) {
    if (!lua_spiffs_mount(1)) return 0;
  }
    
#ifdef LOBO_SPIFFS_DBG
  uint8_t bspiffs_debug = spiffs_debug;
  spiffs_debug |= 0x08;
#endif

  int check[7] = {0};
  fs_check = &check[0];
  printf("\r\nChecking fs, please wait...\r\n");
  SPIFFS_check(&fs);
  fs_check = NULL;
  printf("Finished.\r\n");
  printf("--------------\r\n");
  printf(" Checks      : %d\r\n", check[SPIFFS_CHECK_PROGRESS]);
  printf(" Errors      : %d\r\n", check[SPIFFS_CHECK_ERROR]);
  printf(" Fix index   : %d\r\n", check[SPIFFS_CHECK_FIX_INDEX]);
  printf(" Fix look up : %d\r\n", check[SPIFFS_CHECK_FIX_LOOKUP]);
  printf(" Del orph pg : %d\r\n", check[SPIFFS_CHECK_DELETE_ORPHANED_INDEX]);
  printf(" Del pages   : %d\r\n", check[SPIFFS_CHECK_DELETE_PAGE]);
  printf(" Del bad file: %d\r\n", check[SPIFFS_CHECK_DELETE_BAD_FILE]);
  printf("--------------\r\n");
  
#ifdef LOBO_SPIFFS_DBG
  spiffs_debug = bspiffs_debug;
#endif  
  return 0;
}

#ifdef LOBO_SPIFFS_DBG
//===================================
static int file_debug( lua_State* L )
{
  if (lua_gettop(L) > 0) {
    int dbg = luaL_checkinteger(L, 1);
    spiffs_debug = dbg & 0x0F;
  }
  lua_pushinteger(L, spiffs_debug);
  return 1;
}
#endif


#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE file_map[] =
{
  { LSTRKEY( "list" ), LFUNCVAL( file_list ) },
  { LSTRKEY( "slist" ), LFUNCVAL( file_slist ) },
  { LSTRKEY( "format" ), LFUNCVAL( file_format ) },
  { LSTRKEY( "open" ), LFUNCVAL( file_open ) },
  { LSTRKEY( "close" ), LFUNCVAL( file_close ) },
  { LSTRKEY( "exists" ), LFUNCVAL( file_exists ) },
  { LSTRKEY( "write" ), LFUNCVAL( file_write ) },
  { LSTRKEY( "writeline" ), LFUNCVAL( file_writeline ) },
  { LSTRKEY( "read" ), LFUNCVAL( file_read ) },
  { LSTRKEY( "readline" ), LFUNCVAL( file_readline ) },
  { LSTRKEY( "remove" ), LFUNCVAL( file_remove ) },
  { LSTRKEY( "seek" ), LFUNCVAL( file_seek ) },
  { LSTRKEY( "tell" ), LFUNCVAL( file_tell ) },
  { LSTRKEY( "flush" ), LFUNCVAL( file_flush ) },
  { LSTRKEY( "rename" ), LFUNCVAL( file_rename ) },
  { LSTRKEY( "info" ), LFUNCVAL( file_info ) },
  { LSTRKEY( "state" ), LFUNCVAL( file_state ) },
  { LSTRKEY( "compile" ), LFUNCVAL( file_compile ) },
  { LSTRKEY( "recv" ), LFUNCVAL( file_recv ) },
  { LSTRKEY( "send" ), LFUNCVAL( file_send ) },
  { LSTRKEY( "check" ), LFUNCVAL( file_check ) },
  { LSTRKEY( "gc" ), LFUNCVAL( file_gc ) },
  { LSTRKEY( "lasterr" ), LFUNCVAL( file_lasterr ) },
  { LSTRKEY( "mkdir" ), LFUNCVAL( file_mkdir ) },
  { LSTRKEY( "chdir" ), LFUNCVAL( file_chdir ) },
  { LSTRKEY( "rmdir" ), LFUNCVAL( file_rmdir ) },
  { LSTRKEY( "find" ), LFUNCVAL( file_find ) },
#ifdef LOBO_SPIFFS_DBG
  { LSTRKEY( "debug" ), LFUNCVAL( file_debug ) },
#endif
#if SPIFFS_TEST_VISUALISATION
  { LSTRKEY( "fsvis" ), LFUNCVAL( file_vis ) },
#endif

#if LUA_OPTIMIZE_MEMORY > 0
#endif  
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_file(lua_State *L)
{
  //lua_spiffs_mount();  
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_FILE, file_map );
  return 1;
#endif
}
