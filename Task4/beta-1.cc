#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <fstream>
#include <string>
#include <iostream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

// константные данные вроде числа клиентов и размера пакета

#define CLIENT_NUM  90// число клиентов
#define SIM_TIME 10.0 // время симуляции
#define CH_DELAY 300 // время на передачу сообщения по каналу
#define alpha 100.0  // альфа, параметр экспоненциального распределения
#define PCK_SIZE 1500 // размер пакета
#define q_max_size "100p" // максимальный размер очереди на хосте

using namespace ns3;
using generator = std::default_random_engine;  // задаем распределение
using distribution = std::exponential_distribution<double>;  // распределение - экспоненциальное

uint64_t packet_sent_total = 0; // общее число отправленных пакетов
uint64_t packet_droped_total = 0; // общее число сброшенных пакетов
uint64_t queue_sum[CLIENT_NUM];
uint64_t pack_send[CLIENT_NUM];
uint64_t max_queue = 0;
uint64_t backofs_total = 0;

static void backof_inc(std::string context, Ptr<const Packet> p) { backofs_total++; }

static void droped_inc(std::string context, Ptr<const Packet> p) { packet_droped_total++; }

NS_LOG_COMPONENT_DEFINE("Client");

class Client : public Application { // модуль приложения
public:
    Client();;
    virtual ~Client();
    void Setup(Ptr <Socket> socket, Address address, generator *gen1, distribution *distr1, Ptr <Queue<Packet>> que1, std::string name1, int num1);
    virtual void StartApplication();  // функция запуска приложения из базового класса Application
    virtual void StopApplication();

    void send_pack(); // отправка пакета
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

void Client::StartApplication() {
    m_running = true;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    m_socket->SetRecvCallback(MakeNullCallback < void, Ptr < Socket >> ());
    m_socket->SetAllowBroadcast(true);
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &Client::send_pack, this);
}

void Client::StopApplication() {
    m_running = false;
    if (m_sendEvent.IsRunning())
        Simulator::Cancel(m_sendEvent);
    if (m_socket)
        m_socket->Close();
}

void Client::send_pack() // отправка пакета и постановка в очередь отправки следующего
{
    Ptr <Packet> msg = Create<Packet>(PCK_SIZE);
    m_socket->Send(msg);  // отправка пакета с использованием заданного сокета
    packet_sent_total++; // увеличиваем общее число отправленных пакетов
    if (m_running) {
        Time Next(MilliSeconds(1000 * (*distr)(*gen)));
        queue_sum[m_num] += que->GetNPackets();
        pack_send[m_num]++;
        if (que->GetNPackets() > max_queue)
            max_queue = que->GetNPackets();
        m_sendEvent = Simulator::Schedule(Next, &Client::send_pack, this);  // планирование
    }
}

Client::~Client() {
    m_socket = 0;
    delete gen;
    delete distr;
}

Client::Client() {
    m_socket = 0;
    m_running = false;
}

void
Client::Setup(Ptr <Socket> socket, Address address, generator *gen1, distribution *distr1, Ptr <Queue<Packet>> que1, std::string name1, int num1) {
    m_socket = socket;
    m_peer = address;
    gen = gen1;
    distr = distr1;
    que = que1;
    name = name1;
    m_num = num1;
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
        que->TraceConnect("Drop", "Host " + std::to_string(i) + ": ", MakeCallback(&droped_inc));
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
    sinkClients.Start(Seconds(1.0));
    sinkClients.Stop(Seconds(1.0 + SIM_TIME));


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
        cl->SetStartTime(Seconds(1.0));
        cl->SetStopTime(Seconds(1.0 + SIM_TIME));
        devices.Get(i)->TraceConnect("MacTxBackoff", "Host " + std::to_string(i) + ": ", MakeCallback(&backof_inc));
    }

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fifth.tr"));

    Simulator::Stop(Seconds(SIM_TIME));
    Simulator::Run();
    Simulator::Destroy();

    double aver_queue = 0;
    for (int i = 0; i < CLIENT_NUM - 1; i++)
        aver_queue += double(queue_sum[i]) / double(pack_send[i]);

    std::cout << "Packets sent:  " << packet_sent_total
              << '\n'  // Без учета переотправленных. То есть это число уникальных пакетов
              << "Packets dropped:  " << packet_droped_total << '\n'
              << "Avg packets lost: " << (float)packet_droped_total / (packet_sent_total - backofs_total) << '\n' // ????????????????????????????
              << "Average queue:   " << aver_queue  << '\n' // в задании сказано, "в буфере адаптера"
              << "Longest queue:       " << max_queue << '\n';
    return 0;
}
