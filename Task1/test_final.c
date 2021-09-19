#include <stdio.h>
#include <stdlib.h>
#define SIZE 1024
double a[SIZE][SIZE];
double new[SIZE][SIZE];
int i;
double  b[SIZE][SIZE];
double  c[SIZE][SIZE];


void main( int argc, char **argv )
{
    int j,k;
    for( i=0; i < SIZE; i++ )
    {
        for( j = 0; j < SIZE; j++ )
        {
            b[i][j] = 1.9;
            c[i][j] = 2.0;
            a[i][j] = 0.0;
        }
    }
    
    double arr1[SIZE];
    double arr2[SIZE];

    for (i = 0; i < SIZE; i++) //обнуление массивов
        arr1[i] = arr2[i] = 0;
        
    for (i = 0; i < SIZE; i++){ //подсчет суммы строк
        for (j = 0; j < SIZE; j++)
            arr1[i] += b[i][j];
    }
    for (i = 0; i < SIZE; i++){ //подсчет суммы столбцов
        for (j = 0; j < SIZE; j++)
            arr2[i] += c[j][i];
    }

    for (i = 0; i < SIZE; i++){ //суммирование по строкам
        for (j = 0; j < SIZE; j++)
            a[i][j] += arr1[i];
    }

    for (i = 0; i < SIZE; i++){ //суммирование по столбцам
        for (j = 0; j < SIZE; j++)
            a[j][i] += arr2[i];
    }


}
