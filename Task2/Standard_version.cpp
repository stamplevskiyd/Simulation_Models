//#include <stdio.h>
#include <math.h>
//#include <stdlib.h>
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
#define EV_FIN_Q 4
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
    Queue queue1, queue2;
    float curr_time[2] = {0,0}; //вводится 2 времени, так как процессоры работают одновременно и продвигать надо разное время
    float run_begin[2];
    Event *curr_ev;
    float dt;
    int cpu_state1 = IDLE, cpu_state2 = IDLE;
    int Got_last_task = 2; // какой процессор взял предыдущее задание
    int Finished_num; //какой процессор выполнил задачу, которую сейчас завершаем
    int Current_CPU;
    srand(2019);
// начальное событие и инициализация календаря
    curr_ev = new Event(curr_time[0], EV_INIT, 0); //укажем время для первого процессора, все равно ноль
    calendar.put( curr_ev );
// цикл по событиям

    while((curr_ev = calendar.get()) != NULL )
    {
        cout << "Время события: " << curr_ev->time << ", тип: " << curr_ev->type << endl;
        // продвигаем нужное время
        Current_CPU = (curr_ev->type == EV_REQ) ? (2 - Got_last_task) : (curr_ev->attr - 1); // для случая REQ это - последний процессор. Для FIN (и других) - кто закончил задачу. А это отражено в самой задаче
        curr_time[Current_CPU] = curr_ev->time;
        // обработка события
        if( (curr_time[0] >= LIMIT) or (curr_time[1] >= LIMIT) )
            break; // типичное дополнительное условие останова моделирования
        switch(curr_ev->type)
        {
            case EV_INIT:  // запускаем генераторы запросов
                calendar.put(new Event(0, EV_REQ, 1)); // предположим, что непонятно, на какой процессор пойдет какое задание. Хотя в начале это очевидно
                calendar.put(new Event(0, EV_REQ, 2));
                break;
            case EV_REQ: {
                // планируем событие окончания обработки, если процессор свободен, иначе ставим в очередь
                dt = get_req_time(curr_ev->attr); //все равно считается отдельно для разных процессоров
                if (Got_last_task == 2) // если предыдущий запрос взял второй, значит этот берет первый
                {
                    if (cpu_state1 == IDLE) // очередь и календарь оставим общие
                    {
                        cout << "CPU-1 (get): dt " << dt << " num " << curr_ev->attr << endl;
                        Got_last_task = 1;
                        cpu_state1 = RUN;
                        calendar.put(new Event(curr_time[0] + dt, EV_FIN, 1));  // это значение при завершении задачи никак не используется. Будем в нем указывать, кто закончил это задание, а не кто его задал
                        run_begin[0] = curr_time[0];
                        //освободим процессоры и отметим, что первый брал задание, на шаге fin
                    } else {
                        queue1.push_back(new Request(dt, curr_ev->attr)); // это задание идет в очередь, независимо от занятости второго процессора, ввиду заданной логики планировщика
                        cout << "CPU-1 (goes to queue): dt " << dt << " num " << curr_ev->attr << endl;
                    }
                }
                else // как в предыдущем варианте, но процессоры меняются местами
                {
                    if (cpu_state2 == IDLE) // очередь и календарь оставим общие
                    {
                        cout << "CPU-2 (get): dt " << dt << " num " << curr_ev->attr << endl;
                        Got_last_task = 2;
                        cpu_state2 = RUN;
                        calendar.put(new Event(curr_time[1] + dt, EV_FIN, 2)); // это значение при завершении задачи никак не используется. Будем в нем указывать, кто закончил это задание, а не кто его задал
                        run_begin[1] = curr_time[1];
                    } else {
                        queue2.push_back(new Request(dt, curr_ev->attr)); // это задание идет в очередь
                        cout << "CPU-2 (goes to queue): dt " << dt << " num " << curr_ev->attr << endl;
                    }
                }
                // планируем событие генерации следующего задания. Никак не меняется
                calendar.put(new Event(curr_time[Current_CPU]  + get_pause_time(curr_ev->attr), EV_REQ, curr_ev->attr));
                break;
            }
            case EV_FIN:
            case EV_FIN_Q:  //так и задумано, обрабатываем 2 метки
                // объявляем процессор свободным и размещаем задание из очереди, если таковое есть
                if (curr_ev->attr == 1)// если эта запись о завершении задания относится к первому процессору
                {
                    cout << "CPU-1 (finish). Работа с " << run_begin[0] << " по " << curr_time[0] << " длит. " << (curr_time[0] - run_begin[0]);
                    if (curr_ev->type == EV_FIN_Q)
                        cout << " task was in queue";
                    cout << endl;
                    cpu_state1 = IDLE;
                    if (!queue1.empty()) //если в очереди есть задача, кидаем ее в календарь
                    {
                        Request *rq = queue1.front();
                        queue1.pop_front();
                        calendar.put(new Event(curr_time[0] + rq->time, EV_FIN_Q, 1)); //это задание было выдано первому процессору. Он же должен его завершить
                        delete rq;
                        run_begin[0] = curr_time[0];
                    }
                }
                else
                {
                    cout << "CPU-2 (finish). Работа с " << run_begin[1] << " по " << curr_time[1] << " длит. " << (curr_time[1] - run_begin[1]);
                    if (curr_ev->type == EV_FIN_Q)
                        cout << " task was in queue";
                    cout << endl;
                    cpu_state2 = IDLE;
                    if (!queue2.empty()) //если в очереди есть задача, кидаем ее в календарь
                    {
                        Request *rq = queue2.front();
                        queue2.pop_front();
                        calendar.put(new Event(curr_time[1] + rq->time, EV_FIN_Q, 2));
                        delete rq;
                        run_begin[1] = curr_time[1];
                    }
                }
                // выводим запись о рабочем интервале
                break;
        } // switch
        delete curr_ev;
        //cout << curr_time[0] << " " << curr_time[1] << endl;
    } // while
    //cout << curr_time[0] << " " << curr_time[1] << endl;
} // main

int rc = 0; int pc = 0;
float get_req_time(int source_num)
{
// Для демонстрационных целей - выдаётся случайное значение
// при детализации модели функцию можно доработать
    double r = ((double)rand())/RAND_MAX;
    cout << "req " << rc << endl; rc++;
    if(source_num == 1) return r*10; else return r*20;
}

float get_pause_time(int source_num) // длительность паузы между заданиями
{
// см. комментарий выше
    double p = ((double)rand())/RAND_MAX;
    cout << "pause " << pc << endl; pc++;
    if(source_num == 1) return p*20; else return p*10;
}

