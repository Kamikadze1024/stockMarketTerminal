/*
 * Parser json
 * Отрезает лишнее от строки
 * и возвращает пары
 * ask/bid из строки в
 * формате <std::string price, std::string amount>
 * для ask и для bid
 * Кашицын Денис, 2020
 */

#ifndef JSONPARSER_HPP
#define JSONPARSER_HPP

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>

class JsonParser {
    //строка
    std::string m_s;

    //timestamp
    std::string m_timestamp;

    //U
    std::string m_U;

    //u
    std::string m_u;

    //asks
    std::vector<std::pair<std::string, std::string>> m_asks;

    //bids
    std::vector<std::pair<std::string, std::string>> m_bids;

    /*
     * метод выделения пар price, amount
     * из поддеревьев ask, bid
     */
    std::vector<std::pair<std::string, std::string> > getValPairs(
            boost::property_tree::ptree& pt);

    //получить значение поля
    std::string getFieldValue(boost::property_tree::ptree const& pt,
                  std::string field);

    //получили первым ask?
    bool m_askIsFirst;
public:
    JsonParser(std::string &s);

    //распарсить полностью
    bool parse();

    //вытащить U и u, чтобы узнать, нужно ли парсить дальше
    bool preParse();

    //вернуть asks и bids
    std::vector<std::pair<std::string, std::string>> getAsks();
    std::vector<std::pair<std::string, std::string>> getBids();

    //вернуть timestamp
    std::string getTimestamp();

    //пришел ли ask первым?
    bool isAskFirst();

    //вернуть U
    std::string getU();

    //вернуть u
    std::string getLittleU();
};

#endif // JSONPARSER_HPP
