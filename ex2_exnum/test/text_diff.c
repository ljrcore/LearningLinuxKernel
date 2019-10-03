#include<stdio.h>
int main()
{
        int x=1;
        double y=1;

        if((!!(sizeof((typeof(x) *)1 == (typeof(y) *)1)))==1)
                printf("表达式结果是1\n");
        else
                printf("表达式结果不是1\n");
}

