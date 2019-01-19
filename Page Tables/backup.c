#include<context.h>
#include<memory.h>
#include<lib.h>
u32 code_l4,code_l3,code_l2,code_l1,code_level1,code_level2,code_level3,stack_l4,stack_l3,stack_l2,stack_l1,stack_level1,stack_level2,stack_level3,code_datapage;
u32 stack_datapage;

void recursion(int step, u64* virtual_memory, int write, int *translation_step){

}
void clean_recursion(){

}
void prepare_context_mm(struct exec_context *ctx)
{
		struct mm_segment test;
		u32 CR3_base = os_pfn_alloc(OS_PT_REG);
		ctx->pgd = CR3_base;
		u64* CR3_virtual_addr = osmap(CR3_base);
		test = ctx->mms[MM_SEG_STACK];
		int write = 0;
		write = (test.access_flags >> 1) & ~(~0 << (1));
		unsigned long t = test.end - 0x0800;
		stack_l4 = (t >> 39) & ~(~0 << (9)); 
		stack_l3 = (t >> 30) & ~(~0 << (9));
		stack_l2 = (t >> 21) & ~(~0 << (9));
		stack_l1 = (t >> 12) & ~(~0 << (9));

        stack_level1 = os_pfn_alloc(OS_PT_REG);
	   	u64* stack_level1_virtual = osmap(stack_level1);
	   	stack_level2 = os_pfn_alloc(OS_PT_REG);
	   	u64* stack_level2_virtual = osmap(stack_level2);
	   	stack_level3 = os_pfn_alloc(OS_PT_REG);
	   	u64* stack_level3_virtual = osmap(stack_level3);
	   	u64 temp;
	   	temp = (stack_level1 << 12) + 7;
	   	*(CR3_virtual_addr + (stack_l4)) = temp;
	   	temp = (stack_level2 << 12) + 7;
	   	*(stack_level1_virtual + (stack_l3)) = temp;
	   	temp = (stack_level3 << 12) + 7;
	   	*(stack_level2_virtual + (stack_l2)) = temp;
	    stack_datapage = os_pfn_alloc(USER_REG);
	   	if(write == 1){
	   		temp = (stack_datapage << 12) + 7;
	   	}			
	   	else{
	   		temp = (stack_datapage << 12) + 5;
	   	}
	   	*(stack_level3_virtual + (stack_l1)) =temp;


	   	test = ctx->mms[MM_SEG_CODE];
	   	t = test.start;
	   	code_l4 = (t >> 39) & ~(~0 << (9)); 
		code_l3 = (t >> 30) & ~(~0 << (9));
		code_l2 = (t >> 21) & ~(~0 << (9));
		code_l1 = (t >> 12) & ~(~0 << (9));
		write = (test.access_flags >> 1) & ~(~0 << (1));
		printf("%x\n",test.access_flags);
		if((code_l4 == stack_l4) && (code_l3!=stack_l3)){
			printf("yes");
			code_level2 = os_pfn_alloc(OS_PT_REG);
			code_level3 = os_pfn_alloc(OS_PT_REG);
			code_datapage = os_pfn_alloc(OS_PT_REG);

			u64* code_virtual1 = osmap(code_level2);
			u64* code_virtual2 = osmap(code_level3);

			temp = (code_level2 << 12) + 7;
			*(stack_level1_virtual + (code_l3)) = temp;
			temp = (code_level3 << 12) + 7; 
			*(code_virtual1 + (code_l2)) = temp;
			if(write == 1){
				temp =  (code_datapage << 12) + 7;
			}
			else{
				temp = (code_datapage << 12) + 5;
			}
			//temp = (code_datapage << 12) + 5;
			*(code_virtual2 + (code_l1)) = temp;	
		}

   	return;
}
void cleanup_context_mm(struct exec_context *ctx)
{
   return;
}
