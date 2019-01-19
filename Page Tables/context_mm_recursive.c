#include<context.h>
#include<memory.h>
#include<lib.h>
int count = 0;
u32 pages[15];
unsigned short regions[15];
u64* stack_virtual[3];
u64* code_virtual[3];
u64* data_virtual[3];
unsigned short write[3];
short compare_bits[4];
u32 data_pfn;

void recursion(unsigned short step,unsigned short segm, short*segment1,short *segment2, short* segment3, u64* virtual_memory){
	if(step == 4){
		return;
	}
	if(segment1[step] == segment2[step] && segment1[step]==segment3[step] ){
		if(segm == 2){
			//printf("yes");
			data_virtual[step] = stack_virtual[step];
			recursion(step+1,segm,segment1,segment2,segment3,data_virtual[step]);  
		}
	}
	else if(segment1[step] != segment2[step] && segment1[step] != segment3[step]){
		u32 pfn;
		u64* temp = virtual_memory;
		u64 entry;
		if(step==3){
			if(segm == 2)
				pfn = data_pfn;
			else
				pfn = os_pfn_alloc(USER_REG);
			if(write[segm] == 1){
				entry = (pfn << 12)+7;
			}
			else
				entry = (pfn << 12)+5;
			*(virtual_memory + segment1[step]) = entry;
			regions[count]=1;
		}
		else{
			pfn = os_pfn_alloc(OS_PT_REG);
			temp = osmap(pfn);
			for(int i=0;i<512;i++){
				*(temp+i) = 0x000;
			}
			
			entry = (pfn<<12)+7;
			*(virtual_memory + segment1[step]) = entry;
			
			regions[count] = 0;
			if(segm ==0)
				stack_virtual[step] = temp;
			else if(segm==1)
				code_virtual[step] = temp;
			else if(segm == 2)
				data_virtual[step] = temp;
		}
		pages[count] = pfn;
		count++;
		recursion(step+1,segm,segment1,compare_bits,compare_bits,temp);
		

	}

	else if(segment1[step] == segment2[step] ){
		if(segm == 1)
			code_virtual[step] = stack_virtual[step];
		else if(segm == 2)
			data_virtual[step] = stack_virtual[step];

		recursion(step+1,segm,segment1,segment2,compare_bits,stack_virtual[step]);
	}

	else if(segment1[step] == segment3[step]){
		if(segm == 2)
			data_virtual[step] = code_virtual[step];

		recursion(step+1,segm,segment1,compare_bits,segment3,code_virtual[step]);

	}

	return;
}
void prepare_context_mm(struct exec_context *ctx)
{
		 count=0;
		 short stack_bits[4];
		 short code_bits[4];
		 short data_bits[4];
		 

		for(int i=0;i<4;i++){
			compare_bits[i] = -1;
		}
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
		recursion(0,0,stack_bits, compare_bits,compare_bits,CR3_virtual_addr);


		test = ctx->mms[MM_SEG_CODE];
		write[1] = (test.access_flags >> 1) & ~(~0 << (1));
		t = test.start;
		code_bits[0] = (t >> 39) & ~(~0 << (9)); 
		code_bits[1] = (t >> 30) & ~(~0 << (9));
		code_bits[2] = (t >> 21) & ~(~0 << (9));
		code_bits[3] = (t >> 12) & ~(~0 << (9));
		recursion(0,1,code_bits, stack_bits,compare_bits,CR3_virtual_addr);


		test = ctx->mms[MM_SEG_DATA];
		write[2] = (test.access_flags >> 1) & ~(~0 << (1));
		t = test.start;
		data_pfn = ctx->arg_pfn; 
		data_bits[0] = (t >> 39) & ~(~0 << (9)); 
		data_bits[1] = (t >> 30) & ~(~0 << (9));
		data_bits[2] = (t >> 21) & ~(~0 << (9));
		data_bits[3] = (t >> 12) & ~(~0 << (9));
		recursion(0,2,data_bits, stack_bits,code_bits,CR3_virtual_addr);
		//printf("%d\n",count);	
        
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
