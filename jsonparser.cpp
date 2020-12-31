#include "jsonparser.hpp"
#include <iostream>
#include <boost/foreach.hpp>
#include <utility>
#include <sstream>


JsonParser::JsonParser(std::string &s) {
    m_s = s;

    m_askIsFirst = false;
}

std::vector<std::pair<std::string, std::string> > JsonParser::getValPairs(
        boost::property_tree::ptree& pt) {

    std::vector<std::pair<std::string, std::string>> res;

    boost::property_tree::ptree::const_iterator end = pt.end();
    boost::property_tree::ptree::const_iterator it;
    for(it = pt.begin(); it != end; ++it) {
        //получение пар price, amount
        boost::property_tree::ptree::const_iterator endArr = it->second.end();
        boost::property_tree::ptree::const_iterator itPair;
        //счетчик первый-второй. Для того, чтобы было понятно кто price, кто amount
        int i = 0;

        std::string price  = "";
        std::string amount = "";
        for(itPair = it->second.begin(), i = 0; itPair != endArr; ++itPair, i++) {
            std::string s = itPair->second.get_value<std::string>();

            //price
            if(i == 0) {
                price = s;
            } else if(i == 1) {
                //amount
                amount = s;
            }
        }
        //создаем пару price, amount
        std::pair<std::string, std::string> priceAmount(price, amount);
        res.push_back(priceAmount);
    }
    return res;
}

std::string JsonParser::getFieldValue(boost::property_tree::ptree const& pt,
                                      std::string field) {
    boost::property_tree::ptree::const_iterator end = pt.end();
    boost::property_tree::ptree::const_iterator it;

    std::string res;
    for(it = pt.begin(); it != end; ++it) {
        if(it->first.compare(field) == 0) {
            res = it->second.get_value<std::string>();
            break;
        }
    }

    return res;
}

//вытащить U и u, чтобы узнать, нужно ли парсить дальше
bool JsonParser::preParse() {
    /*
     * Парсинг json
     */
    std::istringstream iss(m_s);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(iss, pt);

    if(pt.empty()) {
        std::cout << "Error preparsing" << std::endl;
        return false;
    }

    //поддерево data
    boost::property_tree::ptree ptData = pt.get_child("data");


    //получаем U
    m_U = getFieldValue(ptData, "U");

    //получаем u
    m_u = getFieldValue(ptData, "u");

    return true;
}

bool JsonParser::parse() {
    /*
     * Поиск "что первично?"
     * ask или bid?
     */
    std::string askPattern = "\"a\":";
    size_t askPos = m_s.find(askPattern);
    std::string bidPattern = "\"b\":";
    size_t bidPos = m_s.find(bidPattern);

    if(askPos <= bidPos) {
        m_askIsFirst = true;
    } else {
        m_askIsFirst = false;
    }

    /*
     * Парсинг json
     */
    std::istringstream iss(m_s);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(iss, pt);

    if(pt.empty()) {
        std::cout << "Error parsing" << std::endl;
        return false;
    }

    //поддерево data
    boost::property_tree::ptree ptData = pt.get_child("data");

    //получаем timestamp из поля E json
    m_timestamp = getFieldValue(ptData, "E");

    //поддерево b
    boost::property_tree::ptree ptB    = ptData.get_child("b");
    //поддерево a
    boost::property_tree::ptree ptA    = ptData.get_child("a");

    //добавляю asks
    std::vector<std::pair<std::string, std::string>> a = getValPairs(ptA);
    if(!a.empty()) {
        m_asks.insert(m_asks.end(), a.begin(), a.end());
    }

    //добавляю bids
    std::vector<std::pair<std::string, std::string>> b = getValPairs(ptB);
    if(!b.empty()) {
        m_bids.insert(m_bids.end(), b.begin(), b.end());
    }

    //сбой в парсинге
    if(a.empty() && b.empty()) {
        return false;
    }

    return true;
}

//вернуть asks и bids
std::vector<std::pair<std::string, std::string>> JsonParser
::getAsks() {
    return m_asks;
}

std::vector<std::pair<std::string, std::string>> JsonParser
::getBids() {
    return m_bids;
}

//вернуть timestamp
std::string JsonParser::getTimestamp() {
    return m_timestamp;
}

bool JsonParser::isAskFirst() {
    return m_askIsFirst;
}

//вернуть U
std::string JsonParser::getU() {
    return m_U;
}

//вернуть u
std::string JsonParser::getLittleU() {
    return m_u;
}
