#include<init.h>
#include<lib.h>
#include<context.h>
#include<memory.h>
/*System Call handler*/
u64 top;
u64 rap;
 long do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
{
    struct exec_context *current = get_current_ctx();
    printf("[GemOS] System call invoked. syscall no  = %d\n", syscall);   
    switch(syscall)
    {
          
          case SYSCALL_EXIT:
                              printf("[GemOS] exit code = %d\n", (int) param1);
                              do_exit();
                              break;
          case SYSCALL_GETPID:
                              printf("[GemOS] getpid called for process %s, with pid = %d\n", current->name, current->id);
                              return current->id;      
          case SYSCALL_WRITE:
                             {
                              short address_bits[4];
                              u64* CR3 = osmap(current->pgd);
                              //printf("addr %x\n",param1 );
                               if(param2>1024){
                          
                                  return -1;
                                }  
                              
                            
                              else{
                                u64 temp = param1;
                                for(int i=0;i<param2;i++){
                                  temp = param1+i;
                                  address_bits[0] = ( temp >> 39) & ~(~0 << (9));
                                  address_bits[1] = ( temp >> 30) & ~(~0 << (9));
                                  address_bits[2] = ( temp >> 21) & ~(~0 << (9));
                                  address_bits[3] = ( temp >> 12) & ~(~0 << (9));
                                  u64* temp_addr = CR3;
                                  for(int j=0;j<4;j++){
                                    u64 value = *(temp_addr+address_bits[j]);
                                    short present = value & 1;
                                    short perm = value & 4;

                                    if( value ==0 || perm ==0 ){
                                      printf("Tried Illegal Address access\n" );
                                      //printf("perm val %x\n %x\n",perm,value);
                                      return -1;
                                    }
                                    u32 pfn = (value >> 12) & ~((long int)~0 << (40));
                                    temp_addr = osmap(pfn); 
                                  }
                                }
                                char* addr = (char*)param1;

                                for(int i=0;i<param2;i++){
                                  printf("%c",*(addr+i) );
                                }
                                return param2; 
                              }
                                     /*Your code goes here*/
                             }
          case SYSCALL_EXPAND:
                             {  
                              
                                if(param1>512){
                                  return -1;
                                }

                                if(param2 == MAP_RD){
                                  u64 end = (current->mms[MM_SEG_RODATA]).end;
                                  u64 next_free = (current->mms[MM_SEG_RODATA]).next_free;
                                  //printf("next %x\n st %x\n", next_free,(current->mms[MM_SEG_RODATA]).start);
                                  u64 final = next_free + (param1<<12);
                                  if(final>end){
                                    return -1;
                                  }
                                  (current->mms[MM_SEG_RODATA]).next_free = final;
                                  return next_free;

                                }
                                else if(param2 == MAP_WR){
                                  u64 end = (current->mms[MM_SEG_DATA]).end;
                                  u64 next_free = (current->mms[MM_SEG_DATA]).next_free;
                                  
                                  u64 final = next_free + (param1<<12);
                                  if(final>end){
                                    return -1;
                                  }
                                  (current->mms[MM_SEG_DATA]).next_free = final;
                                  
                                  return next_free;
                                }


                                     /*Your code goes here*/
                             }
          case SYSCALL_SHRINK:
                             {  
                                       /*Your code goes here*/
                                if(param2 == MAP_RD){
                                  u64 start = (current->mms[MM_SEG_RODATA]).start;
                                  u64 next_free = (current->mms[MM_SEG_RODATA]).next_free;
                                  
                                  u64 final = next_free - (param1<<12);
                                  if(final<start){
                                    return -1;
                                  }
                                  u64* CR3 = osmap(current->pgd);
                                  short address_bits[4];
                                  for(int i=0;i<param1;i++){
                                    u64 temp = final + (i<<12);
                                    u64* temp_addr = CR3;
                                    address_bits[0] = ( temp >> 39) & ~(~0 << (9));
                                    address_bits[1] = ( temp >> 30) & ~(~0 << (9));
                                    address_bits[2] = ( temp >> 21) & ~(~0 << (9));
                                    address_bits[3] = ( temp >> 12) & ~(~0 << (9));
                                    for(int j=0;j<4;j++){
                                      u64 value = *(temp_addr+address_bits[j]);
                                      short present = value & 1;
                                      if(present==0){
                                        break;
                                      }
                                      u32 pfn = (value >> 12) & ~((long int)~0 << (40));
                                      if(j==3){
                                        *(temp_addr+address_bits[3])=0;
                                      }
                                      temp_addr = osmap(pfn);
                                      if(j==3){
                                        
                                        os_pfn_free(USER_REG,pfn);
                                        asm volatile("invlpg (%0)" ::"r" (temp) : "memory");
                                      } 
                                    }
                                  }
                                  (current->mms[MM_SEG_RODATA]).next_free = final;
                                  return final;

                                }
                                else if(param2 == MAP_WR){
                                  

                                  u64 start = (current->mms[MM_SEG_DATA]).start;
                                  u64 next_free = (current->mms[MM_SEG_DATA]).next_free;
                                  u64 final = next_free - (param1<<12);
                                  
                                  if(final<start){
                                    return -1;
                                  }
                                  u64* CR3 = osmap(current->pgd);
                                  short address_bits[4];
                                  
                                  for(int i=0;i<param1;i++){

                                    
                                    u64 temp = final + (i<<12);
                                    u64* temp_addr = CR3;
                                    address_bits[0] = ( temp >> 39) & ~(~0 << (9));
                                    address_bits[1] = ( temp >> 30) & ~(~0 << (9));
                                    address_bits[2] = ( temp >> 21) & ~(~0 << (9));
                                    address_bits[3] = ( temp >> 12) & ~(~0 << (9));
                                    for(int j=0;j<4;j++){

                                      u64 value = *(temp_addr+address_bits[j]);
                                      short present = (value >> 0) & ~(~0 << (1));
                                      if(present==0){
                                        
                                        break;
                                      }
                                      u32 pfn = (value >> 12) & ~((long int)~0 << (40));
                                      if(j==3){
                                        *(temp_addr+address_bits[3])=0;
                                      }
                                      temp_addr = osmap(pfn);
                                      if(j==3){
                                      
                                        os_pfn_free(USER_REG,pfn);
                                        asm volatile("invlpg (%0)" ::"r" (temp) : "memory");
                                      } 
                                    }
                                  }
                                  (current->mms[MM_SEG_DATA]).next_free = final;
                                  
                                  return final;
                                }
                             }
                             
          default:
                              return -1;
                                
    }
    return 0;   /*GCC shut up!*/
}

extern int handle_div_by_zero(void)
{
    /*Your code goes in here*/
    u64 rap;
    asm volatile (
        "mov %%rbp, %0;"
    : "=r" (rap));   
    u64 rip = *(u64 *)(rap+8);
    printf("Div-by-zero detected at %x\n",rip );
    do_exit();
    return 0;
}

extern int handle_page_fault(void)
{

    asm volatile (
        "push %%rax;"
        "push %%rbx;"
        "push %%rcx;"
        "push %%rdx;"
        "push %%rdi;"
        "push %%rsi;"
        "push %%r8;"
        "push %%r9;"
        "push %%r10;"
        "push %%r11;"
        "push %%r12;"
        "push %%r13;"
        "push %%r14;"
        "push %%r15;"
        "mov %%rsp, %0;"
        "mov %%rbp, %1;"
    : "=r" (top),"=r"(rap));

    
    u64 error_code = *(u64 *)(rap+8);
    
    u64 rip = *(u64 *)(rap+16);
    rap = rap+16;

    struct exec_context *current = get_current_ctx();
    struct mm_segment stack = current->mms[MM_SEG_STACK];
    struct mm_segment rodata = current->mms[MM_SEG_RODATA];
    struct mm_segment data = current->mms[MM_SEG_DATA];
    //printf("end %x %x %x\n",stack.end,rodata.end,data.end );
    //printf("start %x %x %x\n",stack.start,rodata.start,data.start );
    u64 cr=0;
    asm volatile (
        "mov %%cr2, %0;"
    : "=r" (cr));
    
    short bit_0,bit_1,bit_2;
    bit_0 = error_code & 1;
    bit_1 = (error_code & 2)/2;
    bit_2 = (error_code & 4)/4;
    if(bit_0 == 1){
      printf("Error %x (Protection Fault) at address  %x at RIP %x \n",error_code,cr,rip );
        do_exit();
    }
    u64* CR3 = osmap(current->pgd);
    
    short address_bits[4];
    address_bits[0] = ( cr >> 39) & ~(~0 << (9));
    address_bits[1] = ( cr >> 30) & ~(~0 << (9));
    address_bits[2] = ( cr >> 21) & ~(~0 << (9));
    address_bits[3] = ( cr >> 12) & ~(~0 << (9));
    //printf("err %x %x %x %x\n",bit_0,bit_1,bit_2,error_code );
    
    if(cr<=data.end && cr>=data.start){
      
      if(cr>=data.next_free || bit_0==1){
        printf("Error %x in accessing address  %x at RIP %x \n",error_code,cr,rip );
        do_exit();
      }
      
      u64* temp_addr = CR3;
      for(int i=0;i<4;i++){

        if(*(temp_addr+address_bits[i])==0){
          
          u64 pfn=0;
          if(i==3){
            pfn = os_pfn_alloc(USER_REG);
          }
          
          else{
            pfn = os_pfn_alloc(OS_PT_REG);
          }
          u64* temp = (u64*)osmap((u32)pfn);
          
          if(i != 3){
            for(int i=0;i<512;i++){
              *(temp+i)=0;
            }  
          }
          
          *(temp_addr+address_bits[i]) = (pfn <<12) + 7;

          temp_addr = temp;

        }
        else{
          
          u64 value = *(temp_addr+address_bits[i]);
          u32 pfn = (value >> 12) & ~((long int)~0 << (40));
          temp_addr = osmap(pfn);

        }
      }


    }

    else if(cr<=rodata.end && cr>=rodata.start){
      
      if(cr>=rodata.next_free || bit_1==1 || bit_0==1){
        
        printf("Error %x in accessing address (Unallowed write or address) %x at RIP %x \n",error_code,cr,rip );
        do_exit();
      }
      
      u64* temp_addr = CR3;
      for(int i=0;i<4;i++){
        
        if(*(temp_addr+address_bits[i])==0){
          u32 pfn;
          if(i==3){
            pfn = os_pfn_alloc(USER_REG);
            
            *(temp_addr+address_bits[i]) = (pfn <<12) + 5;
            
          }
          else{
            pfn = os_pfn_alloc(OS_PT_REG);
            
            *(temp_addr+address_bits[i]) = (pfn <<12) + 7;
            
          }
          u64* temp = osmap(pfn);
          if(i != 3){
            for(int i=0;i<512;i++){
              *(temp+i)=0;
            }  
          }
          
          temp_addr = temp;
        }
        else{
         
          u64 value = *(temp_addr+address_bits[i]);
          
          u32 pfn = (value >> 12) & ~((long int)~0 << (40));
          temp_addr = osmap(pfn);

        }
      }

    }

    else if(cr< stack.end && cr>=stack.start){
      
      if(bit_0==1){
        printf("Error %x in accessing address (Unallowed action in stack) %x at RIP %x \n",error_code,cr,rip );
        do_exit();
      }
      
      u64* temp_addr = CR3;
      for(int i=0;i<4;i++){
        if(*(temp_addr+address_bits[i])==0){
          u32 pfn;
          if(i==3){
            pfn = os_pfn_alloc(USER_REG);
          }
          else{
            pfn = os_pfn_alloc(OS_PT_REG);
          }
          u64* temp = osmap(pfn);
          if(i != 3){
            for(int i=0;i<512;i++){
              *(temp+i)=0;
            }  
          }
          *(temp_addr+address_bits[i]) = (pfn <<12) + 7;
          temp_addr = temp;
        }
        else{
          
          u64 value = *(temp_addr+address_bits[i]);
          u64 pfn = (value >> 12) & ~((long int)~0 << (40));
          temp_addr = osmap((u32)pfn);

        }
      }
    }

    else{

      printf("Error %x in accessing address %x at RIP %x since address not belongs to any User segment \n",error_code,cr,rip);
      do_exit();

    }

    asm volatile(
                  "mov %0, %%rsp;"
                  "pop %%r15;"
                  "pop %%r14;"
                  "pop %%r13;"
                  "pop %%r12;"
                  "pop %%r11;"
                  "pop %%r10;"
                  "pop %%r9;"
                  "pop %%r8;"
                  "pop %%rsi;"
                  "pop %%rdi;"
                  "pop %%rdx;"
                  "pop %%rcx;"
                  "pop %%rbx;"
                  "pop %%rax;"
                  "mov %%rbp, %%rsp;"
                  "pop %%rbp;"
                  "add $8,%%rsp;"
                  "iretq;"
                  :
                  : "r"(top)
                  : "memory" );

    return 0;

}


/*
asm volatile (
          "push %0;"
          "push %1;"
          "push %2;"
          "push %3;"
        : "=r" (curr_context->rax),"=r"(curr_context->rbx),"=r"(curr_context->rcx),"=r"(curr_context->rdx));

        asm volatile (
          "push %0;"
          "push %1;"
          "push %2;"
          "push %3;"
        : "=r" (curr_context->rsi),"=r"(curr_context->rdi),"=r"(curr_context->r8),"=r"(curr_context->r9));

        asm volatile (
          "push %0;"
          "push %1;"
          "push %2;"
          "push %3;"
        : "=r" (curr_context->r10),"=r"(curr_context->r11),"=r"(curr_context->r12),"=r"(curr_context->r13));

        asm volatile (
          "push %0;"
          "push %1;"
        : "=r" (curr_context->r14),"=r"(curr_context->r15));

        asm volatile(
          "pop %r15;"
          "pop %r14;"
          "pop %r13;"
          "pop %r12;"
          "pop %r11;"
          "pop %r10;"
          "pop %r9;"
          "pop %r8;"
          "pop %rdi;"
          "pop %rsi;"
          "pop %rdx;"
          "pop %rcx;"
          "pop %rbx;"
          "pop %rax;"

        );

*/
