#include<init.h>
#include<lib.h>
#include<context.h>
#include<memory.h>
static void exit(int);
static int main(void);


void init_start()
{
  int retval = main();
  exit(0);
}

/*Invoke system call with no additional arguments*/
static long _syscall0(int syscall_num)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

/*Invoke system call with one argument*/

static long _syscall1(int syscall_num, u64 exit_code)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

static long _syscall2(int syscall_num, u64 buf,int length )
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

static void exit(int code)
{
  _syscall1(SYSCALL_EXIT, code); 
}

static int getpid()
{
  return(_syscall0(SYSCALL_GETPID));
}

static long write(char* buf,int length)
{
  return(_syscall2(SYSCALL_WRITE, (u64)buf, length));
} 

static void* expand(u32 size,int flags){
   u64 k;
   k=_syscall2(SYSCALL_EXPAND, (u64)size, flags);
   
  if(k==-1){
    return NULL;
  }
  return (void*)k;
}

static void* shrink(u32 size,int flags){
   u64 k=(_syscall2(SYSCALL_SHRINK, (u64)size, flags));
  if(k==-1){
    return NULL;
  }
  return (void*)k;
}




static int main(){

#if 1
  
  char *ptrs = (unsigned long *)0x7FF000001;
  *ptrs='A';
#endif

  // void *ptr1;
  // char *ptr = (char *) expand(8, MAP_WR);
  
  // if(ptr == NULL)
  //             write("FAILED\n", 7);
  
  // *(ptr + 8192) = 'A';   /*Page fault will occur and handled successfully*/
  // write(ptr+8192,1);
  // ptr1 = (char *) shrink(7, MAP_WR);
  // *ptr = 'A';          /*Page fault will occur and handled successfully*/

  // *(ptr + 4096) = 'A';   /*Page fault will occur and PF handler should termminate 
  //                  the process (gemOS shell should be back) by printing an error message*/
  // exit(0);


  // Vinayak

  // int it=2/0;
//   void *ptr1;
//   char *ptr = (char *) expand(20, MAP_WR);
  
//   if(ptr == NULL)
//               write("Expand FAILED\n", 7);
  
//   *(ptr + 8192) = 'X';   /*Page fault will occur and handled successfully*/
//   *(ptr+ 8193) = 'C';
//   write(ptr+8192,1); /*check if value is written properly */

//   ptr1 = (char *) shrink(100, MAP_WR);

//   if(ptr1 != NULL)
//       write("Shrink not working\n",18);


//   *ptr = 'X';          /*Page fault will occur and handled successfully*/
//   *(ptr+1) = 'B';
//   *(ptr+2) = 'C';
//   *(ptr+3) = 'D'; 
//   ptr1 = (char*)shrink(20,MAP_WR);


//   if(ptr1==NULL)
//       write("shrink not working\n",18);

//     Page fault will occur and PF handler should termminate 
//                    the process (gemOS shell should be back) by printing an error message
//   char *ptr2 = (char *) expand(12, MAP_RD);

//   if(ptr2==NULL)
//     write("expand not working\n",18);

//   ptr = (char*)expand(5,MAP_WR);

//   long k =write(ptr+1,1);   /*write should get failed*/
//   k=write((char*)0x100,10);
//   if(k!=-1)
//     write("write not working\n",17);

//   k=write("Hello\n",6);

//   if(k!=-1)
//     write("write working\n",14);

//   u64* t = (u64*)(0x7FF000000+16);
//   *t = 64;

//   write("stack page fault handled\n",25);

//   char* ptr3 = (char *) expand(20, MAP_RD);

//    char x=*(ptr3);
//    write((char *)ptr3,16);
//   // if(x==0){
//   //   write("read only page fault handled\n",29);
//   // }
// // int *x = 0x9;
// // int y = *x;
//   *(ptr3 + 1023) = 100;   /*page fault will occur and not handled successfully"*/
// // int *u = ptr3;
// // int xx= *(ptr3);
//   exit(0);
  char* ptr = ( char* ) expand(8, MAP_WR);
  *(ptr + 4096) = 'X';
  write(ptr+4096,1);
  shrink(8, MAP_WR);
  *(ptr + 4096) = 'A';
  exit(0);
}
//5Te8Y4drgCRfCx8ugdwuEX8KFC6k2EUu