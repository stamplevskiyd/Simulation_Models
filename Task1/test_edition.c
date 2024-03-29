#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SIZE 4
double a[SIZE][SIZE];
double new[SIZE][SIZE];
int i;
double  b[SIZE][SIZE];
double  c[SIZE][SIZE];

//предварительно рабочий алгоритм

void main( int argc, char **argv )
{
    srand(time(NULL));
    int j,k;
    for( i=0; i < SIZE; i++ )
    {
        for( j = 0; j < SIZE; j++ )
        {
            b[i][j] = rand();
            c[i][j] = rand();
        }
    }
    for( j = 0; j < SIZE; j++ )
    {
        for( i = 0; i < SIZE; i++ )
        {
            a[i][j] = 0;
            for( k = 0; k < SIZE; k++ )
                a[i][j] += b[i][k]+c[k][j];
        }
    }

    ///////////////////////////////////////////////////

    double arr1[SIZE];
    double arr2[SIZE];

    for (i = 0; i < SIZE; i++) //обнуление массивов
        arr1[i] = arr2[i] = 0;
    for( j = 0; j < SIZE; j++ )
        for( i = 0; i < SIZE; i++ )
            new[i][j] = 0;

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
            new[i][j] += arr1[i];
    }

    for (i = 0; i < SIZE; i++){ //суммирование по столбцам
        for (j = 0; j < SIZE; j++)
            new[j][i] += arr2[i];
    }

    /////////////////////////////////////////////////

    for (i = 0; i < SIZE; i++){ //проверка
        for (j = 0; j < SIZE; j++)
            if (a[i][j] != new[i][j]){
                printf("Does not work\n");
                return;
            }
    }
    printf("Everything is fine\n");
    for( j = 0; j < SIZE; j++ ) {
        for (i = 0; i < SIZE; i++)
            printf(" %.1f", b[i][j]);
        printf("\n");
    }
}
