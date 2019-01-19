#include<stdio.h>
#include<stdlib.h>
int main(){
	int arr[10];
	unsigned long int* p;
	for(int i=0;i<10;i++){
		arr[i]=i;
	}
	p = (unsigned long int*)malloc(1*sizeof(unsigned long int*));
	*p = 10;
	p[2]=16;
	printf("number %ld",*(p+2));
	//printf("%d %d\n",arr[2],arr[3]);
	//printf("%d\n",x<<4);
	return 0;
}