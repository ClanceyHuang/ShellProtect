#include "studData.h"
#include "Stud\Stud.h"
#include "puPEinfoData.h"
#include "CompressionData.h"

PuPEInfo obj_pePe;
extern _Stud* g_stu;
extern char g_filenameonly[MAX_PATH];

#define NEWSECITONNAME ".VMP"

studData::studData()
{
	m_lpBase = obj_pePe.puGetImageBase();

#ifdef _WIN64
	m_dwNewSectionAddress64 = (DWORD64)obj_pePe.puGetSectionAddress((char *)m_lpBase, (BYTE *)".VMP");
#else
	m_dwNewSectionAddress = (DWORD)obj_pePe.puGetSectionAddress((char *)m_lpBase, (BYTE *)".VMP");
#endif


	m_Oep = obj_pePe.puOldOep();
}

studData::~studData()
{

} 

// Stud热身
BOOL studData::LoadLibraryStud()
{
	CompressionData obj_compress;

	m_studBase = obj_compress.puGetStubBase();
	// 获取dll的导出函数
#ifdef _WIN64
	dexportAddress = GetProcAddress((HMODULE)m_studBase, "vmentry");
#else
	dexportAddress = GetProcAddress((HMODULE)m_studBase, "main");
#endif
	// ImageBase
#ifdef _WIN64
	m_dwStudSectionAddress64 = (DWORD64)obj_pePe.puGetSectionAddress((char *)m_studBase, (BYTE *)".text");
	m_dwNewSectionAddress64 = (DWORD64)obj_pePe.puGetSectionAddress((char *)m_lpBase, (BYTE *)".VMP");
	m_ImageBase64 = ((PIMAGE_NT_HEADERS)obj_pePe.puGetNtHeadre())->OptionalHeader.ImageBase;
#else
	m_dwStudSectionAddress = (DWORD)obj_pePe.puGetSectionAddress((char *)m_studBase, (BYTE *)".text");
	m_dwNewSectionAddress = (DWORD)obj_pePe.puGetSectionAddress((char *)m_lpBase, (BYTE *)".VMP");
	m_ImageBase = ((PIMAGE_NT_HEADERS)obj_pePe.puGetNtHeadre())->OptionalHeader.ImageBase;
#endif // _WIN64

	return TRUE;
}

// 修复重定位
BOOL studData::RepairReloCationStud()
{
	PIMAGE_DOS_HEADER pStuDos = (PIMAGE_DOS_HEADER)m_studBase;
#ifdef _WIN64
	PIMAGE_NT_HEADERS pStuNt = (PIMAGE_NT_HEADERS)(pStuDos->e_lfanew + (DWORD64)m_studBase);
	PIMAGE_BASE_RELOCATION pStuRelocation = (PIMAGE_BASE_RELOCATION)(pStuNt->OptionalHeader.DataDirectory[5].VirtualAddress + (DWORD64)m_studBase);
#else
	PIMAGE_NT_HEADERS pStuNt = (PIMAGE_NT_HEADERS)(pStuDos->e_lfanew + (DWORD)m_studBase);
	PIMAGE_BASE_RELOCATION pStuRelocation = (PIMAGE_BASE_RELOCATION)(pStuNt->OptionalHeader.DataDirectory[5].VirtualAddress + (DWORD)m_studBase);
#endif // _WIN64

	typedef struct _Node
	{
		WORD offset : 12;
		WORD type : 4;
	}Node, *PNode;

#ifdef _WIN64
	LONGLONG dwDelta = (__int64)m_studBase - m_ImageBase64;
#endif
	DWORD OldAttribute = 0;
	while (pStuRelocation->SizeOfBlock)
	{
		DWORD nStuRelocationBlockCount = (pStuRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;

		_Node* RelType = (PNode)(pStuRelocation + 1);

		for (DWORD i = 0; i < nStuRelocationBlockCount; ++i)
		{
			if (RelType[i].type == 3) {
				DWORD* pRel = (DWORD *)(pStuRelocation->VirtualAddress + RelType[i].offset + (DWORD)m_studBase);

				VirtualProtect(pRel, 8, PAGE_READWRITE, &OldAttribute);

				*pRel = *pRel - (DWORD)m_studBase - ((PIMAGE_SECTION_HEADER)m_dwStudSectionAddress)->VirtualAddress + m_ImageBase + ((PIMAGE_SECTION_HEADER)m_dwNewSectionAddress)->VirtualAddress;

				VirtualProtect(pRel, 8, OldAttribute, &OldAttribute);
			}
#ifdef _WIN64
			if (RelType->type == 10) {
				PULONGLONG pAddress = (PULONGLONG)((DWORD64)m_studBase + pStuRelocation->VirtualAddress + RelType[i].offset);
				VirtualProtect(pAddress, 8, PAGE_READWRITE, &OldAttribute);
				*pAddress += dwDelta;
				//*pAddress = *pAddress - (DWORD64)m_studBase - ((PIMAGE_SECTION_HEADER)m_dwStudSectionAddress64)->VirtualAddress + ((PIMAGE_SECTION_HEADER)m_dwNewSectionAddress64)->VirtualAddress + m_ImageBase64;
				VirtualProtect(pAddress, 8, OldAttribute, &OldAttribute);
			}

#endif // _WIN64
		}
#ifdef _WIN64
		pStuRelocation = (PIMAGE_BASE_RELOCATION)((DWORD64)pStuRelocation + pStuRelocation->SizeOfBlock);
#else
		pStuRelocation = (PIMAGE_BASE_RELOCATION)((DWORD)pStuRelocation + pStuRelocation->SizeOfBlock);
#endif
	}
	return TRUE;
}

// 拷贝stud数据到新增区段
BOOL studData::CopyStud()
{
	g_stu->s_dwOepBase = m_Oep;

	FILE* fpFile = nullptr;

	if ((fpFile = fopen(g_filenameonly, "ab+")) == NULL)
		AfxMessageBox(L"文件打开失败");

	fwrite(&m_Oep, sizeof(DWORD), 1, fpFile);

	fclose(fpFile);

	PIMAGE_SECTION_HEADER studSection = obj_pePe.puGetSectionAddress((char *)m_studBase, (BYTE *)".text");

	PIMAGE_SECTION_HEADER SurceBase = obj_pePe.puGetSectionAddress((char *)m_lpBase, (BYTE *)".VMP");

#ifdef _WIN64
	memcpy(
		(void *)(SurceBase->PointerToRawData + (DWORD64)m_lpBase),
		(void *)(studSection->VirtualAddress + (DWORD64)m_studBase),
		studSection->Misc.VirtualSize
	);
#else
	memcpy(
		(void *)(SurceBase->PointerToRawData + (DWORD)m_lpBase),
		(void *)(studSection->VirtualAddress + (DWORD)m_studBase),
		studSection->Misc.VirtualSize
	);
#endif

	DWORD dwRiteFile = 0;	OVERLAPPED overLapped = { 0 };

	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)obj_pePe.puGetNtHeadre();

	// OEP地点
#ifdef _WIN64
	pNt->OptionalHeader.AddressOfEntryPoint = (DWORD64)dexportAddress - (DWORD64)m_studBase - studSection->VirtualAddress + SurceBase->VirtualAddress;
#else
	pNt->OptionalHeader.AddressOfEntryPoint = (DWORD)dexportAddress - (DWORD)m_studBase - studSection->VirtualAddress + SurceBase->VirtualAddress;
#endif

	int nRet = WriteFile(obj_pePe.puFileHandle(), obj_pePe.puGetImageBase(), obj_pePe.puFileSize(), &dwRiteFile, &overLapped);

	if (!nRet)
		return FALSE;

	return TRUE;
}