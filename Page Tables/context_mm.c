#include<context.h>
#include<memory.h>
#include<lib.h>
int count = 0;
u32 pages[15];
unsigned short regions[15];
unsigned short write[3];
u32 data_pfn;

void allocate(unsigned short segm, short* bits, u64* CR3_virtual_addr){
	
	u64 non_zero = ~0;
	u64* temp_addr;
	if(*(CR3_virtual_addr + bits[0]) == 0){
		u32 pfn = os_pfn_alloc(OS_PT_REG);
		temp_addr = osmap(pfn);
		pages[count] = pfn;
		regions[count] =0;
		count++;
		*(CR3_virtual_addr + bits[0]) = (pfn << 12) +7;
		for(int i=0;i<512;i++){
			*(temp_addr+i)=0x000;
		}
	}
	else{
		temp_addr = osmap((((*(CR3_virtual_addr + bits[0])) >> 12) & ~(non_zero << (40))));
	}

	if(*(temp_addr + bits[1]) == 0){
		u32 pfn = os_pfn_alloc(OS_PT_REG);
		pages[count] = pfn;
		regions[count] =0;
		count++;
		*(temp_addr + bits[1]) = (pfn << 12) +7;
		temp_addr = osmap(pfn);
		for(int i=0;i<512;i++){
			*(temp_addr+i)=0x000;
		}
	}
	else{
		temp_addr = osmap((((*(temp_addr + bits[1])) >> 12) & ~(non_zero << (40))));
	}

	if(*(temp_addr + bits[2]) == 0){
		u32 pfn = os_pfn_alloc(OS_PT_REG);
		pages[count] = pfn;
		regions[count] =0;
		count++;
		*(temp_addr + bits[2]) = (pfn << 12) +7;
		temp_addr = osmap(pfn);
		for(int i=0;i<512;i++){
			*(temp_addr+i)=0x000;
		}
	}
	else{
		temp_addr = osmap((((*(temp_addr + bits[2])) >> 12) & ~(non_zero << (40))));
	}

	u32 pfn;
	if(segm != 2){
		 pfn = os_pfn_alloc(USER_REG);
	}
	else{
		pfn = data_pfn;
	}
	pages[count] = pfn;
	regions[count] = 1;
	count++;
	u64 data;
	if(write[segm] == 1)
		data = (pfn << 12)+7;
	else
		data = (pfn <<12)+5;

	*(temp_addr + bits[3]) = data;

	return;
}
void prepare_context_mm(struct exec_context *ctx)
{
		 count=0;
	 		
		 short stack_bits[4];
		 short code_bits[4];
		 short data_bits[4];
		 
		for(int i=0;i<15;i++){
			regions[i] = 2;
		}


		struct mm_segment test;
		u32 CR3_base = os_pfn_alloc(OS_PT_REG);
		pages[count] = CR3_base ;
		regions[count] = 0;
		count++;
		ctx->pgd = CR3_base;
		u64* CR3_virtual_addr = osmap(CR3_base);
		for(int i=0;i<512;i++){
			*(CR3_virtual_addr+i)=0x000;
		}

		test = ctx->mms[MM_SEG_STACK];
		write[0] = (test.access_flags >> 1) & ~(~0 << (1));
		unsigned long t = test.end - 0x0800;
		stack_bits[0] = (t >> 39) & ~(~0 << (9)); 
		stack_bits[1] = (t >> 30) & ~(~0 << (9));
		stack_bits[2] = (t >> 21) & ~(~0 << (9));
		stack_bits[3] = (t >> 12) & ~(~0 << (9));
		allocate(0,stack_bits,CR3_virtual_addr);


		test = ctx->mms[MM_SEG_CODE];
		write[1] = (test.access_flags >> 1) & ~(~0 << (1));
		t = test.start;
		code_bits[0] = (t >> 39) & ~(~0 << (9)); 
		code_bits[1] = (t >> 30) & ~(~0 << (9));
		code_bits[2] = (t >> 21) & ~(~0 << (9));
		code_bits[3] = (t >> 12) & ~(~0 << (9));
		allocate(1,code_bits,CR3_virtual_addr);

		test = ctx->mms[MM_SEG_DATA];
		write[2] = (test.access_flags >> 1) & ~(~0 << (1));
		t = test.start;
		data_pfn = ctx->arg_pfn; 
		data_bits[0] = (t >> 39) & ~(~0 << (9)); 
		data_bits[1] = (t >> 30) & ~(~0 << (9));
		data_bits[2] = (t >> 21) & ~(~0 << (9));
		data_bits[3] = (t >> 12) & ~(~0 << (9));
		allocate(2,data_bits,CR3_virtual_addr);
		
        
   	return;
}

void cleanup_context_mm(struct exec_context *ctx)
{
	u64 temp;
	
	for(int i=0;i<count;i++){
		if(regions[i]==1){
			//printf("yes\n");
			temp = pages[i];
			os_pfn_free(USER_REG,temp);
		}
		else if(regions[i]==0){
			//printf("no\n");
			temp = pages[i];
			os_pfn_free(OS_PT_REG,temp);
		}
	}

   return;
}
