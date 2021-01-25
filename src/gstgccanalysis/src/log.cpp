#include "log.h"

template <class T>
log<T>::log() {}
template <class T>
void log<T>::setFileName(std::string filename)
{
    out.open(filename);
}

template <class T>
void log<T>::setHeader(std::vector<std::string> headers)
{
    for (int i = 0; i < headers.size()-1; i++)
        out << headers[i] << ",";
    out << headers[headers.size()-1];

}
template <class T>
void log<T>::logging(std::vector<T> data)
{
    for (int i = 0; i < data.size()-1; i++)
        out << data[i] << ",";
    out << data[data.size()-1];
}
template <class T>
log<T>::~log()
{
    out.close();
}

