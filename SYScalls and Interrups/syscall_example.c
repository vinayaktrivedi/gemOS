#include<stdio.h>
#include<stdlib.h>

main()
{
  int pid = 0;
   unsigned int a=0;
  asm volatile("movl %%eax, %0"
               :"=r" (a)
               :
               :"memory"               
  );
  printf("%x\n",a );
// #if 0
//   printf("Before syscall\n");
//   pid = getpid();
//   printf("After syscall pid = %d\n", pid);
//   exit(0); 
//   printf("After exit\n");
// #else
//   printf("Before syscall\n");
//   asm volatile("movl $20, %%eax;"    /*SYSCALL GETPID = 20*/
//                "int $0x80;"
//                "movl %%eax, %0"
//                :"=r" (pid)
//                :
//                :"memory"               
//   );
//   printf("After syscall pid = %d\n", pid);
//   asm volatile("movl $1, %%eax;"   /*SYSCALL EXIT = 1*/
//                "mov $0, %%ebx;"
//                "int $0x80;"
//                :
//                :
//                :"memory"               
//   );
//   printf("After exit\n");
// #endif
}