#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <fstream>
#include <string>
#include <iostream>
#include <utility>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

// константные данные вроде числа клиентов и размера пакета


#define CLIENT_NUM  30 // число клиентов
#define SIM_TIME 10.0 // время симуляции
#define CH_DELAY 300 // время на передачу сообщения по каналу
#define alpha 50.0  // альфа, параметр экспоненциального распределения
#define PCK_SIZE 1500 // размер пакета
#define q_max_size "100p" // максимальный размер очереди на хосте

using namespace ns3;
using generator = std::default_random_engine;  // задаем распределение
using distribution = std::exponential_distribution<double>;  // распределение - экспоненциальное

 // отправлено
long packet_sent_total = 0; // общее число отправленных пакетов
long packet_sent[CLIENT_NUM];  // сколько пакетов послал каждый клиент

 // сброшено
long packet_droped_total = 0; // общее число сброшенных пакетов
long packet_droped[CLIENT_NUM]; // сколько пакетов потерял каждый клиент

 // дубли
long duplicates_total = 0;
long duplicates[CLIENT_NUM]; // сколько пакетов переотправлял каждый клиент

// очереди
long max_queue = 0;
long queue[CLIENT_NUM];  // очереди клиентов в момент времени
long max_queue_clients[CLIENT_NUM]; // максимальные очереди клиентов


NS_LOG_COMPONENT_DEFINE("Client");

class Client : public Application { // модуль приложения
public:
    Client();
    virtual ~Client();
    void Setup(Ptr <Socket> socket, Address address, generator *gen1, distribution *distr1, Ptr <Queue<Packet>> que1, std::string name1, int num1);
    void send(); // отправка пакета
    virtual void StartApplication();  // функция запуска приложения из базового класса Application
    virtual void StopApplication();

    Ptr <Socket> m_socket;
    Address m_peer;
    EventId m_sendEvent;
    bool m_running;
    generator *gen;
    distribution *distr;
    Ptr <Queue<Packet>> que;
    std::string name;
    int m_num;
};

Client::Client() {
    m_socket = 0;
    m_running = false;
}

Client::~Client() {
    m_socket = 0;
    delete gen;
    delete distr;
}

void Client::Setup(Ptr <Socket> socket, Address address, generator *gen1, distribution *distr1, Ptr <Queue<Packet>> que1, std::string name1, int num1) {
    m_socket = socket;
    m_peer = address;
    gen = gen1;
    distr = distr1;
    que = que1;
    name = std::move(name1);
    m_num = num1;
}

void Client::send() // отправка пакета и постановка в очередь отправки следующего
{
    Ptr <Packet> msg = Create<Packet>(PCK_SIZE);
    m_socket->Send(msg);  // отправка пакета с использованием заданного сокета
    packet_sent_total++; // увеличиваем общее число отправленных пакетов
    if (m_running) {
        Time New(MilliSeconds(1000 * (*distr)(*gen)));
        queue[m_num] += que->GetNPackets();
        packet_sent[m_num]++;
        if (que->GetNPackets() > max_queue) // теперь обновим максимумы очередей для всех хостов и для каждого по отдельности
            max_queue = que->GetNPackets();
        if (que->GetNPackets() > max_queue_clients[m_num])
            max_queue_clients[m_num] = que->GetNPackets();
        m_sendEvent = Simulator::Schedule(New, &Client::send, this);  // планирование
    }
}

void Client::StartApplication() {
    m_running = true;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    m_socket->SetRecvCallback(MakeNullCallback < void, Ptr < Socket >> ());
    m_socket->SetAllowBroadcast(true);
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &Client::send, this);
}

void Client::StopApplication() {
    m_running = false;
    if (m_sendEvent.IsRunning())
        Simulator::Cancel(m_sendEvent);
    if (m_socket)
        m_socket->Close();
}


static void duplicate(std::string context, Ptr<const Packet> p) {
    std::string::size_type pos = context.find(':');
    int i = std::stoi(context.substr(5, pos-1));
    duplicates[i]++;
    duplicates_total++;
}

static void drop(std::string context, Ptr<const Packet> p)
{
    std::string::size_type pos = context.find(':');
    int i = std::stoi(context.substr(5, pos-1));
    packet_droped[i]++;
    packet_droped_total++;
}


int main(int argc, char **argv) {
    LogComponentEnable("Client", LOG_LEVEL_INFO);
    NodeContainer nodes;
    nodes.Create(CLIENT_NUM); // задаем число клиентов

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));  // задаем характеристики системы: B = 100mbps
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(CH_DELAY)));  // задержка на распространение по каналу
    csma.SetQueue("ns3::DropTailQueue");

    NetDeviceContainer devices = csma.Install(nodes);

    std::vector<Ptr < Queue < Packet>>> queues;
    for (uint32_t i = 0; i < CLIENT_NUM; i++) {
        Ptr <RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(0.0000000001));
        Ptr <DropTailQueue<Packet>> que = CreateObject < DropTailQueue < Packet >> ();
        que->SetMaxSize(QueueSize(q_max_size)); // задаем максимальный размер очереди
        que->TraceConnect("Drop", "Host " + std::to_string(i) + ": ", MakeCallback(&drop));
        queues.push_back(que);
        devices.Get(i)->SetAttribute("TxQueue", PointerValue(que));
        devices.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.1.0", "255.255.255.0");  // назначаем ip адреса из сети 10.0.1.0 с маской 24 (на последний октет)
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(CLIENT_NUM - 1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkClients = packetSinkHelper.Install(nodes.Get(CLIENT_NUM - 1));
    sinkClients.Start(Seconds(0.0));
    sinkClients.Stop(Seconds(SIM_TIME));


    for (uint32_t i = 0; i < CLIENT_NUM - 1; i++) {
        Ptr <Socket> ns3UdpSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        Ptr <Client> cl = CreateObject<Client>();
        cl->Setup(ns3UdpSocket,
                  sinkAddress,
                  new generator(i),
                  new distribution(alpha),
                  queues[i],
                  "Host " + std::to_string(i), // имя (номер) хоста
                  i);
        nodes.Get(i)->AddApplication(cl);
        cl->SetStartTime(Seconds(0.0));
        cl->SetStopTime(Seconds(SIM_TIME));
        devices.Get(i)->TraceConnect("MacTxBackoff", "Host " + std::to_string(i) + ": ", MakeCallback(&duplicate));
    }

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fifth.tr"));

    Simulator::Stop(Seconds(SIM_TIME));
    Simulator::Run();
    Simulator::Destroy();

    int i;
    double total_fraction = 0;
    double M_buffer = 0;
    double Max_M_buffer = 0;
    for (i = 0; i < CLIENT_NUM-1; i++)
    {
        total_fraction += (double)(packet_droped[i] + duplicates[i]) / double((packet_sent[i] + packet_droped[i] + duplicates[i]) * CLIENT_NUM);
        Max_M_buffer += double(max_queue_clients[i]) / CLIENT_NUM;
        M_buffer += double(queue[i]) / double(packet_sent[i]);
    }
    M_buffer = M_buffer / CLIENT_NUM;

    std::cout << "[=============] SIMULATION ENDS [=============]\n";
    std::cout << "Packets Sent (No dups):\t" << packet_sent_total << " \n";
    std::cout << "   - Packets Dropped:\t" << packet_droped_total << " \n";
    std::cout << "   - Duplicates:\t" << duplicates_total << " \n";
    std::cout << "[*] Fraction of Lost packets:\t" << double(packet_droped_total + duplicates_total) / double(packet_droped_total + duplicates_total + packet_sent_total)  << " \n";
    std::cout << "[CHAR1] Average Fraction of Lost packets:\t" << (double) total_fraction << " \n";
    std::cout << "[CHAR3] Averege Buffer Queue:\t" << M_buffer << " \n";
    std::cout << "[CHAR3] Max Average Buffer Queue:\t" << Max_M_buffer << " \n";
    std::cout << "[CHAR3] Max Buffer Queue:\t" << max_queue << " \n";
    return 0;
}
