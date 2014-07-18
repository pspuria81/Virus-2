#include<windows.h>
#include <stdio.h>
#include <stdlib.h>
PBYTE pRemoteCode, pCode, pOrignCode;
DWORD dwSizeOfCode;
DWORD GetBaseKernel32();
DWORD GetGetProcAddrBase(DWORD base);
__declspec(naked) void code_start()
{
__asm{	
		push ebp
		mov ebp,esp
		push ebx
		call _delta
	_delta:
		pop ebx
		sub ebx,offset _delta
		lea ecx,[ebx+GetBaseKernel32]
		call ecx		//调用得到kernel32.dll地址的函数
		lea ecx,[ebx+GetGetProcAddrBase]
		push eax
		call ecx		//调用得到GetGetProcAddrBase地址的函数
		mov esp,ebp
		pop ebp
		ret	
	}	
}


__declspec(naked) DWORD GetBaseKernel32()
{
__asm{
        push    ebp
        mov     ebp, esp
        push    esi
        push    edi
        xor     ecx, ecx                    // ECX = 0
        mov     esi, fs:[0x30]              // ESI = &(PEB) ([FS:0x30])
        mov     esi, [esi + 0x0c]           // ESI = PEB->Ldr
        mov     esi, [esi + 0x1c]           // ESI = PEB->Ldr.InInitOrder
	_next_module:
        mov     eax, [esi + 0x08]           // EBP = InInitOrder[X].base_address
        mov     edi, [esi + 0x20]           // EBP = InInitOrder[X].module_name (unicode)
        mov     esi, [esi]                  // ESI = InInitOrder[X].flink (next module)
        cmp     [edi + 12*2], cx            // modulename[12] == 0 ?
        jne     _next_module                 // No: try next module.
        pop     edi
        pop     esi
        mov     esp, ebp
        pop     ebp
        ret
	
	
	}


}
__declspec(naked) DWORD GetGetProcAddrBase(DWORD base)
{
	__asm{

        push    ebp
        mov     ebp, esp
        push    edx
        push    ebx
        push    edi
        push    esi
        mov     ebx, [ebp+8]
        mov     eax, [ebx + 0x3c] // edi = BaseAddr, eax = pNtHeader
        mov     edx, [ebx + eax + 0x78]
        add     edx, ebx          // edx = Export Table (RVA)
        mov     ecx, [edx + 0x18] // ecx = NumberOfNames
        mov     edi, [edx + 0x20] //
        add     edi, ebx          // ebx = AddressOfNames
	_search:
        dec     ecx
        mov     esi, [edi + ecx*4]
        add     esi, ebx
        mov     eax, 0x50746547 // "PteG"
        cmp     [esi], eax
        jne     _search
        mov     eax, 0x41636f72 //"Acor"
        cmp     [esi+4], eax
        jne     _search
        mov     edi, [edx + 0x24] //
        add     edi, ebx      // edi = AddressOfNameOrdinals
        mov     cx, word ptr [edi + ecx*2]  // ecx = GetProcAddress-orinal
        mov     edi, [edx + 0x1c] //
        add     edi, ebx      // edi = AddressOfFunction
        mov     eax, [edi + ecx*4]
        add     eax, ebx      // eax = GetProcAddress
        
        pop     esi
        pop     edi
        pop     ebx
        pop     edx
        
        mov     esp, ebp
        pop     ebp
        ret
	}


}

__declspec(naked) void code_end()
{
    __asm _emit 0xCC
}


DWORD make_code()
{
		
	__asm {
        mov edx, offset code_start
        mov dword ptr [pOrignCode], edx
        mov eax, offset code_end
        sub eax, edx
        mov dword ptr [dwSizeOfCode], eax
    }
	
	return 	dwSizeOfCode;
}
int inject_code(DWORD pid)
{

	DWORD hproc, hthrd;
	DWORD sizeCode;
	int numx;
	int rstr;//远程分配内存的首地址
	int addr;
	int TID=0;
	hproc = OpenProcess(
          PROCESS_CREATE_THREAD  | PROCESS_QUERY_INFORMATION
        | PROCESS_VM_OPERATION   | PROCESS_VM_WRITE 
        | PROCESS_VM_READ, FALSE, pid);
	if (!hproc) printf("OpenProc Error\n");
	sizeCode=(DWORD)code_end-(DWORD)code_start;
	printf("%d\n",sizeCode);
	rstr = (PBYTE) VirtualAllocEx(hproc, 
        0, sizeCode, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(hproc, rstr, code_start, sizeCode, &numx);
	hthrd = CreateRemoteThread(hproc, 
      NULL, 0, (LPTHREAD_START_ROUTINE)rstr,//建立远程线程执行code代码
            0, 0 , &TID);
	WaitForSingleObject(hthrd, INFINITE);
	GetExitCodeThread(hthrd, &addr);		
	printf("addr of GetGetProcAddrBase: 0x%08x\n",addr);
	return 0;
	



}

int main(int argc,char* argv[]){
	
	
	int pid = atoi(argv[1]);
	printf("pid is %d\n",pid);
	//make_code();
	inject_code(pid);
	return 0;
}


