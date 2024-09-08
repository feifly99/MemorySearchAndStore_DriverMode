#include "DebugeeHeader.h"
VOID KernelDriverThreadSleep(
    LONG msec
)
{
    LARGE_INTEGER my_interval;
    my_interval.QuadPart = DELAY_ONE_MILLISECOND;
    my_interval.QuadPart *= msec;
    KeDelayExecutionThread(KernelMode, 0, &my_interval);
}
PVAL createValidAddressNode(
    ULONG64 begin, 
    ULONG64 end, 
    ULONG memState, 
    ULONG memProtectAttributes, 
    BOOLEAN executeFlag
)
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
PRSL createSavedResultNode(
    ULONG times, 
    ULONG64 address
)
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
VOID getRegionGapAndPages(
    PVAL headVAL
)
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
ULONG64 getMaxRegionPages(
    PVAL head
)
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
VOID computeLPSArray(
    CONST UCHAR* pattern, 
    UL64 M, 
    UL64* lps
)
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
VOID KMP_searchPattern(
    CONST UCHAR* des,
    CONST UCHAR* pattern, 
    SIZE_T desLen, 
    SIZE_T patLen, 
    ULONG64 pageBeginAddress, 
    UL64* lpsAddress, 
    PRSL* headRSL
)
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
            //DbgPrint("在地址%llx匹配成功\n", (ULONG64)(pageBeginAddress + i - j));
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
BOOLEAN isSame(
    PUCHAR A, 
    PUCHAR B, 
    SIZE_T size
)
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
VOID printListVAL(
    PVAL headVAL
)
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
VOID printListRSL(
    PRSL headRSL
)
{
    PRSL temp = headRSL;
    while (temp->ResultAddressEntry.Flink != &headRSL->ResultAddressEntry)
    {
        DbgPrint("times: %ld, address: %p", temp->times, (PVOID)temp->address);
        temp = CONTAINING_RECORD(temp->ResultAddressEntry.Flink, RSL, ResultAddressEntry);
    }
    DbgPrint("times: %ld, address: %p", temp->times, (PVOID)temp->address);
}
VOID ReadBuffer(
    PVOID bufferHead, 
    SIZE_T size
)
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
VOID buildValidAddressSingleList(
    PHANDLE phProcess,
    PMEMORY_INFORMATION_CLASS pMIC, 
    PMEMORY_BASIC_INFORMATION pmbi, 
    PVAL* headVAL, 
    ULONG64 addressMaxLimit
)
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
VOID buildDoubleLinkedAddressListForPatternStringByKMPAlgorithm(
    PVAL headVAL, 
    PPEPROCESS pPe, 
    PUCHAR pattern, 
    SIZE_T patternLen, 
    PRSL* headRSL
)
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
                goto M; //此时出现bugcheck导致KMP搜索不会执行，因此也不会分配next数组内存，因此不用ExFreePool(addressNeedFree)，直接进行下一个链表节点就行了。
                //[!]【在except结束后，要么加上return STATUS_UNSUCCESSFUL！要么goto到下一块！双重detachAPC会蓝屏，而且此BUG不是次次都有，不定时出现！】
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
VOID processHiddenProcedure(
    ULONG64 pid
)
{
    PEPROCESS pe = IoGetCurrentProcess();
    ULONG64 UniqueProcessIdOffset = 0x440;
    ULONG64 ActiveProcessLinksOffset = 0x448;
    PLIST_ENTRY thisPeNode = NULL;
    ULONG64 initialPID = *(ULONG64*)((ULONG64)pe + UniqueProcessIdOffset);
    while (*(ULONG64*)((ULONG64)pe + UniqueProcessIdOffset) != pid)
    {
        thisPeNode = (PLIST_ENTRY)((ULONG64)pe + ActiveProcessLinksOffset);
        pe = (PEPROCESS)((UL64)thisPeNode->Flink - ActiveProcessLinksOffset);
    }
    DbgPrint("***%p***", *(HANDLE*)((ULONG64)pe + UniqueProcessIdOffset));
    //这个pe就是目标pid的进程，接下来是断链隐藏
    PLIST_ENTRY currPeListEntryAddress = (PLIST_ENTRY)((UL64)pe + ActiveProcessLinksOffset);
    PLIST_ENTRY prevPeListEntryAddress = currPeListEntryAddress->Blink;
    PLIST_ENTRY nextPeListEntryAddress = currPeListEntryAddress->Flink;
    prevPeListEntryAddress->Flink = nextPeListEntryAddress;
    nextPeListEntryAddress->Blink = prevPeListEntryAddress;
    DbgPrint("进程0x%p(%llu)已经断链隐藏.", (PVOID)pid, pid);
    return;
}
VOID displayAllModuleInfomationByProcessId(
    ULONG64 pid
)
{
    PEPROCESS pe = NULL;
    PsLookupProcessByProcessId((HANDLE)pid, &pe);
    KAPC_STATE apc = { 0 };
    KeStackAttachProcess(pe, &apc);
    ULONG64 pebAddress = (UL64)pe + 0x550;
    ULONG64 peb = *(ULONG64*)pebAddress;
    ULONG64 ldrAddress = peb + 0x18;
    ULONG64 ldr = *(ULONG64*)ldrAddress;
    ULONG64 InLoadOrderModuleListAddress = ldr + 0x10;
    PLIST_ENTRY entry = (PLIST_ENTRY)InLoadOrderModuleListAddress;
    ULONG64 initialEntryAddress = (UL64)entry;
M:
    DbgPrint("%wZ", (PUNICODE_STRING)((UL64)(((PLIST_ENTRY)entry)->Flink) + 0x58));
    entry = entry->Flink;
    //如果只是initialEntryAddress的话，最后一个UNICODE_STRING会打印NULL，非常危险.
    //原因：entry是LIST_ENTRY的头节点，不附带任何属性信息；
    //从entry = entry->Flink开始才是起点，entry = entry->Blink是终点。
    if ((UL64)entry != (UL64)(((PLIST_ENTRY)initialEntryAddress)->Blink))
    {
        goto M;
    }
    else
    {
        goto G;
    }
G:
    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(pe);
    return;
}
VOID ExFreeResultSavedLink(
    PRSL* headRSL
)
{
    PRSL tempRSL = *headRSL;
    while (tempRSL != NULL && tempRSL->ResultAddressEntry.Flink != NULL)
    {
        //[!]一定要有tempRSL != NULL这句！因为当temp == NULL的时候，tempRSL->ResultAddressEntry.Flink != NULL隐含了一个指针访问操作，会蓝屏！！
        PRSL tempX = CONTAINING_RECORD(tempRSL->ResultAddressEntry.Flink, RSL, ResultAddressEntry);
        tempRSL->ResultAddressEntry.Flink = NULL;
        tempRSL->ResultAddressEntry.Blink = NULL;
        ExFreePool(tempRSL); tempRSL = NULL;
        tempRSL = tempX;
    }
}
VOID ExFreeValidAddressLink(
    PVAL* headVAL
)
{
    PVAL temp = *headVAL;
    while (temp != NULL && temp->ValidAddressEntry.Next != NULL)
    {
        PVAL tempX = CONTAINING_RECORD(temp->ValidAddressEntry.Next, VAL, ValidAddressEntry);
        ExFreePool(temp); temp = NULL;
        temp = tempX;
    }
}
