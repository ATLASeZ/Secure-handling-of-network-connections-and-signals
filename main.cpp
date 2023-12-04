#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <signal.h>

#pragma comment(lib, "Ws2_32.lib")

std::mutex m_mutex;                                     // Мьютекс
std::condition_variable m_notification;                 // Уведомление для ожидающиъ потоков о изменении состояния
volatile sig_atomic_t wasSignalHangUp = 0;              // Обработчик сигнала (флаг)

/* Обработка сигнала */
void SignalHangUpHandler(int signal)
{
    /* Когда сигнал будет получен, установим обработчику значение 1 */
    wasSignalHangUp = 1;
}

/* Обработка данных (полученных от клиента через сокет) */
void ConnectionManagment(SOCKET clientSocket)
{
    /* Создаём массив для хранения данных, полученных от клиента */
    char buffer[1024];
    /* Считываем данные из сокета в буфер */
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    /* Проверяем, были ли фактически получены какие либо данные от клиента */
    if (bytesReceived > 0)
    {
        /* Захватываем мьютекс перед выводом в консоль */
        std::lock_guard<std::mutex> lock(m_mutex);
        /* Если данные были получены, выводим количество байт */
        std::cout << "Полученные данные от клиента: " << bytesReceived << " байт" << std::endl;
    }
}

/* Принятие новых соединений от клиентов */
void AcceptConnections(SOCKET serverSocket)
{
    /* Создаём структуру для хранения информации о клиенте, подключившегося к серверу */
    sockaddr_in clientSocketAddress;
    /* Вычисляем размер структуры */
    int clientAddressLength = sizeof(clientSocketAddress);
    /* Блокируем программу до тех пор, пока не будет подключён клиент
       Когда клиент подключается, создаём новый сокет для взаимодействия с этим клиентом */
    SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientSocketAddress, &clientAddressLength);

    /* Проверяем, успешно ли был создан сокет для нового клиента */
    if (clientSocket != INVALID_SOCKET)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            /* Если сокет успешно создан, выводится сообщение о том, что новое соединение было принято */
            std::cout << "Новое соединение принято" << std::endl;

            /* Преобразование IP адреса в формат UNICODE */
            wchar_t ipString[INET6_ADDRSTRLEN];
            InetNtopW(AF_INET, &(clientSocketAddress.sin_addr), ipString, INET6_ADDRSTRLEN);
            /* Выводим IP адрес клиента */
            std::wcout << L"IP адрес клиента: " << ipString << std::endl;

            std::cout << "Порт клиента: " << ntohs(clientSocketAddress.sin_port) << std::endl;
        }
        /* Создаётся новый поток, который будет управлять соединением с клиентом */
        std::thread connectionThread(ConnectionManagment, clientSocket);
        /*  Поток запускается независимо от главного потока и работает параллельно с ним */
        connectionThread.detach();
        /* Сокет закрывается, так как он больше не нужен для принятия новых соединений */
        closesocket(clientSocket);
    }
    else
    {
        /* Вывод сообщения об ошибке в случае неудачной попытки принятия соединения */
        std::cerr << "Ошибка при попытке принять соединение" << std::endl;
    }
}

int main()
{
    setlocale(LC_ALL, "Rus");

    /* Создаём объект, который будет содержать информацию о реализации сокетов */
    WSADATA wsaData;
    /* Запускаем в работу сокеты */
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    /* Проверяем ошибки при инициализации библиотеки сокетов */
    if (result != 0)
    {
        /* Выводим сообщение об ошибке */
        std::cerr << "Не удалось выполнить WSAStartup" << std::endl;

        return 1;
    }

    /* Создаём серверный сокет */
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /* Проверяем ошибки при создании серверного сокета */
    if (serverSocket == INVALID_SOCKET)
    {
        /* Выводим сообщение об ошибке */
        std::cerr << "Ошибка при создании сокета" << std::endl;
        /* Очищаем сокет */
        WSACleanup();

        return 1;
    }

    /* Создаём структуру для представления адреса сервера */
    sockaddr_in serverAddress;
    /* Семейство адресов */
    serverAddress.sin_family = AF_INET;
    /* Устанавливаем порт для прослушивания */
    serverAddress.sin_port = htons(12345);
    /* Устанавливаем адрес для прослушивания */
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    /* Проверяем ошибки при привязывании адреса к серверному сокету */
    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR || listen(serverSocket, 5) == SOCKET_ERROR)
    {
        /* Выводим сообщение об ошибке */
        std::cerr << "Ошибка привязки сокета" << std::endl;
        /* Закрываем сокет */
        closesocket(serverSocket);
        /* Очищаем сокет */
        WSACleanup();

        return 1;
    }

    /* Проверяем ошибки при прослушивании сокета */
    if (listen(serverSocket, 5) == SOCKET_ERROR)
    {
        /* Выводим сообщение об ошибке */
        std::cerr << "Ошибка прослушивания сокета" << std::endl;
        /* Закрываем сокет */
        closesocket(serverSocket);
        /* Очищаем сокет */
        WSACleanup();

        return 1;
    }

    /* Устанавливаем обработчик сигналов */
    signal(SIGTERM, SignalHangUpHandler);

    /*  */
    while (true)
    {
        /* Создаём набор файловых дескрипторов для хранения сокетов, готовых для чтения */
        fd_set fds;
        /* Очищаем все биты в наборе */
        FD_ZERO(&fds);
        /* Добавляем serverSocket в набор */
        FD_SET(serverSocket, &fds);

        /* Проверяем флаг (если флаг установлен) */
        if (wasSignalHangUp)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            /* Выводим сообщение о принятии сигнала */
            std::cout << "Сигнал принят" << std::endl;
            /* Сбрасываем флаг */
            wasSignalHangUp = 0;
        }

        FD_SET(serverSocket, &fds);

        /* Проверяем наличие событий ввода/вывода на сокете */
        if (select(serverSocket + 1, &fds, NULL, NULL, NULL) == -1)
        {
            /* Если программа вернула ошибку, получаем код этой ошибки */
            int error = WSAGetLastError();

            /* Операция была прервана сигналом, и программа может продолжить ожидание событий ввода/вывода */
            if (error == WSAEINTR)
            {
                continue;
            }
            else
            {
                /* Выводим сообщение об ошибке */
                std::cerr << "Ошибка при выборе: " << error << std::endl;

                break;
            }
        }

        /* Вызывается функция для принятия входящих соединений */
        AcceptConnections(serverSocket);
    }

    /* Закрываем сокет */
    closesocket(serverSocket);
    /* Очищаем память */
    WSACleanup();

    return 0;
}