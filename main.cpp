#include <iostream>
#include <memory>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <fstream>
#include <signal.h>
#include <iostream>
#include <atomic>
#include "jsonparser.hpp"
#include "InternalStruct.hpp"

//константы
static const std::string CONST_KEY_REVERSE        = "-r";
static const std::string CONST_KEY_SAVE_LEN       = "-l";
static const int         CONST_DEFAULT_SAVE_LEN   = 10;
static const int         CONST_THD_SLP_PER_MS     = 1000;
static const int         CONST_FAILED_RC          = 2;

//глобалы
static bool                  REVERSE_FLAG      = false;//флаг реверсивного отображения
static int                   SAVE_LEN          = 0;//глубина хранения
static InternalStruct        InternalStruct; //хранилище данных
static std::atomic<bool>     thdFlag; //флаг работы потока

//функция перехвата CTRL + C
void ctrlcDesc (int sig) {
    /*
     * Запрещаю прерывать выполнение завершения
     */
    sigset_t newMask;
    sigset_t oldMask;

    if(sigfillset(&newMask) != 0) {
        std::cout << "Failed to sigfillset" << std::endl;
        exit(CONST_FAILED_RC);
    }

    if(sigprocmask(SIG_SETMASK, &newMask, &oldMask) != 0) {
        std::cout << "Failed to mask signals" << std::endl;
        exit(CONST_FAILED_RC);
    }

    thdFlag.store(false);
    std::cout << "Ctrl + C found" << std::endl;

    //разрешить получение сигналов
    if(sigprocmask(SIG_SETMASK, &oldMask, NULL) != 0) {
        std::cout << "Failed to umask signals" << std::endl;
        exit(CONST_FAILED_RC);
    }
    sig = 0;
}

//функция перехвата sigusr1
void sigusr1Hdl(int sig) {
    /*
     * Запрещаю прерывать выполнение вывода
     */
    sigset_t newMask;
    sigset_t oldMask;

    if(sigfillset(&newMask) != 0) {
        std::cout << "Failed to sigfillset" << std::endl;
        exit(CONST_FAILED_RC);
    }

    if(sigprocmask(SIG_SETMASK, &newMask, &oldMask) != 0) {
        std::cout << "Failed to mask signals" << std::endl;
        exit(CONST_FAILED_RC);
    }

    /*
     * отобразить итоговый результат работы программы
     * по сигналу SIGUSR1
     */
    std::string usr1Str = InternalStruct.getResString(REVERSE_FLAG);
    //вывод результата работы программы на экран
    std::cout << usr1Str << std::endl;

    //разрешить получение сигналов
    if(sigprocmask(SIG_SETMASK, &oldMask, NULL) != 0) {
        std::cout << "Failed to umask signals" << std::endl;
        exit(CONST_FAILED_RC);
    }
}


/*
 * Поток, который оставляет программу работающей, когда
 * все строки из stdin уже получены
 */
void programmAliveThdFunc() {
    while(thdFlag) {
        /*
         * Усыпить поток апдейта структуры InternalStruct
         */
        boost::chrono::milliseconds period(CONST_THD_SLP_PER_MS);
        boost::this_thread::sleep_for(period);
    }
}

//распечатать значения глобалов
void printGlobals() {
    std::cout << "REVERSE_FLAG    = "
              << REVERSE_FLAG << std::endl;    //флаг реверсивного отображения
    std::cout << "SAVE_LEN        = "
              << SAVE_LEN << std::endl;        //глубина хранения
}



int main(int argc, char *argv[]) {
    //регистрация обработчика ctrl + c
    signal(SIGINT, ctrlcDesc);

    //регистрация обработчика SIGUSR1
    signal(SIGUSR1, sigusr1Hdl);

    //разрешение потоку поддержки работы работать
    thdFlag.store(true);

    //разбор аргументов командной строки
    for(int count = 0; count < argc; count++) {
        std::string readArg(argv[count]);
        if(readArg.compare(CONST_KEY_REVERSE) == 0) {
            //отображать в обратном порядке
            REVERSE_FLAG = true;
        }

        //нестандартная глубина хранения
        if(readArg.compare(CONST_KEY_SAVE_LEN) == 0) {
            if((count + 1) < argc) {
                std::string saveLen(argv[count + 1]);
                std::stringstream ss(saveLen);
                ss >> SAVE_LEN;
            } else {
                std::cout << "ATTENTION: Используется глубина хранения по-умолчанию"
                          << std::endl;
                SAVE_LEN = CONST_DEFAULT_SAVE_LEN;
            }
        }
    }

    //если параметр -l вообще не задан
    if(SAVE_LEN == 0) {
        std::cout << "ATTENTION: Используется глубина хранения по-умолчанию"
                  << std::endl;
        SAVE_LEN = CONST_DEFAULT_SAVE_LEN;
    }

    /*
     * вывести на экран значения глобалов,
     * которые получены из аргументов
     */
    printGlobals();

    /*
     * Инициализация хранилища
     */
    InternalStruct.init(SAVE_LEN);

    /*
     * Однопоточное получение строки из
     * stdin, и немедленная ее обработка
     */
    std::string msg;
    bool askFirst = false;
    while (std::getline(std::cin, msg)) {

        //если "пойман" ctrl + c - завершиться
        if(!thdFlag.load()) {
            std::cout << "Ctrl + c получен, завершаюсь" << std::endl;
            exit(0);
        }

        JsonParser jp(msg);

        //препарсинг, для "вытаскивания" U и u
        if(!jp.preParse()) {
            std::cout << "PreParsing failed" << std::endl;
            continue;
        }

        std::string U = jp.getU();
        std::string u = jp.getLittleU();

        if(InternalStruct.needDropEvent(U, u)) {
            std::cout << "Event is dropping" << std::endl;
            continue;
        }

        if(!jp.parse()) {
            std::cout << "Parsing failed" << std::endl;
            continue;
        }

        std::vector<std::pair<std::string, std::string>> bids
                = jp.getBids();

        std::vector<std::pair<std::string, std::string>> asks
                = jp.getAsks();

        std::string timestamp = jp.getTimestamp();

        askFirst = jp.isAskFirst();

        InternalStruct.update(timestamp, asks, bids, askFirst);

        /*
         * Чтобы наблюдать изменения биржевого стакана в динамике,
         * раскомментировать.
         */
        //boost::chrono::milliseconds period(CONST_THD_SLP_PER_MS);
        //boost::this_thread::sleep_for(period);
    }

    /*
     * Поток, который оставляет программу работающей, когда
     * все строки из stdin уже получены
     */
    boost::thread programmAliveThd(&programmAliveThdFunc);

    // программе дождаться окончания работы поддерживающего потока
    programmAliveThd.join();

    return 0;
}
