#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<apic.h>
#include<idt.h>
#include<memory.h>
#include<context.h>
#include<types.h>
#include<lib.h>

static u64 sleep_var = 0;
static u64 numticks;
static u64* temp_rsp;
static u64 top;
static u64* temp_rbp;
static struct list start;
static struct list* rr_list = &(start);
static u64 curr_proc = 1;


static void save_current_context()
{
  
    struct exec_context *current = get_current_ctx();
    struct user_regs* regs_temp = &(current->regs);
  
    u64* temp_sys = temp_rbp;

    regs_temp->rax = *(temp_sys+13);
    regs_temp->rbx = *(temp_sys+12);
    regs_temp->rcx = *(temp_sys+11);
    regs_temp->rdx = *(temp_sys+10);
    regs_temp->rdi = *(temp_sys+9);
    regs_temp->rsi = *(temp_sys+8);
    regs_temp->r8 = *(temp_sys+7);
    regs_temp->r9 = *(temp_sys+6);
    regs_temp->r10 = *(temp_sys+5);
    regs_temp->r11 = *(temp_sys+4);
    regs_temp->r12 = *(temp_sys+3);
    regs_temp->r13= *(temp_sys+2);
    regs_temp->r14 = *(temp_sys+1);
    regs_temp->r15 = *(temp_sys);


    u64* temp_save;
    asm volatile(
      "mov %%rbp, %0;"
      :"=r"(temp_save)
      );

    temp_save = (u64*)*temp_save;

    regs_temp->entry_ss = *(temp_save+5);
    regs_temp->entry_rsp = *(temp_save+4);
    regs_temp->entry_rflags = *(temp_save+3);
    regs_temp->entry_cs = *(temp_save+2);
    regs_temp->entry_rip = *(temp_save+1);
    regs_temp->rbp = *(temp_save);
  //printf("saving %x %x %x %x %x\n",regs_temp->entry_ss,regs_temp->entry_rsp,regs_temp->entry_rflags,regs_temp->entry_cs,regs_temp->entry_rip );
    return;
   
} 

static void schedule_context(struct exec_context *next)
{

  struct exec_context *current = get_current_ctx();
  if(current->state == RUNNING)
    current->state = READY;
  
  next->state = RUNNING;
  
  struct user_regs* regs_temp = &(next->regs);
 
 //printf("schedluing: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/

 set_tss_stack_ptr(next);
 set_current_ctx(next);

 asm volatile(

  "push %0;"
  "push %1;"
  "push %2;"
  "push %3;"
  :
  : "r"(regs_temp->entry_ss),"r"(regs_temp->entry_rsp),"r"(regs_temp->entry_rflags),"r"(regs_temp->entry_cs)
  : "memory"
  );

 asm volatile(
  "push %0;"
  "push %1;"
  "push %2;"
  "push %3;"
  
  :
  : "r"(regs_temp->entry_rip),"r"(regs_temp->rbp),"r"(regs_temp->rax),"r"(regs_temp->rbx)
  : "memory"
  );

  asm volatile(
  "push %0;"
  "push %1;"
  "push %2;"
  "push %3;"
  
  :
  : "r"(regs_temp->rcx),"r"(regs_temp->rdx),"r"(regs_temp->rdi),"r"(regs_temp->rsi)
  : "memory"
  );

  asm volatile(
  "push %0;"
  "push %1;"
  "push %2;"
  "push %3;"
  
  :
  : "r"(regs_temp->r8),"r"(regs_temp->r9),"r"(regs_temp->r10),"r"(regs_temp->r11)
  : "memory"
  );

  asm volatile(
  "push %1;"
  "push %2;"
  "push %3;"
  "push %4;"
  "mov %%rsp, %0;"
  : "=r"(temp_rbp)
  : "r"(regs_temp->r12),"r"(regs_temp->r13),"r"(regs_temp->r14),"r"(regs_temp->r15)
  : "memory"
  );
   
   
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
                  "pop %%rbp;"
                  "iretq;"
                  :
                  : "r"(temp_rbp)
                  :"memory"
                   );
  
 return;

}


static struct exec_context *pick_next_context()
{
  
  struct exec_context* current = get_current_ctx();
  struct node* front = dequeue_front(rr_list);

  if(front == NULL && sleep_var == curr_proc && sleep_var > 0){
    return get_ctx_by_pid(0);
  }

  else if(front == NULL && curr_proc == 0 ){
    os_pfn_free(OS_PT_REG, (u64)current->os_stack_pfn);
    //printf("Exiting %s Process\n",current->name);
    
    curr_proc = 1;
    sleep_var = 0;
    do_cleanup();
  }

  else if(front==NULL){
    printf("%x\n",curr_proc);
    return current;
  }

  else{
    
    return get_ctx_by_pid(front->value);
  }
}

static struct exec_context* schedule()
{
 
 struct exec_context *next;  
 next = pick_next_context();
 return next;
     
}

static void do_sleep_and_alarm_account()
{
 /*All processes in sleep() must decrement their sleep count*/
  for(int i=0;i<16;i++){
    struct exec_context *list = get_ctx_by_pid(i);
    if(list->state==WAITING && list->ticks_to_sleep>1){
      (list->ticks_to_sleep)--;
    }  
    else if(list->state==WAITING && list->ticks_to_sleep==1){
      list->ticks_to_sleep =0;
      list->state = READY;
      sleep_var--;
      enqueue_tail(rr_list,list->pid);
    }

  }
  
  return;

}

/*The five functions above are just a template. You may change the signatures as you wish*/

void do_exit()
{
  /*You may need to invoke the scheduler from here if there are
    other processes except swapper in the system. Make sure you make 
    the status of the current process to UNUSED before scheduling 
    the next process. If the only process alive in system is swapper, 
    invoke do_cleanup() to shutdown gem5 (by crashing it, huh!)
    */
    curr_proc--;
    struct exec_context* current = get_current_ctx();
    current->state = UNUSED;
    struct exec_context* next =  schedule();
    //printf("Exiting %s Process\n",current->name);
    printf("scheduling: old pid = %d  new pid  = %d\n", current->pid, next->pid);
    schedule_context(next);
    
    curr_proc = 1;
    sleep_var = 0;
    do_cleanup();
      /*Call this conditionally, see comments above*/
}

/*system call handler for sleep*/
long do_sleep(u32 ticks)
{

    sleep_var++;
   u64 rsp_check;
   u64 cr3_new;
   asm volatile(
    "mov %%rbp,%0;"
    "mov %%rsp,%1;"
    :"=r"(temp_rsp),"=r"(rsp_check)
    );

   u64* temp_sys = (u64*) *temp_rsp;

   struct exec_context *current = get_current_ctx();
   current->ticks_to_sleep = ticks;
   current->state = WAITING;

   struct user_regs* regs_temp = &(current->regs);
    
    
    regs_temp->entry_ss = *(temp_sys+20);
    regs_temp->entry_rsp = *(temp_sys+19);
    regs_temp->entry_rflags = *(temp_sys+18);
    regs_temp->entry_cs = *(temp_sys+17);
    regs_temp->entry_rip = *(temp_sys+16);
    regs_temp->rbp = *(temp_sys+10);
    regs_temp->rax = 0;
    regs_temp->rbx = *(temp_sys+15);
    regs_temp->rcx = *(temp_sys+14);
    regs_temp->rdx = *(temp_sys+13);
    regs_temp->rdi = *(temp_sys+11);
    regs_temp->rsi = *(temp_sys+12);
    regs_temp->r8 = *(temp_sys+9);
    regs_temp->r9 = *(temp_sys+8);
    regs_temp->r10 = *(temp_sys+7);
    regs_temp->r11 = *(temp_sys+6);
    regs_temp->r12 = *(temp_sys+5);
    regs_temp->r13= *(temp_sys+4);
    regs_temp->r14 = *(temp_sys+3);
    regs_temp->r15 = *(temp_sys+2);

     struct exec_context* next =  schedule();
     printf("scheduling: old pid = %d  new pid  = %d\n", current->pid, next->pid);
     schedule_context(next);

     return 0;      

}

/*
  system call handler for clone, create thread like 
  execution contexts
*/
long do_clone(void *th_func, void *user_stack)
{

  curr_proc++;
  struct exec_context* current = get_current_ctx();
  
  struct exec_context* new_context = get_new_ctx();
  struct user_regs* regs_temp = &(new_context->regs);

  regs_temp->entry_ss = 0x2b;
  regs_temp->entry_cs = 0x23;
  new_context->state = READY ;
  
  new_context->type = current->type;
  new_context->used_mem = current->used_mem;
  new_context->pgd = current->pgd;
  new_context->os_rsp = current->os_rsp;
  new_context->pending_signal_bitmap =0;
  new_context->alarm_config_time = 0;
  new_context->ticks_to_alarm = 0;
  new_context->ticks_to_sleep = 0;
  for (int i = 0; i < MAX_SIGNALS; ++i)
  {
    new_context->sighandlers[i] = current->sighandlers[i];
  }

      struct mm_segment *temp = &(new_context->mms[MM_SEG_DATA]);
      temp->start = (current->mms[MM_SEG_DATA]).start;
      temp->end = (current->mms[MM_SEG_DATA]).end;
      temp->next_free = (current->mms[MM_SEG_DATA]).next_free;
      temp->access_flags = (current->mms[MM_SEG_DATA]).access_flags;

       temp = &(new_context->mms[MM_SEG_CODE]);
      temp->start = (current->mms[MM_SEG_CODE]).start;
      temp->end = (current->mms[MM_SEG_CODE]).end;
      temp->next_free = (current->mms[MM_SEG_CODE]).next_free;
      temp->access_flags = (current->mms[MM_SEG_CODE]).access_flags;

       temp = &(new_context->mms[MM_SEG_STACK]);
      temp->start = (current->mms[MM_SEG_DATA]).start;
      temp->end = (current->mms[MM_SEG_DATA]).end;
      temp->next_free = (current->mms[MM_SEG_DATA]).next_free;
      temp->access_flags = (current->mms[MM_SEG_DATA]).access_flags;

       temp = &(new_context->mms[MM_SEG_RODATA]);
      temp->start = (current->mms[MM_SEG_RODATA]).start;
      temp->end = (current->mms[MM_SEG_RODATA]).end;
      temp->next_free = (current->mms[MM_SEG_RODATA]).next_free;
      temp->access_flags = (current->mms[MM_SEG_RODATA]).access_flags;


  char* p_name = current->name;
  char* c_name = new_context->name;
  int i=0;
  while(p_name[i]!='\0'){
    c_name[i] = p_name[i];
    i++;
  }

  int j = (new_context->pid)/10;
  if(j!=0){
    c_name[i] = '0'+j;
    i++; 
  }

  j= (new_context->pid)%10;
  c_name[i] = '0'+j;
  i++;
  c_name[i] = '\0';

  new_context->os_stack_pfn = os_pfn_alloc(OS_PT_REG);

  asm volatile(
    "mov %%rbp, %0;"
    :"=r"(temp_rbp)
    );

  temp_rbp = (u64*)*temp_rbp;
  regs_temp->entry_rflags = *(temp_rbp+18);
  regs_temp->entry_rip = (u64)th_func;
  regs_temp->entry_rsp = (u64)user_stack;

  char* ini = (char*)user_stack;
  *ini = 0;
  regs_temp->rbp = (u64)user_stack;
  enqueue_tail(rr_list,new_context->pid);
  
  return new_context->pid;
}


void handle_timer_tick()
{
 /*
   This is the timer interrupt handler. 
   You should account timer ticks for alarm and sleep
   and invoke schedule
 */
  
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
        :"=r"(temp_rbp)
    );

  printf("Got a tick. #ticks = %u\n", ++numticks);   /*XXX Do not modify this line*/
  struct exec_context *current = get_current_ctx();


  // if(numticks == 1){

  //   init_list(rr_list);
  // }

  if(current->state == RUNNING && current->pid !=0){

    enqueue_tail(rr_list,current->pid);
  }
    
  do_sleep_and_alarm_account();
  
  
  if (current->alarm_config_time !=0){
    if(current->ticks_to_alarm == 1){
      current->ticks_to_alarm = current->alarm_config_time;
      u64* pointer;
      asm volatile(
        "mov %%rbp, %0;"
        :"=r"(pointer)
        );

      invoke_sync_signal(SIGALRM, (pointer+4),(pointer+1));
    }

    else{
      (current->ticks_to_alarm)--;
    }

  }

    for(int i=0;i<16;i++){
      struct exec_context* r = get_ctx_by_pid(i);
      if(r->pid != current->pid && r->state == UNUSED && r->os_stack_pfn!=0){
        os_pfn_free(OS_PT_REG, (u64)r->os_stack_pfn);
        r->os_stack_pfn = 0;
      }
    }

  if(current->pid!=0)
    save_current_context();

  struct exec_context* next = schedule();

  if(next->pid != current->pid){
    printf("scheduling: old pid = %d  new pid  = %d\n", current->pid, next->pid);
    ack_irq();
    schedule_context(next);
       
  }

  ack_irq(); 

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
                  "iretq;"
                  :
                  : "r"(temp_rbp)
                  :"memory"
                   );
  
}

long invoke_sync_signal(int signo, u64 *ustackp, u64 *urip)
{
   /*If signal handler is registered, manipulate user stack and RIP to execute signal handler*/
   /*ustackp and urip are pointers to user RSP and user RIP in the exception/interrupt stack*/
    printf("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
    /*Default behavior is exit( ) if sighandler is not registered for SIGFPE or SIGSEGV.
    Ignore for SIGALRM*/
    struct exec_context* curr_context = get_current_ctx();
    u64 ori_rip = (u64)curr_context->sighandlers[signo] ;
    
    if(ori_rip!=0){
        
        u64 rsp = *ustackp;
        u64* stack = (u64*)rsp;   
        stack = stack - 1;
        *(stack) = *urip;
        *ustackp = (u64) stack;
        *urip = ori_rip;
        return 0;

    }

    if(signo != SIGALRM)
      do_exit();
    else
      return 0;

}

/*system call handler for signal, to register a handler*/
long do_signal(int signo, unsigned long handler)
{
 struct exec_context *current = get_current_ctx();
 if(signo >= MAX_SIGNALS || signo < 0){
  return 1;
 }

 current->sighandlers[signo] = (void*)handler;
 return 0;
}

/*system call handler for alarm*/
long do_alarm(u32 ticks)
{
  
  //printf("In kernel and ticks %x\n",ticks);
  struct exec_context *current = get_current_ctx();

  current->ticks_to_alarm = ticks;  
  current->alarm_config_time = ticks;   
  return 0;
}
