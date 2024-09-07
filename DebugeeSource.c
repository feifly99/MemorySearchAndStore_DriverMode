#include "DebugeeHeader.h"
VOID KernelDriverThreadSleep(LONG msec)
{
    LARGE_INTEGER my_interval;
    my_interval.QuadPart = DELAY_ONE_MILLISECOND;
    my_interval.QuadPart *= msec;
    KeDelayExecutionThread(KernelMode, 0, &my_interval);
}
PVAL createValidAddressNode(ULONG64 begin, ULONG64 end, ULONG memState, ULONG memProtectAttributes, BOOLEAN executeFlag)
{
    PVAL newNode = (PVAL)ExAllocatePoolWithTag(PagedPool, sizeof(VAL), 'WWWW');
    if (newNode)
    {
        newNode->beginAddress = begin;
        newNode->endAddress = end;
        newNode->memoryState = memState;
        newNode->memoryProtectAttributes = memProtectAttributes;
        newNode->executeFlag = executeFlag;
        newNode->regionGap = 0x0;
        newNode->pageNums = 0x0;
        newNode->ValidAddressEntry.Next = NULL;
    }
    return newNode;
}
PRSL createSavedResultNode(ULONG times, ULONG64 address)
{
    PRSL newNode = (PRSL)ExAllocatePoolWithTag(PagedPool, sizeof(RSL), 'VVVV');
    if (newNode)
    {
        newNode->times = times;
        newNode->address = address;
        newNode->ResultAddressEntry.Flink = NULL;
        newNode->ResultAddressEntry.Blink = NULL;
    }
    return newNode;
}
VOID getRegionGapAndPages(PVAL headVAL)
{
    PVAL temp = headVAL;
    while (temp->ValidAddressEntry.Next != NULL)
    {
        temp->regionGap = temp->endAddress - temp->beginAddress;
        temp->pageNums = (temp->regionGap / 0x1000) + 1;
        temp = CONTAINING_RECORD(temp->ValidAddressEntry.Next, VAL, ValidAddressEntry);
    }
    return;
}
ULONG64 getMaxRegionPages(PVAL head)
{
    PVAL temp = head;
    ULONG64 ret = 0x0;
    while (temp->ValidAddressEntry.Next != NULL)
    {
        if (temp->pageNums >= ret)
        {
            ret = temp->pageNums;
        }
        temp = CONTAINING_RECORD(temp->ValidAddressEntry.Next, VAL, ValidAddressEntry);
    }
    return ret;
}
VOID computeLPSArray(CONST UCHAR* pattern, UL64 M, UL64* lps)
{
    UL64 len = 0;
    lps[0] = 0;
    SIZE_T i = 1;
    while (i < M)
    {
        if (pattern[i] == pattern[len])
        {
            len++;
            lps[i] = len;
            i++;
        }
        else
        {
            if (len != 0)
            {
                len = lps[len - 1];
            }
            else
            {
                lps[i] = len;
                i++;
            }
        }
    }
}
VOID KMP_searchPattern(CONST UCHAR* des, CONST UCHAR* pattern, SIZE_T desLen, SIZE_T patLen, ULONG64 pageBeginAddress, UL64* lpsAddress, PRSL* headRSL)
{
    UL64 M = patLen;
    UL64 N = desLen;
    UL64* lps = (UL64*)ExAllocatePoolWithTag(PagedPool, M * sizeof(UL64), 'wwww');
    UL64 j = 0;
    computeLPSArray(pattern, M, lps);
    SIZE_T i = 0;
    while (i < N)
    {
        if (pattern[j] == des[i])
        {
            j++;
            i++;
        }
        if (j == M && lps)
        {
            //DbgPrint("�ڵ�ַ%llxƥ��ɹ�\n", (ULONG64)(pageBeginAddress + i - j));
            if (*headRSL == NULL)
            {
                *headRSL = createSavedResultNode(1, (ULONG64)(pageBeginAddress + i - j));
                if (*headRSL)
                {
                    (*headRSL)->ResultAddressEntry.Flink = &((*headRSL)->ResultAddressEntry);
                    (*headRSL)->ResultAddressEntry.Blink = (*headRSL)->ResultAddressEntry.Flink;
                }
            }
            else
            {
                PRSL temp = *headRSL;
                while (temp->ResultAddressEntry.Flink != &((*headRSL)->ResultAddressEntry))
                {
                    temp = CONTAINING_RECORD(temp->ResultAddressEntry.Flink, RSL, ResultAddressEntry);
                }
                PRSL newNode = createSavedResultNode(1, (ULONG64)(pageBeginAddress + i - j));
                if (newNode)
                {
                    temp->ResultAddressEntry.Flink = &newNode->ResultAddressEntry;
                    newNode->ResultAddressEntry.Flink = &((*headRSL)->ResultAddressEntry);
                    newNode->ResultAddressEntry.Blink = &temp->ResultAddressEntry;
                    (*headRSL)->ResultAddressEntry.Blink = &newNode->ResultAddressEntry;
                }
            }
            j = lps[j - 1];
        }
        else if (i < N && pattern[j] != des[i])
        {
            if (j != 0 && lps)
            {
                j = lps[j - 1];
            }
            else
            {
                i = i + 1;
            }
        }
    }
    *lpsAddress = (UL64)lps;
}
BOOLEAN isSame(PUCHAR A, PUCHAR B, SIZE_T size)
{
    for (size_t j = 0; j < size; j++)
    {
        if (A[j] != B[j])
        {
            return 0;
        }
        else
        {
            continue;
        }
    }
    return 1;
}
VOID printListVAL(PVAL headVAL)
{
    size_t cnt = 0;
    PVAL temp = headVAL;
    while (temp->ValidAddressEntry.Next != NULL)
    {
        cnt++;
        DbgPrint("ListNodeIndex: 0x%llx, begin: 0x%p\t end: 0x%p\t regionGap: 0x%llx\t pageNums: 0x%llx\t memState: %lx\t memProtect: %lx\t", cnt, (PVOID)temp->beginAddress, (PVOID)temp->endAddress, temp->regionGap, temp->pageNums, temp->memoryState, temp->memoryProtectAttributes);
        temp = CONTAINING_RECORD(temp->ValidAddressEntry.Next, VAL, ValidAddressEntry);
    }
    return;
}
VOID printListRSL(PRSL headRSL)
{
    PRSL temp = headRSL;
    while (temp->ResultAddressEntry.Flink != &headRSL->ResultAddressEntry)
    {
        DbgPrint("times: %ld, address: %p", temp->times, (PVOID)temp->address);
        temp = CONTAINING_RECORD(temp->ResultAddressEntry.Flink, RSL, ResultAddressEntry);
    }
    DbgPrint("times: %ld, address: %p", temp->times, (PVOID)temp->address);
}
VOID ReadBuffer(PVOID bufferHead, SIZE_T size)
{
    ULONG64 temp = (ULONG64)bufferHead;
    for (size_t j = 0; j < size - 16; j += 16)
    {
        DbgPrint("%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t%hhx\t",
            *(UCHAR*)(temp + j + 0),
            *(UCHAR*)(temp + j + 1),
            *(UCHAR*)(temp + j + 2),
            *(UCHAR*)(temp + j + 3),
            *(UCHAR*)(temp + j + 4),
            *(UCHAR*)(temp + j + 5),
            *(UCHAR*)(temp + j + 6),
            *(UCHAR*)(temp + j + 7),
            *(UCHAR*)(temp + j + 8),
            *(UCHAR*)(temp + j + 9),
            *(UCHAR*)(temp + j + 10),
            *(UCHAR*)(temp + j + 11),
            *(UCHAR*)(temp + j + 12),
            *(UCHAR*)(temp + j + 13),
            *(UCHAR*)(temp + j + 14),
            *(UCHAR*)(temp + j + 15)
        );
    }
}
VOID buildValidAddressSingleList(PHANDLE phProcess,PMEMORY_INFORMATION_CLASS pMIC, PMEMORY_BASIC_INFORMATION pmbi, PVAL* headVAL, ULONG64 addressMaxLimit)
{
    ULONG64 currentAddress = 0x0;
    PVAL temp = NULL;
    ULONG64 writeAddressLen = 0x0;
    while (currentAddress <= addressMaxLimit)
    {
        if (NT_SUCCESS(ZwQueryVirtualMemory(*phProcess, (PVOID)currentAddress, *pMIC, pmbi, sizeof(MEMORY_BASIC_INFORMATION), &writeAddressLen)))
        {
            if (pmbi->Protect != 0x00 && pmbi->Protect != 0x01 && pmbi->Protect != 0x104 && pmbi->Protect != 0x100)
            {
                if (*headVAL == NULL)
                {
                    if (pmbi->Protect == 0x10)
                    {
                        PVAL newNode = createValidAddressNode((UL64)pmbi->BaseAddress, (UL64)pmbi->BaseAddress + (UL64)pmbi->RegionSize - 1, (UL64)pmbi->State, (UL64)pmbi->Protect, 1);
                        *headVAL = newNode;
                    }
                    else
                    {
                        PVAL newNode = createValidAddressNode((UL64)pmbi->BaseAddress, (UL64)pmbi->BaseAddress + (UL64)pmbi->RegionSize - 1, (UL64)pmbi->State, (UL64)pmbi->Protect, 0);
                        *headVAL = newNode;
                    }
                }
                else
                {
                    temp = *headVAL;
                    while (temp->ValidAddressEntry.Next != NULL)
                    {
                        temp = CONTAINING_RECORD(temp->ValidAddressEntry.Next, VAL, ValidAddressEntry);
                    }
                    if (pmbi->Protect == 0x10)
                    {
                        PVAL newNode = createValidAddressNode((UL64)pmbi->BaseAddress, (UL64)pmbi->BaseAddress + (UL64)pmbi->RegionSize - 1, (UL64)pmbi->State, (UL64)pmbi->Protect, 1);
                        temp->ValidAddressEntry.Next = &newNode->ValidAddressEntry;
                    }
                    else
                    {
                        if (temp->endAddress + 0x1 == (UL64)pmbi->BaseAddress)
                        {
                            temp->endAddress += pmbi->RegionSize;
                        }
                        else
                        {
                            PVAL newNode = createValidAddressNode((UL64)pmbi->BaseAddress, (UL64)pmbi->BaseAddress + (UL64)pmbi->RegionSize - 1, (UL64)pmbi->State, (UL64)pmbi->Protect, 0);
                            temp->ValidAddressEntry.Next = &newNode->ValidAddressEntry;
                        }
                    }
                }
            }
        }
        currentAddress = (ULONG64)pmbi->BaseAddress + pmbi->RegionSize;
    }
}
VOID buildDoubleLinkedAddressListForPatternStringByKMPAlgorithm(PVAL headVAL, PPEPROCESS pPe, PUCHAR pattern, SIZE_T patternLen, PRSL* headRSL)
{
    PVAL temp = headVAL;
    KAPC_STATE apc = { 0 };
    while (temp->ValidAddressEntry.Next != NULL)
    {
        UCHAR* bufferReceive = (UCHAR*)ExAllocatePoolWithTag(PagedPool, temp->pageNums * 4096, 'TTTT');
        if (bufferReceive)
        {
            KeStackAttachProcess(*pPe, &apc);
            UL64 addressNeedFree = 0x0;
            __try
            {
                memcpy(bufferReceive, (PVOID)temp->beginAddress, temp->pageNums * 4096);
            }
            __except (1)
            {
                KeUnstackDetachProcess(&apc);
                ObDereferenceObject(*pPe);
                ExFreePool(bufferReceive);
                goto M; //��ʱ����bugcheck����KMP��������ִ�У����Ҳ�������next�����ڴ棬��˲���ExFreePool(addressNeedFree)��ֱ�ӽ�����һ������ڵ�����ˡ�
                //[!]����except������Ҫô����return STATUS_UNSUCCESSFUL��Ҫôgoto����һ�飡˫��detachAPC�����������Ҵ�BUG���Ǵδζ��У�����ʱ���֣���
            }
            KeUnstackDetachProcess(&apc);
            ObDereferenceObject(*pPe);
            KMP_searchPattern(bufferReceive, pattern, temp->pageNums * 4096, patternLen, temp->beginAddress, &addressNeedFree, headRSL);
            ExFreePool((PVOID)bufferReceive); bufferReceive = NULL;
            ExFreePool((PVOID)addressNeedFree); addressNeedFree = (UL64)NULL;
            M:temp = CONTAINING_RECORD(temp->ValidAddressEntry.Next, VAL, ValidAddressEntry);
        }
        else
        {
            break;
        }
    }
}
VOID ExFreeResultSavedLink(PRSL* headRSL)
{
    PRSL tempRSL = *headRSL;
    while (tempRSL != NULL && tempRSL->ResultAddressEntry.Flink != NULL)
    {
        //[!]һ��Ҫ��tempRSL != NULL��䣡��Ϊ��temp == NULL��ʱ��tempRSL->ResultAddressEntry.Flink != NULL������һ��ָ����ʲ���������������
        PRSL tempX = CONTAINING_RECORD(tempRSL->ResultAddressEntry.Flink, RSL, ResultAddressEntry);
        tempRSL->ResultAddressEntry.Flink = NULL;
        tempRSL->ResultAddressEntry.Blink = NULL;
        ExFreePool(tempRSL); tempRSL = NULL;
        tempRSL = tempX;
    }
}
VOID ExFreeValidAddressLink(PVAL* headVAL)
{
    PVAL temp = *headVAL;
    while (temp != NULL && temp->ValidAddressEntry.Next != NULL)
    {
        PVAL tempX = CONTAINING_RECORD(temp->ValidAddressEntry.Next, VAL, ValidAddressEntry);
        ExFreePool(temp); temp = NULL;
        temp = tempX;
    }
}