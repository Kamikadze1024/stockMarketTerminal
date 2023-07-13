/*
 * Внутренняя структура данных
 * Потокозащищена
 * Кашицын Денис, 2020
 */
#ifndef INTERNALSTRUCT_HPP
#define INTERNALSTRUCT_HPP

#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <map>


class InternalStruct {
private:
    int                                 m_saveLen;
    mutable std::mutex                  m_mut;

    std::string                         m_currTimestamp;
    std::map<std::string, std::string>  m_asks;
    std::map<std::string, std::string>  m_bids;
    bool                                m_askFirst;
    unsigned long int                   m_lastUpdateId;
    unsigned long int                   m_littleU;

    //константы формирования ответа
    const std::string START_A = "{\"a\":";
    const std::string START_B = "{\"b\":";
    const std::string A_SECT  = "\"a\":";
    const std::string B_SECT  = "\"b\":";
    const std::string OP_KV   = "[";
    const std::string CL_KV   = "]";
    const std::string CLOSE_F = "}";
    const std::string ZERO_S  = "0.00000000";//нулевое значение

    const std::string ERR_STRING = "ERROR in internal struct";

public:
    InternalStruct() {
        /*
         * По этому адресу
         * https://api.binance.com/api/v3/depth?symbol=BNBBTC&limit=1000
         * он разный на каждом запросе
         * Я подогнал его под набор данных
         */
        m_lastUpdateId = 6511117588;

        m_littleU      = 0;
    }

    //инит хранилища
    void init(int saveLen) { m_saveLen = saveLen; }

    //занести данные
    void update(std::string timestamp,
                std::vector<std::pair<std::string, std::string>> asks,
                std::vector<std::pair<std::string, std::string>> bids,
                bool                                             askFirst) {

        m_currTimestamp = timestamp;
        m_askFirst      = askFirst;

        /*
         * Уровни цен будут отсортированы по-возрастанию автоматически
         * ибо так реализован std::map
         */
        //asks
        for(auto it = asks.begin(); it != asks.end(); it++) {
            auto firstIt = m_asks.find(it->first);
            //нет такого ключа
            if(firstIt == m_asks.end()) {
                //вставить пару если справа не ноль
                if(it->second.compare(ZERO_S) != 0) {
                    m_asks.insert({it->first, it->second});
                }
            } else if (firstIt != m_asks.end()) { //такой ключ есть
                //пришло указание для удаления уровня цены
                if(it->second.compare(ZERO_S) == 0) {
                    m_asks.erase(firstIt);
                }
            }
        }

        //bids
        for(auto it = bids.begin(); it != bids.end(); it++) {
            auto firstIt = m_bids.find(it->first);
            //нет такого ключа
            if(firstIt == m_bids.end()) {
                //вставить пару если справа не ноль
                if(it->second.compare(ZERO_S) != 0) {
                    m_bids.insert({it->first, it->second});
                }
            } else if (firstIt != m_bids.end()) { //такой ключ есть
                //пришло указание для удаления уровня цены
                if(it->second.compare(ZERO_S) == 0) {
                    m_bids.erase(firstIt);
                }
            }
        }

        /*
         * Отображаю m_saveLen
         * верхних значений цены
         * для формирования стакана
         */
         //количество элементов на удаление из asks
         if(m_asks.size() > (unsigned long)m_saveLen) {
             int forRm = m_asks.size() - (unsigned long)m_saveLen;
             //m_asks.erase(m_asks.begin() + 1, m_asks.begin() + forRm + 1);
             auto i = m_asks.begin();
             while((i != m_asks.end()) && (forRm > 0)) {
                 i = m_asks.erase(i);
                 forRm--;
             }
         }

         //количество элементов на удаление из bid
         if(m_bids.size() > (unsigned long)m_saveLen) {
             int forRm = m_bids.size() - (unsigned long)m_saveLen;
             //m_bids.erase(m_bids.begin() + 1, m_bids.begin() + forRm + 1);
             auto i = m_bids.begin();
             while((i != m_bids.end()) && (forRm > 0)) {
                 i = m_bids.erase(i);
                 forRm--;
             }
         }
    }

    //direct - отображать в прямом или обратном порядках
    std::string getResString(bool reverse) {
        std::unique_lock<std::mutex> lk(m_mut);

        /*
         * В структуре, поля отсортированы
         * по возрастанию.
         * Направление отображения зависит
         * от ключа -r и того, что пришло
         * первым a или b. Это отношение
         * эквивалентности.
         */
        bool direction = false;
        if((!m_askFirst) && (!reverse)) {
            direction = true;
        } else if((!m_askFirst) && (reverse)) {
            direction = false;
        } else if((m_askFirst) && (!reverse)) {
            direction = false;
        } else if((m_askFirst) && (reverse)) {
            direction = true;
        }

        //в контейнере у нас пусто
        if((m_asks.empty()) && (m_bids.empty())) {
            return ERR_STRING;
        }

        //поток для А
        std::stringstream ssA;

        //открывает блок А
        ssA << OP_KV;
        if(direction) {
            unsigned long i = 0;
            for(auto it = m_asks.begin(); it != m_asks.end(); it++) {
                ssA << OP_KV;
                ssA << it->first << ", ";//price
                ssA << it->second; //amount
                ssA << CL_KV;

                //ставим запятую если надо
                if((i + 1) < m_asks.size()) {
                    ssA << ",";
                }
                i++;
            }
        } else {
            int i = (int)m_asks.size() - 1;
            for(auto it = m_asks.rbegin(); it != m_asks.rend(); ++it, i--) {
                ssA << OP_KV;
                ssA << it->first << ", ";//price
                ssA << it->second; //amount
                ssA << CL_KV;

                //ставим запятую если надо
                if((i - 1) > 0) {
                    //std::cout << "i - 1 = " << (i - 1) << std::endl;
                    ssA << ",";
                }
            }
        }
        //закрывает блок а
        ssA << CL_KV;

        //поток для B
        std::stringstream ssB;

        //открывает блок B
        ssB << OP_KV;

        if(direction) {
            unsigned long i = 0;
            for(auto it = m_bids.begin(); it != m_bids.end(); ++it) {
                ssB << OP_KV;
                ssB << it->first << ", ";//price
                ssB << it->second; //amount
                ssB << CL_KV;

                //ставим запятую если надо
                if((i + 1) < m_bids.size()) {
                    ssB << ",";
                }
                i++;
            }
        } else {
            int i = (int)m_bids.size() - 1;
            for(auto it = m_bids.rbegin(); it != m_bids.rend(); ++it, i--) {
                ssB << OP_KV;
                ssB << it->first << ", ";//price
                ssB << it->second; //amount
                ssB << CL_KV;

                //ставим запятую если надо
                if((i - 1) > 0) {
                    //std::cout << "i - 1 = " << (i - 1) << std::endl;
                    ssB << ",";
                }
            }
        }
        //закрывает блок b
        ssB << CL_KV;

        //собираем итоговую строку
        std::stringstream ss;

        //показываем только bids
        if((m_asks.empty()) && (!m_bids.empty())) {
            ss << START_B;
            std::string bList = ssB.str();
            ss << bList;
            ss << CLOSE_F;
        } else if((m_bids.empty()) && (!m_asks.empty())) {
            //показываем только asks
            ss << START_A;
            std::string aList = "";
            aList = ssA.str();
            ss << aList;
            ss << CLOSE_F;
        } else {
            //показываем оба
            ss << START_A;
            std::string aList = "";
            aList = ssA.str();
            ss << aList;
            ss << ", ";
            ss << B_SECT;
            std::string bList = "";
            bList = ssB.str();
            ss << bList;
            ss << CLOSE_F;
        }

        std::string res;
        res = ss.str();
        return res;
    }

    //нужно "дропнуть" пакет?
    bool needDropEvent(std::string                                      U,
                       std::string                                      u) {
        std::lock_guard<std::mutex> lk(m_mut);
        std::stringstream ssu(u);
        unsigned long uL = 0;
        ssu >> uL;
        if(uL <= m_lastUpdateId) {
            std::cout << "Drop event " << m_lastUpdateId
                      << " " << uL << std::endl;
            return true;
        }

        std::stringstream ssU(U);
        unsigned long UL = 0;
        ssU >> UL;

        /*
         * проверка условия, что U текущего элемента должно
         * быть u прошлого + 1
         */
        //первая инициализация
        if(m_littleU == 0) {
            m_littleU = uL;
        } else if(UL < (m_littleU + 1)) {
            //все последующие
            /*
             * Не выполняется условие:
             * U текущего должно быть равно u+1 предыдущего эвента
             */
            std::cout << "Little U error" << std::endl;
            return true;
        }

        /*
         * Опускаем, что первый эвент потока должен
         * удовлетворять условию
         * U <= lastUpdateId+1 AND u >= lastUpdateId+1.
         */
        /*if((UL <= (m_lastUpdateId + 1)) && (uL >= (m_lastUpdateId + 1))) {
            std::cout << "U <= lastUpdateId+1 AND u >= lastUpdateId+1."
                      << std::endl;
            return false;
        } else {
            std::cout << "NOT U <= lastUpdateId+1 AND u >= lastUpdateId+1."
                      << std::endl;
            return true;
        }*/

        m_littleU = uL;
        return false;
    }
};

#endif // INTERNALSTRUCT_HPP
