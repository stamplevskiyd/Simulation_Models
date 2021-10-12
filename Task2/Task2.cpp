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
    Event(float t, int tt, int a) {
        time = t;
        type = tt;
        attr = a;
    }
};
// типы событий
#define EV_INIT 1
#define EV_REQ 2
#define EV_FIN1 3
#define EV_FIN2 4
// состояния
#define RUN 1
#define IDLE 0

#define LIMIT 1000 // время окончания моделирования

class Request // задание в очереди
{
public:
    float time;     // время выполнения задания без прерываний
    int source_num; // номер источника заданий (1 или 2)
    Request(float t, int s) {
        time = t;
        source_num = s;
    }
};

class Calendar : public list<Event *> // календарь событий
{
public:
    void put(Event *ev); // вставить событие в список с упорядочением по полю time
    Event *get(); // извлечь первое событие из календаря (с наименьшим модельным временем)
};

void Calendar::put(Event *ev) {
    list<Event *>::iterator i;
    Event **e = new (Event *);
    *e = ev;
    if (empty()) {
        push_back(*e);
        return;
    }
    i = begin();
    while ((i != end()) && ((*i)->time <= ev->time)) {
        ++i;
    }
    insert(i, *e);
}

Event *Calendar::get() {
    if (empty()) return NULL;
    Event *e = front();
    pop_front();
    return e;
}


typedef list<Request *> Queue; // очередь заданий к процессору

float get_req_time(int source_num); // длительность задания
float get_pause_time(int source_num); // длительность паузы между заданиями


class Balancer {
public:
    int Got_last_task = 1; // какой процессор взял последнее задание. По умолчанию-CPU-2, чтобы начать с первого
    void Define_CPU() {
        int copy = Got_last_task;
        Got_last_task = 1 - copy;
    }

    int Current_CPU(Event *curr) { // считает, с каким процессором работаем сейчас
        // для случая REQ это - последний процессор. Для FIN (и других) - кто закончил задачу. А это отражено в самой задаче
        if (curr->type == EV_REQ)
            return 1 - Got_last_task;
        if (curr->type == EV_FIN1)
            return 0;
        if (curr->type == EV_FIN2)
            return 1;
    }
};

int main(int argc, char **argv) {
    Calendar calendar;
    Queue queue[2];
    Balancer B1;
    float curr_time[2] = {0, 0}; //вводится 2 времени, так как процессоры работают одновременно и продвигать надо разное время
    float run_begin[2];
    Event *curr_ev;
    float dt;
    int cpu_state[2] = {IDLE, IDLE};
    srand(2019);
// начальное событие и инициализация календаря
    curr_ev = new Event(curr_time[0], EV_INIT, 0); //укажем время для первого процессора, все равно ноль
    calendar.put(curr_ev);
// цикл по событиям

    while ((curr_ev = calendar.get()) != NULL) {
        cout << "Время события: " << curr_ev->time << ", тип: " << curr_ev->type << endl;
        cout << "CPU-1, queue ";
        for (Request* rq:queue[0]) cout << rq->time << " | "; //выведем задачи в очереди и время на их выполнение
        cout << endl << "CPU-2, queue ";
        for (Request* rq:queue[1]) cout << rq->time << " | ";
        cout << endl;

        // продвигаем нужное время
        int Curr_Proc = B1.Current_CPU(curr_ev);
        curr_time[Curr_Proc] = curr_ev->time;
        // обработка события
        if ((curr_time[0] >= LIMIT) or (curr_time[1] >= LIMIT))
            break; // типичное дополнительное условие останова моделирования
        switch (curr_ev->type) {
            case EV_INIT:  // запускаем генераторы запросов
                calendar.put(new Event(0, EV_REQ,1)); // предположим, что непонятно, на какой процессор пойдет какое задание. Хотя в начале это очевидно
                calendar.put(new Event(0, EV_REQ,2));
                break;
            case EV_REQ: {
                // планируем событие окончания обработки, если процессор свободен, иначе ставим в очередь
                dt = get_req_time(curr_ev->attr); //все равно считается отдельно для разных процессоров
                if (cpu_state[Curr_Proc] == IDLE) // очередь и календарь оставим общие
                {
                    cout << "CPU-" << Curr_Proc + 1 << " (get): dt " << dt << " num " << curr_ev->attr << endl;
                    B1.Define_CPU(); //говорим, какой процессор взял задание и какой-следующий
                    cpu_state[Curr_Proc] = RUN;
                    calendar.put(new Event(curr_time[Curr_Proc] + dt, EV_FIN1 + Curr_Proc, curr_ev->attr));
                    run_begin[Curr_Proc] = curr_time[Curr_Proc];
                    //освободим процессоры и отметим, что первый брал задание, на шаге fin
                } else {
                    B1.Define_CPU(); // будем считать, что процессор взял задание, даже если он поместил его в очередь
                    queue[Curr_Proc].push_back(new Request(dt, curr_ev->attr)); // это задание идет в очередь, независимо от занятости второго процессора, ввиду заданной логики планировщика
                    cout << "CPU-" << Curr_Proc + 1 << " (goes to queue): dt " << dt << " num " << curr_ev->attr << endl;
                }
                // планируем событие генерации следующего задания. Никак не меняется
                calendar.put(new Event(curr_time[Curr_Proc] + get_pause_time(curr_ev->attr), EV_REQ, curr_ev->attr));
                break;
            }
            case EV_FIN1:
            case EV_FIN2:  //так и задумано, обрабатываем 2 метки
                // объявляем процессор свободным и размещаем задание из очереди, если таковое есть

                //вывод
                cout << "CPU-" << Curr_Proc + 1 << " (finish). Работа с " << \
                    run_begin[Curr_Proc] << " по " << curr_time[Curr_Proc] << " длит. "\
                    << (curr_time[Curr_Proc] - run_begin[Curr_Proc]) << endl;
                //обработка
                cpu_state[Curr_Proc] = IDLE;
                if (!queue[Curr_Proc].empty()) //если в очереди есть задача, кидаем ее в календарь
                {
                    Request *rq = queue[Curr_Proc].front();
                    queue[Curr_Proc].pop_front();
                    calendar.put(new Event(curr_time[Curr_Proc] + rq->time, EV_FIN1 + Curr_Proc, Curr_Proc + 1)); //это задание было выдано первому процессору. Он же должен его завершить
                    cpu_state[Curr_Proc] = RUN;
                    delete rq;
                    run_begin[Curr_Proc] = curr_time[Curr_Proc];
                }

                // выводим запись о рабочем интервале
                break;
        } // switch
        delete curr_ev;
    } // while
} // main

//оставлю как было, и так достаточно наглядно, так как задан srand
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


