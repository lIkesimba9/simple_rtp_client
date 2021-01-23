#include "log.h"

template <class T>
log<T>::log() {}
template <class T>
void log<T>::setFileName(string filename)
{
    out.open(filename);
}

template <class T>
void log<T>::setHeader(vector<string> headers)
{
    for (int i = 0; i < headers.size()-1; i++)
        out << headers[i] << ",";
    out << headers[headers.size()-1];

}
template <class T>
void log<T>::logging(vector<T> data)
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

