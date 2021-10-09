#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <list>
#include <iostream>

using namespace std;

// Очень простой пример построения имитационной модели с календарём событий
// Модель "самодостаточная" - не используются библиотеки для организации имитационного моделирования
// Возможности С++ используются недостаточно. Что можно улучшить в этой части?

class Event  // событие в календаре
{
public:
    float time; // время свершения события
    int type;   // тип события
    int attr; // дополнительные сведения о событии в зависимости от типа
    Event(float t, int tt, int a) {time = t; type = tt; attr = a;}
};
// типы событий
#define EV_INIT 1
#define EV_REQ 2
#define EV_FIN 3
// состояния
#define RUN 1
#define IDLE 0

#define LIMIT 1000 // время окончания моделирования

class Request // задание в очереди
{
public:
    float time;     // время выполнения задания без прерываний
    int source_num; // номер источника заданий (1 или 2)
    Request(float t, int s) {time = t; source_num = s;}
};

class Calendar: public list<Event*> // календарь событий
{
public:
    void put (Event* ev); // вставить событие в список с упорядочением по полю time
    Event* get (); // извлечь первое событие из календаря (с наименьшим модельным временем)
};

void Calendar::put(Event *ev)
{
    list<Event*>::iterator i;
    Event ** e = new (Event*);
    *e = ev;
    if( empty() ){ push_back(*e); return; }
    i = begin();
    while((i != end()) && ((*i)->time <= ev->time))
    {
        ++i;
    }
    insert(i, *e);
}
Event* Calendar::get()
{
    if(empty()) return NULL;
    Event *e = front();
    pop_front();
    return e;
}


typedef list<Request*> Queue; // очередь заданий к процессору

float get_req_time(int source_num); // длительность задания
float get_pause_time(int source_num); // длительность паузы между заданиями


int main(int argc, char **argv )
{
    Calendar calendar;
    Queue queue;
    float curr_time = 0;
    Event *curr_ev;
    float dt;
    int cpu_state1 = IDLE, cpu_state2 = IDLE;
    int Got_last_task = 2; // какой процессор взял предыдущее задание
    float run_begin;
    bool First_finished = false; // указываем, какой(какие) из процессоров выполнил(и) задание
    bool Second_finished = false;
    int First_completed = 0, Second_completed = 0, First_client = 0, Second_client = 0;
    srand(2019);
// начальное событие и инициализация календаря
    curr_ev = new Event(curr_time, EV_INIT, 0);
    calendar.put( curr_ev );
// цикл по событиям

    while((curr_ev = calendar.get()) != NULL )
    {
        curr_time = curr_ev->time; // продвигаем время
        // обработка события
        if( curr_time >= LIMIT )break; // типичное дополнительное условие останова моделирования
        switch(curr_ev->type)
        {
            case EV_INIT:  // запускаем генераторы запросов
                calendar.put(new Event(curr_time, EV_REQ, 1));
                calendar.put(new Event(curr_time, EV_REQ, 2));
                break;
            case EV_REQ: {
                // планируем событие окончания обработки, если процессор свободен, иначе ставим в очередь
                dt = get_req_time(curr_ev->attr); //все равно считается по-разному для разных процессоров,
                if (curr_ev->attr == 1) {
                    cout << "New task from 1st client" << endl;
                    First_client++;
                }
                else {
                    Second_client++;
                    cout << "New task from 2nd client" << endl;
                }

                int init = Got_last_task; // так как она изменяется

                if (Got_last_task == 2) // если предыдущий запрос взял второй, значит этот берет первый
                {
                    if (cpu_state1 == IDLE) // очередь и календарь оставим общие
                    {
                        Got_last_task = 1;
                        cpu_state1 = RUN;
                        cout << "First took a task" << endl;
                        calendar.put(new Event(curr_time + dt, EV_FIN, curr_ev->attr));
                        First_finished = true;
                        run_begin = curr_time;
                        //освободим процессоры и отметим, что первый брал задание, на шаге fin
                    } else
                        queue.push_back(new Request(dt, curr_ev->attr)); // это задание идет в очередь, независимо от занятости второго процессора, ввиду заданной логики планировщика
                }
                if (init == 1) // как в предыдущем варианте, но процессоры меняются местами
                {
                    if (cpu_state2 == IDLE) // очередь и календарь оставим общие
                    {
                        Got_last_task = 2;
                        cpu_state2 = RUN;
                        cout << "Second took a task" << endl;
                        calendar.put(new Event(curr_time + dt, EV_FIN, curr_ev->attr));
                        Second_finished = true;
                        run_begin = curr_time;
                    } else
                        queue.push_back(new Request(dt, curr_ev->attr)); // это задание идет в очередь
                }
                // планируем событие генерации следующего задания. Никак не меняется
                calendar.put(new Event(curr_time + get_pause_time(curr_ev->attr), EV_REQ, curr_ev->attr));
                break;
            }
            case EV_FIN:
                // объявляем процессор свободным и размещаем задание из очереди, если таковое есть
                if (First_finished)// если эта запись о завершении задания относится к первому процессору
                {
                    cpu_state1 = IDLE;
                    cout << "First finished a task" << endl;
                    First_completed++;
                    First_finished = false;
                }
                if (Second_finished)
                {
                    cpu_state2 = IDLE;
                    cout << "Second finished a task" << endl;
                    Second_completed++;
                    Second_finished = false;
                }
                // выводим запись о рабочем интервале
                if (!queue.empty()) //если в очереди есть задача, кидаем ее в календарь
                {
                    Request *rq = queue.front();
                    queue.pop_front();
                    calendar.put(new Event(curr_time+rq->time, EV_FIN, rq->source_num));
                    cout << "Task from queue from " << curr_ev->attr << " client was given to processors" << endl;
                    delete rq;
                    run_begin = curr_time;
                } break;
        } // switch
        delete curr_ev;
    } // while
    cout << endl<< "Statistics:" << endl << endl;
    cout << "First client made " << First_client << " requests" << endl;
    cout << "Second client made " << Second_client << " requests" << endl;
    cout << "First server processed " << First_completed << " requests" << endl;
    cout << "Second server processed " << Second_completed << " requests" << endl;
} // main

int rc = 0; int pc = 0;
float get_req_time(int source_num)
{
// Для демонстрационных целей - выдаётся случайное значение
// при детализации модели функцию можно доработать
    double r = ((double)rand())/RAND_MAX;
    //cout << "req " << rc << endl; rc++;
    if(source_num == 1) return r*10; else return r*20;
}

float get_pause_time(int source_num) // длительность паузы между заданиями
{
// см. комментарий выше
    double p = ((double)rand())/RAND_MAX;
    //cout << "pause " << pc << endl; pc++;
    if(source_num == 1) return p*20; else return p*10;
}

