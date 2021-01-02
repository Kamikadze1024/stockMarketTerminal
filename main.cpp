#include <iostream>
#include <memory>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <fstream>
#include <signal.h>
#include "jsonparser.hpp"
#include "ThreadsafeQueue.hpp"
#include "InternalStruct.hpp"
#include <iostream>

//константы
static const std::string CONST_KEY_REVERSE        = "-r";
static const std::string CONST_KEY_FILENAME       = "-f";
static const std::string CONST_KEY_SAVE_LEN       = "-l";
static const std::string CONST_DEFAULT_FILE_NAME  = "defaultFileName";
static const int         CONST_DEFAULT_SAVE_LEN   = 10;
static const int         CONST_THD_SLP_PERIOD_MS  = 15; //период сна потоков в милисекундах

//глобалы
static std::string    INPUT_FILE_NAME; //имя файла с json ами
static bool           REVERSE_FLAG      = false;//флаг реверсивного отображения
static bool           THREAD_ACTIVE_FLG = true; //флаг работоспособности потоков
static int            SAVE_LEN          = 0;//глубина хранения
static InternalStruct InternalStruct; //хранилище данных
static bool           NEED_SHOW_REZ     = false; //надо показать результат работы программы


//функция перехвата CTRL + C
void ctrlcDesc (int sig) {
    //остановить потоки
    THREAD_ACTIVE_FLG = false;
    std::cout << "Ctrl + C found" << std::endl;
    sig = 0;
}

//функция перехвата sigusr1
void sigusr1Hdl(int sig) {
    std::cout << "SIGUSR1 HANDLED" << std::endl;

    //защита от "закидывания сигналами"
    if(!NEED_SHOW_REZ) {
        NEED_SHOW_REZ = true;
    }
    sig = 0;
}

//задержка потока, вызвавшего wait
void wait(int seconds) {
  boost::this_thread::sleep_for(boost::chrono::seconds{seconds});
}


//поток разбора пришедших сообщений
void receivedMsgsParser(boost::shared_ptr<ThreadsafeQueue<std::string>>
                        inpQueue) {

        /*
         * Усыпить поток апдейта структуры InternalStruct
         */
        boost::chrono::milliseconds period(CONST_THD_SLP_PERIOD_MS);
        boost::this_thread::sleep_for(period);
}

//распечатать значения глобалов
void printGlobals() {
    std::cout << "INPUT_FILE_NAME = "
              << INPUT_FILE_NAME << std::endl; //имя файла с json ами
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

    //разбор аргументов командной строки
    for(int count = 0; count < argc; count++) {
        std::string readArg(argv[count]);
        if(readArg.compare(CONST_KEY_REVERSE) == 0) {
            //отображать в обратном порядке
            REVERSE_FLAG = true;
        }

        if(readArg.compare(CONST_KEY_FILENAME) == 0) {
            if((count + 1) < argc) {
                //сохранить имя файла с json ами
                std::string       inpFn(argv[count + 1]);
                INPUT_FILE_NAME = inpFn;
            } else {
                std::cout << "ATTENTION: Используется имя файла по-умолчанию"
                          << std::endl;
                INPUT_FILE_NAME = CONST_DEFAULT_FILE_NAME;
            }
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

    //если параметр -f вообще не задан
    if(INPUT_FILE_NAME.compare("") == 0) {
        std::cout << "ATTENTION: Используется имя файла по-умолчанию"
                  << std::endl;
        INPUT_FILE_NAME = CONST_DEFAULT_FILE_NAME;
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
    }

    /*
     * отобразить итоговый результат работы программы
     * по сигналу SIGUSR1
     */
    std::string usr1Str = InternalStruct.getResString(REVERSE_FLAG);
    //вывод результата работы программы на экран
    std::cout << usr1Str << std::endl;

    /*
     * потокобезопасная очередь получаемых извне
     * апдейтов ask и bid
     */
    boost::shared_ptr<ThreadsafeQueue<std::string> >
            inputQueue(new ThreadsafeQueue<std::string>());

    //запуск потока, парсящего полученные строки
    boost::thread parseReceivedMsgsThd(receivedMsgsParser,
                                       inputQueue);

    parseReceivedMsgsThd.join();

    return 0;
}
