TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lpthread -lboost_system -lboost_thread -lboost_chrono

SOURCES += \
        jsonparser.cpp \
        main.cpp

HEADERS += \
    InternalStruct.hpp \
    ThreadsafeQueue.hpp \
    jsonparser.hpp
